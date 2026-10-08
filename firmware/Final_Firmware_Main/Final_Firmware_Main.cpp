/*
 * ============================================================
 * PROJET ARACHNAE v2 — GOD-MODE FIRMWARE (HOTSPOT EDITION)
 * Board: Seeed Studio XIAO ESP32S3 Sense
 * Network: Station Mode (STA) connecting to Mobile Hotspot
 * * Architecture:
 * - CORE 0: Handles Wi-Fi, HTTP Server, and OV2640 MJPEG Streaming
 * - CORE 1: Handles WebSockets, IK Math, Safety Monitor, and Servos
 *
 * OTA:
 *   ArduinoOTA enabled after WiFi connects. onStart() calls
 *   emergencyStop() so the arm cannot be mid-motion while its
 *   own firmware is being overwritten. Set OTA_PASSWORD in
 *   secrets.h before mounting on the arm.
 *
 * REMOTE LOGGING (wsLog):
 *   Once the XIAO is mounted and off USB, Serial is unreachable.
 *   wsLog(msg) prints to Serial AND broadcasts
 *   {"type":"log","msg":"...","t":<millis>} to every connected
 *   WebSocket client on port 8765. Connect any WS client to
 *   ws://<ip>:8765 to see these live. It's forward-declared here
 *   (before the other includes) so servo_control.h and friends
 *   can call it too — see servo_control.h for the safety-side
 *   log calls (emergency stops, load strikes, gripper contact).
 *   No client connected? It just skips the broadcast and still
 *   prints to Serial, so USB debugging still works unchanged.
 * ============================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include "esp_camera.h"
#include "esp_http_server.h"

// ── Remote log forward declaration ────────────────────────────
// Defined below, after g_wsServer exists. Declared here so every
// header included after this point (servo_control.h, teach_mode.h,
// etc.) can call it too.
void wsLog(const String& msg);
// Send one text frame to one WebSocket client (the CV capture agent).
void wsSendTo(uint8_t num, const char* msg);

// g_autoMode is defined below (with the other arm-state globals) but
// teach_mode.h's teachModeStart() needs to clear it, and that header is
// included before this file's own globals block — same pattern as wsLog.
extern bool g_autoMode;

// ── Include your custom headers ──────────────────────────────
#include "config.h"
#include "kinematics.h"
#include "servo_control.h"
#include "teach_mode.h"
#include "teach_server.h"

// ── WiFi Config ──────────────────────────────────────────────
// Credentials come from secrets.h (gitignored) — STA_SSID / STA_PASS for the
// phone hotspot, AP_SSID / AP_PASS for the arm's own fallback network.
// These used to be hardcoded here while secrets.h sat unused, which is why
// the documented AP fallback never worked.
#include "secrets.h"

// ── OTA Config ────────────────────────────────────────────────
// OTA_PASSWORD lives in secrets.h with the Wi-Fi credentials. Anyone on the
// same network who knows it can push firmware over the air.
static const char* OTA_HOSTNAME = "arachnae";

// Bumped by hand to visually confirm an OTA push actually applied new code
// (printed once at boot, over both USB serial and the wsLog feed).
#define FW_BUILD_TAG "suite-6-capture"

// ── WebSocket server (port 8765) ─────────────────────────────
WebSocketsServer g_wsServer(8765);

// ============================================================
//  wsLog — Serial print + WS broadcast, for remote debugging
//  once the board is mounted and USB is unreachable.
// ============================================================
void wsLog(const String& msg) {
  Serial.println(msg);
  if (g_wsServer.connectedClients() == 0) return;   // nobody listening — skip broadcast
  StaticJsonDocument<320> doc;
  doc["type"] = "log";
  doc["msg"]  = msg;
  doc["t"]    = millis();
  char buf[384];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  g_wsServer.broadcastTXT(buf, n);
}

void wsSendTo(uint8_t num, const char* msg) { g_wsServer.sendTXT(num, msg); }

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
bool              g_autoMode    = false;   // extern-declared above the includes
static uint32_t   g_targetAgeMs = 0;
static bool       g_gripped     = false;
static int        g_pendingState = -1;

static constexpr uint32_t TARGET_STALE_MS = 3000;
static const char* FSM_NAMES[] = { "IDLE","SCANNING","APPROACH","DESCEND","GRIP","LIFT","HANDOFF","HOME" };

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
  jointsTick();
}

static bool waitMotion(uint32_t timeout_ms) {
  return waitForMotionComplete(timeout_ms, 8, tickServers);
}

static bool moveToXYZ(float x, float y, float z, uint16_t speed, uint32_t timeout_ms) {
  if (g_faultActive) return false;
  JointAngles angles;
  float pitchDeg;
  IKResult res = solveWithPitches(x, y, z, angles, pitchDeg);   // same pitch order the /test CV panel reports
  if (res == IKResult::OK) {
    armMoveToAngles(angles, speed);
    g_arm.current = angles;
    return waitMotion(timeout_ms);
  }
  char buf[96];
  snprintf(buf, sizeof(buf), "[IK] No solution for (%.0f, %.0f, %.0f): %s", x, y, z, ikResultStr(res));
  wsLog(buf);
  return false;
}

// The active target raised straight up (BODY frame) by mm, in the arm frame.
static Vec3 aboveTarget(float mm) {
  return Vec3{g_activeTarget.x_mm, g_activeTarget.y_mm, g_activeTarget.z_mm} + upInArm() * mm;
}

// Waits without stalling WiFi / WebSocket / the safety monitor / the trackers.
static void pumpFor(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms && !g_faultActive) { tickServers(); delay(20); }
}

static uint32_t g_fsmEnterMs = 0;

static void enterState(GrabFSM s) {
  g_arm.fsm    = s;
  g_fsmEnterMs = millis();
  char buf[48];
  snprintf(buf, sizeof(buf), "[FSM] -> %s", FSM_NAMES[(uint8_t)s]);
  wsLog(buf);
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
      Vec3 a = aboveTarget(APPROACH_CLEARANCE_MM);
      bool ok = moveToXYZ(a.x, a.y, a.z, APPROACH_SPEED, MOTION_TIMEOUT_MS);
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
      gripperCloseToContact(200, tickServers);
      pumpFor(300);
      g_gripped = (gripperLoad() > 150);
      enterState(GrabFSM::LIFT);
      break;

    case GrabFSM::LIFT: {
      Vec3 l = aboveTarget(LIFT_HEIGHT_MM);
      moveToXYZ(l.x, l.y, l.z, LIFT_SPEED, MOTION_TIMEOUT_MS);
      enterState(GrabFSM::HANDOFF);
      break;
    }

    case GrabFSM::HANDOFF: {
      // The taught handoff pose (the FSM only runs once all 7 are taught).
      // Joints only: moveToTaught() would also drive the gripper to the
      // opening it had when the pose was taught, dropping the object.
      if (g_taught.savedFlags & (1u << P_HANDOFF)) {
        JointAngles angles = poseToAngles(g_taught.pose[P_HANDOFF]);
        armMoveToAngles(angles, HANDOFF_SPEED);
        g_arm.current = angles;
        // Hold the object out, then let go. This used to wait for the load
        // to DROP and then CLOSE the jaws, so it never released: the object
        // had to be wrenched out or was carried back home.
        if (waitMotion(MOTION_TIMEOUT_MS)) {
          char buf[64];
          snprintf(buf, sizeof(buf), "[FSM] Handoff: take it - releasing in %.1f s", HANDOFF_HOLD_MS / 1000.0f);
          wsLog(buf);
          pumpFor(HANDOFF_HOLD_MS);
          gripperOpen();
          pumpFor(HANDOFF_RELEASE_MS);
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

  char buf[320];
  snprintf(buf, sizeof(buf),
    "{\"type\":\"status\",\"fsm\":\"%s\",\"auto\":%s,\"gripped\":%s,"
    "\"base_deg\":%.1f,\"sh_deg\":%.1f,\"el_deg\":%.1f,\"fa_deg\":%.1f,"
    "\"safety_scale\":%.2f,\"fault\":%s,\"fault_reason\":\"%s\"}",
    FSM_NAMES[(uint8_t)g_arm.fsm], g_autoMode ? "true" : "false", g_gripped ? "true" : "false",
    g_arm.current.base * 180.0f / M_PI, g_arm.current.shoulder * 180.0f / M_PI,
    g_arm.current.elbow * 180.0f / M_PI, g_arm.current.forearm * 180.0f / M_PI,
    g_safetyScale, g_faultActive ? "true" : "false", g_faultReason.c_str()
  );
  g_wsServer.broadcastTXT(buf);
}

void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    char buf[140];
    snprintf(buf, sizeof(buf), "{\"type\":\"hello\",\"safety_scale\":%.2f,\"fault\":%s,\"build\":\"" FW_BUILD_TAG "\"}",
             g_safetyScale, g_faultActive ? "true" : "false");
    g_wsServer.sendTXT(num, buf);
  } else if (type == WStype_DISCONNECTED) {
    if (g_cvCap.agent == num) { g_cvCap.agent = -1; wsLog("[CV] capture agent disconnected"); }
    g_autoMode = false;
    if (g_arm.fsm != GrabFSM::IDLE && g_arm.fsm != GrabFSM::HOME) enterState(GrabFSM::HOME);
  } else if (type == WStype_TEXT) {
    JsonDocument doc;   // cv_obs is well over 256 bytes
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
    } else if (strcmp(msgType, "estop") == 0) {
      emergencyStop("E-STOP (websocket)");
    } else if (strcmp(msgType, "mode") == 0) {
      // auto needs armed + calibrated (all 7 poses AND gripper open/closed) + no fault
      if (g_armed && !g_faultActive && g_taught.calibrated && gripperCalibrated() && !demoRunning()) {
        g_autoMode = (strcmp((const char*)doc["value"], "auto") == 0);
        if (g_autoMode && g_arm.fsm == GrabFSM::IDLE) enterState(GrabFSM::SCANNING);
      }
    } else if (strcmp(msgType, "target") == 0) {
      if (g_autoMode && !g_faultActive && (float)doc["confidence"] >= CV_CONFIDENCE_THRESHOLD) {
        const char* frame = doc["frame"] | "base";
        float lx = doc["x"], ly = doc["y"], lz = doc["z"];
        if (!strcmp(frame, "tip")) {
          Vec3 p = tipToBase(currentAngles(), lx, ly, lz);
          g_cvTarget.x_mm = p.x; g_cvTarget.y_mm = p.y; g_cvTarget.z_mm = p.z;
        } else {
          g_cvTarget.x_mm = lx; g_cvTarget.y_mm = ly; g_cvTarget.z_mm = lz;
        }
        g_cvTarget.confidence = doc["confidence"];
        g_targetFresh = true; g_targetAgeMs = millis();
      }
    } else if (strcmp(msgType, "stop") == 0 || strcmp(msgType, "home") == 0) {
      demoStop("stopped (websocket)");
      g_autoMode = false; enterState(GrabFSM::HOME);
    } else if (strcmp(msgType, "cv_obs") == 0) {
      cvObserve(doc);   // /test CV panel only - never moves the arm
    } else if (strcmp(msgType, "agent_hello") == 0) {
      if (strcmp(doc["role"] | "", "capture") == 0) { g_cvCap.agent = num; wsLog("[CV] capture agent connected"); }
    } else if (strcmp(msgType, "cv_capture_status") == 0) {
      cvCaptureStatus(doc);   // laptop recorder's progress for /test - never moves the arm
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
  Serial.println(F("[Boot] Build: " FW_BUILD_TAG));

  // 1. Initialize Servos
  servoInit();
  loadTaughtPositions();   // taught slots + envelope + boot-pose signature, from NVS
  loadDemo();              // demo sequence steps, from NVS
  
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
    camLoadAndApply();   // image settings saved from /test (brightness, white balance, mirror...)
    Serial.println(F("[CAM] OV2640 Ready"));
  }

  // 3. Initialize Wi-Fi.
  //
  // WIFI_BOOT_MODE (config.h): 0 = try the hotspot, fall back to our own AP;
  // 1 = AP only; 2 = STA only. The fallback matters because the services
  // below used to start ONLY on a successful hotspot connect — if the phone
  // wasn't broadcasting at boot, the arm came up with no teach server, no
  // camera, no WebSocket and no OTA, and the only way back was a power-cycle.
  // Now the arm always ends up reachable on some network.
  bool staUp = false;

#if WIFI_BOOT_MODE != 1
  WiFi.mode(WIFI_STA);
  esp_wifi_set_country_code(WIFI_COUNTRY, false);   // channels 12-13 (config.h)
  WiFi.setSleep(false);              // keep latency down; this link is our only control path
  WiFi.setAutoReconnect(true);       // survive a brief hotspot drop without a reboot
  WiFi.begin(STA_SSID, STA_PASS);
  Serial.printf("[WiFi] Connecting to %s ", STA_SSID);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_STA_TIMEOUT_MS) {
    delay(500); Serial.print(".");
  }
  Serial.println();
  staUp = (WiFi.status() == WL_CONNECTED);
#endif

  String ip;
  if (staUp) {
    ip = WiFi.localIP().toString();
    Serial.printf("[WiFi] Connected to %s — IP %s\n", STA_SSID, ip.c_str());
  } else {
#if WIFI_BOOT_MODE == 2
    Serial.println(F("[WiFi] STA failed and WIFI_BOOT_MODE=2 (STA only) — no network."));
    ip = "0.0.0.0";
#else
    // Own access point. Address here is always 192.168.4.1, which is the
    // whole point: no subnet hunting after the phone reassigns its range.
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    ip = WiFi.softAPIP().toString();
    Serial.printf("[WiFi] Hotspot not found — started own AP '%s' at %s\n", AP_SSID, ip.c_str());
#endif
  }

  // 4. Start Network Services — unconditionally, on whichever network we got.
  {
    startCameraServer(); // Port 81
    teachServerSetup();  // Port 80
    g_wsServer.begin();  // Port 8765
    g_wsServer.onEvent(onWsEvent);

    // Stable name so neither you nor the OTA tool has to chase the IP.
    if (MDNS.begin(OTA_HOSTNAME)) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[mDNS] http://%s.local/\n", OTA_HOSTNAME);
    }

    // ── OTA (Over-The-Air updates) ─────────────────────────
    // onStart() cuts torque immediately so the arm can never be
    // mid-motion while its own firmware is overwritten.
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart([]() {
      wsLog("[OTA] Update starting - cutting torque for safety");
      emergencyStop("OTA update in progress");
    });
    ArduinoOTA.onEnd([]() {
      wsLog("[OTA] Update complete - rebooting");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      // Progress goes to Serial only (too frequent for the log feed,
      // and the WS link is about to drop anyway once flashing starts).
      Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      char buf[64];
      const char* reason = "Unknown";
      if (error == OTA_AUTH_ERROR)         reason = "Auth Failed";
      else if (error == OTA_BEGIN_ERROR)   reason = "Begin Failed";
      else if (error == OTA_CONNECT_ERROR) reason = "Connect Failed";
      else if (error == OTA_RECEIVE_ERROR) reason = "Receive Failed";
      else if (error == OTA_END_ERROR)     reason = "End Failed";
      snprintf(buf, sizeof(buf), "[OTA] Error[%u]: %s", error, reason);
      wsLog(buf);
    });
    ArduinoOTA.begin();
    Serial.printf("[OTA] Ready — flash via IP %s or hostname %s.local\n",
                  ip.c_str(), OTA_HOSTNAME);

    Serial.printf("[Stream] http://%s:81/stream\n", ip.c_str());
    Serial.printf("[Teach]  http://%s/teach\n", ip.c_str());
    Serial.printf("[WS]     ws://%s:8765\n", ip.c_str());
    wsLog("[Boot] Build: " FW_BUILD_TAG " — WS log online");
  }

  // 5. SECURE BOOT SEQUENCE
  //
  // Nothing moves at boot, calibrated or not: powered motion is gated on
  // g_armed, which only the Arm button sets. This used to call
  // moveToTaught("home") here, which motionAllowed() silently refused while
  // the log claimed "System Active" — misleading on a machine you wear.
  g_arm.fsm = GrabFSM::IDLE;
  Serial.println(F("============================================="));
  if (g_taught.calibrated) {
    Serial.println(F(" [Boot] Calibrated. DISARMED — press Arm in /teach."));
  } else {
    Serial.println(F(" [Boot] UNCALIBRATED. Teach all 7 positions first."));
  }
  Serial.printf (" [Boot] Suite: http://%s/\n", ip.c_str());
  Serial.println(F("============================================="));
  wsLog("[Boot] Ready, DISARMED. Open the suite to arm.");
}

// ============================================================
//  MAIN LOOP (Running on Core 1)
// ============================================================
void loop() {
  ArduinoOTA.handle();
  teachServerLoop();
  g_wsServer.loop();
  runSafetyMonitor();
  jointsTick();
  demoTick();

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
