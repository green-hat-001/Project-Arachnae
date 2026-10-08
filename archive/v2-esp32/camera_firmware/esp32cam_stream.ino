/*
 * ============================================================
 *  PROJET ARACHNAE — Camera Stream Firmware
 *  Board: Seeed Studio XIAO ESP32S3 Sense
 *
 *  Key differences from AI-Thinker ESP32-CAM:
 *    - Completely different pin assignment (see below)
 *    - 8MB PSRAM always present — always runs at high resolution
 *    - ESP32-S3 has a dedicated camera peripheral (no LEDC clock)
 *    - USB-C native — flash by just plugging in, no boot button trick
 *    - Smaller (21×17.5mm) — fits cleanly on the gripper housing
 *    - OV2640 on detachable ribbon cable
 *
 *  Arduino IDE board setting:
 *    Tools → Board → XIAO_ESP32S3
 *    Tools → PSRAM → OPI PSRAM  ← IMPORTANT, enables the 8MB
 *    Tools → Partition Scheme → Huge APP (3MB No OTA)
 *
 *  Streams MJPEG at http://<IP>/stream
 *  Single capture at http://<IP>/capture
 *  Info JSON at     http://<IP>/info
 * ============================================================
 */

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// ── WiFi — must match main ESP32 ────────────────────────────
const char* SSID     = "YOUR_HOTSPOT";
const char* PASSWORD = "YOUR_PASSWORD";

// ── XIAO ESP32S3 Sense OV2640 pin map ────────────────────────
// Do NOT change these — they are fixed by the PCB routing.
#define PWDN_GPIO_NUM    -1   // no power-down pin on this board
#define RESET_GPIO_NUM   -1   // no hardware reset pin
#define XCLK_GPIO_NUM    10
#define SIOD_GPIO_NUM    40   // I2C SDA (camera config bus)
#define SIOC_GPIO_NUM    39   // I2C SCL
#define Y9_GPIO_NUM      48   // D7 — MSB of pixel data
#define Y8_GPIO_NUM      11
#define Y7_GPIO_NUM      12
#define Y6_GPIO_NUM      14
#define Y5_GPIO_NUM      16
#define Y4_GPIO_NUM      18
#define Y3_GPIO_NUM      17
#define Y2_GPIO_NUM      15   // D0 — LSB of pixel data
#define VSYNC_GPIO_NUM   38
#define HREF_GPIO_NUM    47
#define PCLK_GPIO_NUM    13

// ── On-board LED (active LOW on XIAO ESP32S3) ────────────────
#define LED_PIN          21

// ── Stream parameters ────────────────────────────────────────
// XIAO ESP32S3 Sense has 8MB OPI PSRAM always available.
// We run at SVGA (800×600) for a good balance of FOV and
// frame rate (~15fps over WiFi). Bump to XGA (1024×768) if
// you need more detail; drop to VGA (640×480) for faster CV.
#define STREAM_FRAMESIZE  FRAMESIZE_SVGA   // 800×600
#define STREAM_QUALITY    12               // JPEG quality 0-63, lower = better
#define STREAM_FB_COUNT    2               // frame buffers in PSRAM

// ── MJPEG boundary ───────────────────────────────────────────
#define PART_BOUNDARY "frame"
static const char* STREAM_CONTENT_TYPE =
  "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART =
  "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;

// ── Frame counter for debug ───────────────────────────────────
static uint32_t frame_count = 0;

// ============================================================
//  MJPEG stream handler
// ============================================================
static esp_err_t stream_handler(httpd_req_t* req) {
  camera_fb_t* fb  = NULL;
  esp_err_t    res = ESP_OK;
  char         part_buf[64];

  res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  // Allow phone CV app to connect cross-origin
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "15");

  digitalWrite(LED_PIN, LOW);   // LED on while streaming

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) { res = ESP_FAIL; break; }

    res = httpd_resp_send_chunk(req,
            STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));

    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, sizeof(part_buf),
                             STREAM_PART, fb->len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req,
              (const char*)fb->buf, fb->len);
    }

    esp_camera_fb_return(fb);
    frame_count++;

    if (res != ESP_OK) break;
  }

  digitalWrite(LED_PIN, HIGH);  // LED off
  return res;
}

