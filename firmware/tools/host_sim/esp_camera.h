#pragma once
#include "Arduino.h"
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
enum { PIXFORMAT_JPEG = 4, FRAMESIZE_VGA = 8, FRAMESIZE_SVGA = 9, CAMERA_FB_IN_PSRAM = 0, CAMERA_GRAB_LATEST = 1, LEDC_CHANNEL_0 = 0, LEDC_TIMER_0 = 0 };
struct camera_config_t { int pin_pwdn, pin_reset, pin_xclk, pin_sscb_sda, pin_sscb_scl, pin_d7, pin_d6, pin_d5, pin_d4, pin_d3, pin_d2, pin_d1, pin_d0, pin_vsync, pin_href, pin_pclk, xclk_freq_hz, pixel_format, frame_size, jpeg_quality, fb_count, fb_location, grab_mode, ledc_channel, ledc_timer; };
struct camera_fb_t { uint8_t* buf; size_t len; };
struct sensor_t {
  struct { int quality; } status;
  int set_exposure_ctrl(sensor_t*, int) { return 0; } int set_aec2(sensor_t*, int) { return 0; } int set_ae_level(sensor_t*, int) { return 0; }
  int set_whitebal(sensor_t*, int) { return 0; } int set_awb_gain(sensor_t*, int) { return 0; } int set_wb_mode(sensor_t*, int) { return 0; }
  int set_sharpness(sensor_t*, int) { return 0; } int set_denoise(sensor_t*, int) { return 0; } int set_contrast(sensor_t*, int) { return 0; }
  int set_saturation(sensor_t*, int) { return 0; } int set_brightness(sensor_t*, int) { return 0; } int set_hmirror(sensor_t*, int) { return 0; } int set_vflip(sensor_t*, int) { return 0; }
};
inline esp_err_t esp_camera_init(const camera_config_t*) { return ESP_OK; }
inline sensor_t* esp_camera_sensor_get() { static sensor_t s; return &s; }
inline camera_fb_t* esp_camera_fb_get() { return nullptr; }
inline void esp_camera_fb_return(camera_fb_t*) {}
#define MALLOC_CAP_SPIRAM 0
inline size_t heap_caps_get_free_size(int) { return 0; }
