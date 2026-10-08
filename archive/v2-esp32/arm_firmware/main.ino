/*
 * ============================================================
 * PROJET ARACHNAE v2 — GOD-MODE FIRMWARE (HOTSPOT EDITION)
 * Board: Seeed Studio XIAO ESP32S3 Sense
 * Network: Station Mode (STA) connecting to Mobile Hotspot
 * * Architecture:
 * - CORE 0: Handles Wi-Fi, HTTP Server, and OV2640 MJPEG Streaming
 * - CORE 1: Handles WebSockets, IK Math, Safety Monitor, and Servos
 * ============================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include "esp_http_server.h"

// ── Include your custom headers ──────────────────────────────
#include "config.h"
#include "kinematics.h"
#include "servo_control.h"
#include "teach_mode.h"
#include "teach_server.h"

// ── WiFi Config (Station Mode connecting to Hotspot) ─────────
#define WIFI_AP_MODE  false
static const char* STA_SSID = "YOUR_HOTSPOT";
static const char* STA_PASS = "YOUR_PASSWORD";

// ── WebSocket server (port 8765) ─────────────────────────────
WebSocketsServer g_wsServer(8765);

// ── XIAO ESP32S3 Sense OV2640 Pin Map ────────────────────────
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    10
#define SIOD_GPIO_NUM    40
#define SIOC_GPIO_NUM    39
#define Y9_GPIO_NUM      48
#define Y8_GPIO_NUM      11
#define Y7_GPIO_NUM      12
#define Y6_GPIO_NUM      14
#define Y5_GPIO_NUM      16
#define Y4_GPIO_NUM      18
#define Y3_GPIO_NUM      17
#define Y2_GPIO_NUM      15
#define VSYNC_GPIO_NUM   38
#define HREF_GPIO_NUM    47
#define PCLK_GPIO_NUM    13
#define LED_PIN          21  // Active LOW on XIAO

// ── Camera Stream Parameters ─────────────────────────────────
#define STREAM_FRAMESIZE FRAMESIZE_VGA  
#define STREAM_QUALITY   15             
#define STREAM_FB_COUNT  2              

#define PART_BOUNDARY "frame"
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
static uint32_t frame_count = 0;

// ── Arm State Globals ─────────────────────────────────────────
static ArmState   g_arm;
static TargetPose g_cvTarget;
static TargetPose g_activeTarget;
static bool       g_targetFresh = false;
static bool       g_autoMode    = false;
static uint32_t   g_targetAgeMs = 0;
static bool       g_gripped     = false;

static constexpr uint32_t TARGET_STALE_MS = 3000;

// ============================================================
//  CAMERA HTTP HANDLERS (Running on Core 0 via ESP-IDF)
// ============================================================
static esp_err_t stream_handler(httpd_req_t* req) {
  camera_fb_t* fb  = NULL;
  esp_err_t    res = ESP_OK;
  char         part_buf[64];

  res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "15");
  digitalWrite(LED_PIN, LOW);   // LED on while streaming

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) { res = ESP_FAIL; break; }

    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
    }
    esp_camera_fb_return(fb);
    frame_count++;
    if (res != ESP_OK) break;
  }
  digitalWrite(LED_PIN, HIGH);  // LED off
  return res;
}

void startCameraServer() {
  httpd_config_t config  = HTTPD_DEFAULT_CONFIG();
  config.server_port     = 81;   // <-- Port 81 so it doesn't fight Teach UI
  config.max_uri_handlers = 8;
  config.core_id         = 0; 

  httpd_uri_t stream_uri = { "/stream", HTTP_GET, stream_handler, NULL };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    Serial.println(F("[CAM] HTTP Stream server started on Port 81"));
  } else {
    Serial.println(F("[CAM] Server start FAILED"));
  }
}

// ============================================================
//  ARM KINEMATICS & FSM LOGIC (Running on Core 1)
// ============================================================
static void tickServers() {
  teachServerLoop();
  g_wsServer.loop();
  runSafetyMonitor();   
}

static bool waitMotion(uint32_t timeout_ms) {
  return waitForMotionComplete(timeout_ms, 8, tickServers);
}

static float adaptivePitch(float z_mm) {
  if (z_mm < -50.0f)  return TOOL_PITCH_LOW_DEG;
  if (z_mm < 150.0f)  return TOOL_PITCH_MID_DEG;
  return TOOL_PITCH_HIGH_DEG;
}

static bool moveToXYZ(float x, float y, float z, uint16_t speed, uint32_t timeout_ms) {
  if (g_faultActive) return false;
  const float pitches[] = { adaptivePitch(z), TOOL_PITCH_LOW_DEG, TOOL_PITCH_MID_DEG, TOOL_PITCH_HIGH_DEG };
  
  for (float p : pitches) {
    JointAngles angles;
    IKResult res = solveIK(x, y, z, angles, p * (M_PI / 180.0f));
    if (res == IKResult::OK) {
      armMoveToAngles(angles, speed);
      g_arm.current = angles;
      return waitMotion(timeout_ms);
    }
  }
  Serial.printf("[IK] No solution for (%.0f, %.0f, %.0f)\n", x, y, z);
  return false;
}

static uint32_t g_fsmEnterMs = 0;

static void enterState(GrabFSM s) {
  g_arm.fsm    = s;
  g_fsmEnterMs = millis();
  static const char* names[] = { "IDLE","SCANNING","APPROACH","DESCEND","GRIP","LIFT","HANDOFF","HOME" };
  Serial.printf("[FSM] → %s\n", names[(uint8_t)s]);
}

static void runFSM() {
  if (g_faultActive) return;

  switch (g_arm.fsm) {
    case GrabFSM::IDLE:
      if (g_autoMode) enterState(GrabFSM::SCANNING);
      break;

    case GrabFSM::SCANNING:
      if (!g_autoMode) { enterState(GrabFSM::IDLE); break; }
      if (g_targetFresh && millis() - g_targetAgeMs < TARGET_STALE_MS &&
          g_cvTarget.confidence >= CV_CONFIDENCE_THRESHOLD) {
        g_activeTarget = g_cvTarget;
        g_targetFresh  = false;
        enterState(GrabFSM::APPROACH);
      }
      break;

    case GrabFSM::APPROACH: {
      gripperOpen();
      float az = g_activeTarget.z_mm + APPROACH_CLEARANCE_MM;
      bool ok = moveToXYZ(g_activeTarget.x_mm, g_activeTarget.y_mm, az, APPROACH_SPEED, MOTION_TIMEOUT_MS);
      enterState(ok ? GrabFSM::DESCEND : GrabFSM::SCANNING);
      break;
    }

    case GrabFSM::DESCEND: {
      bool ok = moveToXYZ(g_activeTarget.x_mm, g_activeTarget.y_mm, g_activeTarget.z_mm, DESCEND_SPEED, MOTION_TIMEOUT_MS);
      if (!ok) { gripperOpen(); enterState(GrabFSM::HOME); break; }
      enterState(GrabFSM::GRIP);
      break;
    }

    case GrabFSM::GRIP:
      gripperCloseToContact(200);
      delay(300);
      g_gripped = (gripperLoad() > 150);
      enterState(GrabFSM::LIFT);
      break;

    case GrabFSM::LIFT: {
      float lz = g_activeTarget.z_mm + LIFT_HEIGHT_MM;
      moveToXYZ(g_activeTarget.x_mm, g_activeTarget.y_mm, lz, LIFT_SPEED, MOTION_TIMEOUT_MS);
      enterState(GrabFSM::HANDOFF);
      break;
    }

    case GrabFSM::HANDOFF: {
      float pitch_rad = HANDOFF_TOOL_PITCH_DEG * (M_PI / 180.0f);
      JointAngles angles;
      if (solveIK(HANDOFF_X_MM, HANDOFF_Y_MM, HANDOFF_Z_MM, angles, pitch_rad) == IKResult::OK) {
        armMoveToAngles(angles, HANDOFF_SPEED);
        g_arm.current = angles;
        waitMotion(MOTION_TIMEOUT_MS);
        uint32_t t0 = millis();
        while (millis() - t0 < 6000 && !g_faultActive) {
          if (gripperLoad() < 50) { delay(400); gripperClose(); break; }
          tickServers(); delay(50);
        }
      } else { gripperOpen(); }
      enterState(GrabFSM::HOME);
      break;
    }

    case GrabFSM::HOME:
      if (g_taught.calibrated) {
        moveToTaught("home", HOME_SPEED);
        waitMotion(MOTION_TIMEOUT_MS);
      } 
      // If we aren't calibrated, we do nothing. Safe mode.
      g_gripped = false;
      enterState(GrabFSM::IDLE);
      break;
  }
}

// ============================================================
//  WEBSOCKETS (Running on Core 1)
// ============================================================
static void broadcastStatus() {
  static uint32_t lastMs = 0;
  if (millis() - lastMs < 200) return;
  lastMs = millis();
  if (g_wsServer.connectedClients() == 0) return;

  static const char* fsmNames[] = { "IDLE","SCANNING","APPROACH","DESCEND","GRIP","LIFT","HANDOFF","HOME" };
  char buf[320];
  snprintf(buf, sizeof(buf),
    "{\"type\":\"status\",\"fsm\":\"%s\",\"auto\":%s,\"gripped\":%s,"
    "\"base_deg\":%.1f,\"sh_deg\":%.1f,\"el_deg\":%.1f,\"fa_deg\":%.1f,"
    "\"safety_scale\":%.2f,\"fault\":%s,\"fault_reason\":\"%s\"}",
    fsmNames[(uint8_t)g_arm.fsm], g_autoMode ? "true" : "false", g_gripped ? "true" : "false",
    g_arm.current.base * 180.0f / M_PI, g_arm.current.shoulder * 180.0f / M_PI,
    g_arm.current.elbow * 180.0f / M_PI, g_arm.current.forearm * 180.0f / M_PI,
    g_safetyScale, g_faultActive ? "true" : "false", g_faultReason.c_str()
  );
  g_wsServer.broadcastTXT(buf);
}

void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    char buf[120];
    snprintf(buf, sizeof(buf), "{\"type\":\"hello\",\"safety_scale\":%.2f,\"fault\":%s}",
             g_safetyScale, g_faultActive ? "true" : "false");
    g_wsServer.sendTXT(num, buf);
  } else if (type == WStype_DISCONNECTED) {
    g_autoMode = false;
    if (g_arm.fsm != GrabFSM::IDLE && g_arm.fsm != GrabFSM::HOME) enterState(GrabFSM::HOME);
  } else if (type == WStype_TEXT) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, payload, length)) return;
    const char* msgType = doc["type"] | "";

    if (strcmp(msgType, "safety") == 0) {
      float newScale = doc["scale"] | -1.0f;
      if (newScale >= 0.0f && newScale <= 1.0f) {
        g_safetyScale = newScale; applyAccel();
        char resp[80]; snprintf(resp, sizeof(resp), "{\"type\":\"safety_ack\",\"scale\":%.2f}", g_safetyScale);
        g_wsServer.sendTXT(num, resp);
      }
    } else if (strcmp(msgType, "fault_reset") == 0) {
      if (g_faultActive) { clearFault(); g_autoMode = false; enterState(GrabFSM::HOME); }
    } else if (strcmp(msgType, "mode") == 0) {
      if (!g_faultActive && g_taught.calibrated) { // Only auto if calibrated
        g_autoMode = (strcmp((const char*)doc["value"], "auto") == 0);
        if (g_autoMode && g_arm.fsm == GrabFSM::IDLE) enterState(GrabFSM::SCANNING);
      }
    } else if (strcmp(msgType, "target") == 0) {
      if (g_autoMode && !g_faultActive && (float)doc["confidence"] >= CV_CONFIDENCE_THRESHOLD) {
        g_cvTarget.x_mm = doc["x"]; g_cvTarget.y_mm = doc["y"]; g_cvTarget.z_mm = doc["z"];
        g_cvTarget.confidence = doc["confidence"];
        g_targetFresh = true; g_targetAgeMs = millis();
      }
    } else if (strcmp(msgType, "stop") == 0 || strcmp(msgType, "home") == 0) {
      g_autoMode = false; enterState(GrabFSM::HOME);
    } else if (strcmp(msgType, "ping") == 0) {
      g_wsServer.sendTXT(num, "{\"type\":\"pong\"}");
    }
  }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n=== ARACHNAE v2: DUAL-CORE GOD-MODE (HOTSPOT) ==="));

  // 1. Initialize Servos 
  servoInit(); 
  
  // 2. Initialize Camera Hardware
  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, HIGH); 
  camera_config_t cam;
  cam.pin_pwdn = PWDN_GPIO_NUM; cam.pin_reset = RESET_GPIO_NUM; cam.pin_xclk = XCLK_GPIO_NUM;
  cam.pin_sscb_sda = SIOD_GPIO_NUM; cam.pin_sscb_scl = SIOC_GPIO_NUM;
  cam.pin_d7 = Y9_GPIO_NUM; cam.pin_d6 = Y8_GPIO_NUM; cam.pin_d5 = Y7_GPIO_NUM; cam.pin_d4 = Y6_GPIO_NUM;
  cam.pin_d3 = Y5_GPIO_NUM; cam.pin_d2 = Y4_GPIO_NUM; cam.pin_d1 = Y3_GPIO_NUM; cam.pin_d0 = Y2_GPIO_NUM;
  cam.pin_vsync = VSYNC_GPIO_NUM; cam.pin_href = HREF_GPIO_NUM; cam.pin_pclk = PCLK_GPIO_NUM;
  cam.xclk_freq_hz = 20000000; cam.pixel_format = PIXFORMAT_JPEG;
  
  cam.frame_size = STREAM_FRAMESIZE; 
  cam.jpeg_quality = STREAM_QUALITY; 
  cam.fb_count = STREAM_FB_COUNT;
  cam.fb_location = CAMERA_FB_IN_PSRAM; 
  cam.grab_mode = CAMERA_GRAB_LATEST;
  cam.ledc_channel = LEDC_CHANNEL_0; cam.ledc_timer = LEDC_TIMER_0; 

  if (esp_camera_init(&cam) != ESP_OK) {
    Serial.println(F("[CAM] Hardware Init FAILED. Check Ribbon Cable."));
  } else {
    sensor_t* s = esp_camera_sensor_get();
    s->set_exposure_ctrl(s, 1); s->set_aec2(s, 1); s->set_ae_level(s, 0);
    s->set_whitebal(s, 1); s->set_awb_gain(s, 1); s->set_wb_mode(s, 0);
    s->set_sharpness(s, 1); s->set_denoise(s, 1);
    s->set_hmirror(s, 0); s->set_vflip(s, 0); 
    Serial.println(F("[CAM] OV2640 Ready"));
  }

  // 3. Initialize Wi-Fi (Station Mode -> Phone Hotspot)
  WiFi.mode(WIFI_STA);
  WiFi.begin(STA_SSID, STA_PASS);
  Serial.print(F("[WiFi] Connecting to hotspot "));
  
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(500); Serial.print(".");
  }
  Serial.println();
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[WiFi] FAILED to connect to the hotspot! Check hotspot password."));
  } else {
    String ip = WiFi.localIP().toString();
    Serial.printf("[WiFi] Connected! IP Address: %s\n", ip.c_str());

    // 4. Start Network Services
    startCameraServer(); // Port 81
    teachServerSetup();  // Port 80
    g_wsServer.begin();  // Port 8765
    g_wsServer.onEvent(onWsEvent);

    Serial.printf("[Stream] http://%s:81/stream\n", ip.c_str());
    Serial.printf("[Teach]  http://%s/teach\n", ip.c_str());
    Serial.printf("[WS]     ws://%s:8765\n", ip.c_str());
  }

  // 5. SECURE BOOT SEQUENCE
  if (g_taught.calibrated) {
    Serial.println(F("[Boot] Moving to HOME..."));
    moveToTaught("home", HOME_SPEED);
    waitMotion(MOTION_TIMEOUT_MS);
    Serial.println(F("[Boot] System Active. Awaiting CV targets."));
    g_arm.fsm = GrabFSM::IDLE;
  } else {
    String ip = WiFi.localIP().toString();
    Serial.println(F("============================================="));
    Serial.println(F(" [Boot] ⚠️ UNCALIBRATED. SERVOS DISABLED."));
    Serial.printf(" [Boot] Open http://%s/teach to calibrate.\n", ip.c_str());
    Serial.println(F("============================================="));
    g_arm.fsm = GrabFSM::IDLE; 
  }
}

// ============================================================
//  MAIN LOOP (Running on Core 1)
// ============================================================
void loop() {
  teachServerLoop();
  g_wsServer.loop();
  runSafetyMonitor();

  if (g_targetFresh && millis() - g_targetAgeMs > TARGET_STALE_MS) {
    g_targetFresh = false;
  }

  // Only allow the arm to run autonomous tracking if it has been calibrated
  if (!g_teachActive && !g_faultActive && g_taught.calibrated) {
    runFSM();
  }

  broadcastStatus();
  delay(10);
}