// ── Single JPEG capture ──────────────────────────────────────
static esp_err_t capture_handler(httpd_req_t* req) {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { httpd_resp_send_500(req); return ESP_FAIL; }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Content-Disposition",
                     "inline; filename=arachnae_capture.jpg");

  esp_err_t res = httpd_resp_send(req,
                    (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

// ── Info endpoint (for CV pipeline to confirm resolution) ────
static esp_err_t info_handler(httpd_req_t* req) {
  char buf[256];
  sensor_t* s = esp_camera_sensor_get();
  snprintf(buf, sizeof(buf),
    "{"
    "\"board\":\"XIAO_ESP32S3_Sense\","
    "\"sensor\":\"OV2640\","
    "\"framesize\":%d,"
    "\"width\":800,"
    "\"height\":600,"
    "\"quality\":%d,"
    "\"frames_served\":%lu,"
    "\"psram_free\":%lu"
    "}",
    STREAM_FRAMESIZE,
    s ? s->status.quality : -1,
    frame_count,
    (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM)
  );
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_send(req, buf, strlen(buf));
  return ESP_OK;
}

// ── Start HTTP server ─────────────────────────────────────────
void startCameraServer() {
  httpd_config_t config  = HTTPD_DEFAULT_CONFIG();
  config.server_port     = 80;
  config.max_uri_handlers = 8;

  httpd_uri_t uris[] = {
    { "/stream",  HTTP_GET, stream_handler,  NULL },
    { "/capture", HTTP_GET, capture_handler, NULL },
    { "/info",    HTTP_GET, info_handler,    NULL },
  };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    for (auto& u : uris)
      httpd_register_uri_handler(stream_httpd, &u);
    Serial.println(F("[CAM] HTTP server started"));
    Serial.printf("[CAM] Stream:  http://%s/stream\n",
                  WiFi.localIP().toString().c_str());
    Serial.printf("[CAM] Capture: http://%s/capture\n",
                  WiFi.localIP().toString().c_str());
    Serial.printf("[CAM] Info:    http://%s/info\n",
                  WiFi.localIP().toString().c_str());
  } else {
    Serial.println(F("[CAM] Server start FAILED"));
  }
}

// ============================================================
//  setup
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n=== ARACHNAE CAM — XIAO ESP32S3 Sense ==="));

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);   // LED off (active low)

  // ── Camera config ─────────────────────────────────────────
  camera_config_t cam;

  cam.pin_pwdn     = PWDN_GPIO_NUM;
  cam.pin_reset    = RESET_GPIO_NUM;
  cam.pin_xclk     = XCLK_GPIO_NUM;
  cam.pin_sscb_sda = SIOD_GPIO_NUM;
  cam.pin_sscb_scl = SIOC_GPIO_NUM;

  cam.pin_d7    = Y9_GPIO_NUM;
  cam.pin_d6    = Y8_GPIO_NUM;
  cam.pin_d5    = Y7_GPIO_NUM;
  cam.pin_d4    = Y6_GPIO_NUM;
  cam.pin_d3    = Y5_GPIO_NUM;
  cam.pin_d2    = Y4_GPIO_NUM;
  cam.pin_d1    = Y3_GPIO_NUM;
  cam.pin_d0    = Y2_GPIO_NUM;

  cam.pin_vsync = VSYNC_GPIO_NUM;
  cam.pin_href  = HREF_GPIO_NUM;
  cam.pin_pclk  = PCLK_GPIO_NUM;

  cam.xclk_freq_hz = 20000000;    // 20 MHz — standard for OV2640
  cam.pixel_format = PIXFORMAT_JPEG;

  // XIAO ESP32S3 Sense always has 8MB OPI PSRAM.
  // If you somehow don't see it, check Tools → PSRAM → OPI PSRAM.
  cam.frame_size   = STREAM_FRAMESIZE;
  cam.jpeg_quality = STREAM_QUALITY;
  cam.fb_count     = STREAM_FB_COUNT;
  cam.fb_location  = CAMERA_FB_IN_PSRAM;   // store frames in PSRAM
  cam.grab_mode    = CAMERA_GRAB_LATEST;   // always get newest frame

  // ledc_channel and ledc_timer are NOT needed on ESP32-S3
  // (it uses the dedicated LCD_CAM peripheral instead of LEDC).
  // Set them to safe dummy values — the driver ignores them on S3.
  cam.ledc_channel = LEDC_CHANNEL_0;
  cam.ledc_timer   = LEDC_TIMER_0;

  esp_err_t err = esp_camera_init(&cam);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Init FAILED: 0x%x\n", err);
    Serial.println(F("[CAM] Check ribbon cable is fully seated"));
    // Blink LED to signal error
    for (int i = 0; i < 10; i++) {
      digitalWrite(LED_PIN, LOW); delay(100);
      digitalWrite(LED_PIN, HIGH); delay(100);
    }
    return;
  }
  Serial.println(F("[CAM] OV2640 init OK"));

  // ── Sensor tuning for indoor / gripper-mounted use ────────
  sensor_t* s = esp_camera_sensor_get();

  s->set_framesize(s,    STREAM_FRAMESIZE);
  s->set_quality(s,      STREAM_QUALITY);

  // Auto exposure — important since gripper will be at varying
  // distances and lighting conditions
  s->set_exposure_ctrl(s, 1);   // AEC on
  s->set_aec2(s,          1);   // AEC DSP on
  s->set_ae_level(s,      0);   // no exposure bias

  // Auto white balance — bottles can be lit by any light source
  s->set_whitebal(s,  1);
  s->set_awb_gain(s,  1);
  s->set_wb_mode(s,   0);       // auto

  // Sharpness / noise — moderate for close-up work
  s->set_sharpness(s,   1);
  s->set_denoise(s,     1);
  s->set_contrast(s,    0);
  s->set_saturation(s,  0);
  s->set_brightness(s,  0);

  // Horizontal flip: the XIAO's ribbon routes the camera
  // to the right side of the board. Flip if image is mirrored.
  // Set to 1 if the live stream looks backwards on your mount.
  s->set_hmirror(s, 0);
  s->set_vflip(s,   0);

  Serial.printf("[CAM] Resolution: 800×600 (SVGA), quality: %d\n",
                STREAM_QUALITY);
  Serial.printf("[CAM] PSRAM free: %lu bytes\n",
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  // ── WiFi ──────────────────────────────────────────────────
  WiFi.begin(SSID, PASSWORD);
  Serial.print(F("[CAM] Connecting WiFi"));
  uint8_t attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print('.');
    attempts++;
    // Blink LED while connecting
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);  // solid off = connected
    Serial.printf("\n[CAM] Connected. IP: %s\n",
                  WiFi.localIP().toString().c_str());
  } else {
    Serial.println(F("\n[CAM] WiFi FAILED — check credentials"));
    return;
  }

  startCameraServer();
  Serial.println(F("[CAM] Ready"));
}

// ============================================================
//  loop — server runs on its own FreeRTOS task
// ============================================================
void loop() {
  // Blink LED briefly every 5s to show the board is alive
  static uint32_t last = 0;
  if (millis() - last > 5000) {
    digitalWrite(LED_PIN, LOW);  delay(50);
    digitalWrite(LED_PIN, HIGH);
    last = millis();
  }
  delay(100);
}
