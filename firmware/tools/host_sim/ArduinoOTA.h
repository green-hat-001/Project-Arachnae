#pragma once
#include "Arduino.h"
enum ota_error_t { OTA_AUTH_ERROR, OTA_BEGIN_ERROR, OTA_CONNECT_ERROR, OTA_RECEIVE_ERROR, OTA_END_ERROR };
class ArduinoOTAClass {
 public:
  typedef std::function<void()> VoidCb;
  typedef std::function<void(unsigned int, unsigned int)> ProgressCb;
  typedef std::function<void(ota_error_t)> ErrorCb;
  void setHostname(const char*) {}
  void setPassword(const char*) {}
  void onStart(VoidCb) {}
  void onEnd(VoidCb) {}
  void onProgress(ProgressCb) {}
  void onError(ErrorCb) {}
  void begin() {}
  void handle() {}
};
static ArduinoOTAClass ArduinoOTA;
