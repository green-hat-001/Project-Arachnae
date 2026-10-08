#pragma once
#include "Arduino.h"
struct IPAddress { String toString() const { return "192.168.4.1"; } };
enum { WIFI_STA = 1, WIFI_AP = 2 };
enum { WL_CONNECTED = 3, WL_DISCONNECTED = 6 };
struct WiFiClass {
  bool staUp = false;
  void mode(int) {}
  void begin(const char*, const char*) { staUp = true; }
  int status() { return staUp ? WL_CONNECTED : WL_DISCONNECTED; }
  IPAddress localIP() { return {}; }
  IPAddress softAPIP() { return {}; }
  bool softAP(const char*, const char*) { return true; }
  void disconnect(bool) {}
  void setSleep(bool) {}
  void setAutoReconnect(bool) {}
};
static WiFiClass WiFi;
