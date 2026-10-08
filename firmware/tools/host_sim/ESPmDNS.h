#pragma once
#include "Arduino.h"

// Host-side stub for the ESP32 mDNS responder. The firmware uses it only to
// publish a stable "arachnae.local" name for the teach server; there is no
// behaviour here worth simulating, so begin() simply succeeds.
class MDNSResponderStub {
 public:
  bool begin(const char*) { return true; }
  void addService(const char*, const char*, uint16_t) {}
  void end() {}
};
static MDNSResponderStub MDNS;
