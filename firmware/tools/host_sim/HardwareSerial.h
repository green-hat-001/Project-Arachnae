#pragma once
#include "Arduino.h"
// ------------------------------------------------------------------
//  ST3215 bus simulator behind a HardwareSerial-like interface
// ------------------------------------------------------------------
struct SimServo {
  uint8_t id = 0;
  uint8_t reg[256];
  double  phys = 0;          // TRUE multi-turn position in steps
  bool    torque = false;
  double  handVel = 0;       // external (hand) velocity in steps/s while torque off
  double  pushVel = 0;       // external force that overpowers even a powered joint (belt slip etc.)
  uint16_t wheelRevBit = 0x8000;   // what the servo really treats as "reverse"
  int  modeWritesWithTorqueOn = 0; // protocol violations counted
  bool present = true;
  int  port = 1;                   // which ESP32 UART's junction this servo is wired to
  double maxSpeed = 3400;
  // Real-drivetrain imperfections the ideal model above lacks. Both default
  // to 0 so every older section keeps its ideal servo.
  //  springBack: steps the drivetrain unwinds, opposite the last powered
  //    motion, the instant torque drops (gear/belt wind-up, gravity on a
  //    loaded joint during a mode switch).
  //  holdDither: amplitude (steps) a powered Mode-0 servo hunts around its
  //    goal once it gets there. The real ST3215 is single-turn in Mode 0, so
  //    a dither that dips below raw 0 lands at 4095 and it drives the long way.
  double springBack = 0;
  double holdDither = 0;
  int    lastDir = 0;
  bool   ditherHi = false;
  uint16_t w(int a) const { return reg[a] | (reg[a + 1] << 8); }
  void setW(int a, uint16_t v) { reg[a] = v & 0xFF; reg[a + 1] = v >> 8; }
  SimServo() { memset(reg, 0, sizeof reg); reg[0x3E] = 120; reg[0x3F] = 30; }
  void setId(uint8_t i) { id = i; reg[0x05] = i; }
  int withinRev() const { long long p = (long long)std::floor(phys); long long m = p % 4096; if (m < 0) m += 4096; return (int)m; }
  // Present load (0x3C) model, off unless one is set: loadBase + speed *
  // loadPerSpeed, clipped at the torque limit (0x22) like the real register,
  // which pins at the limit when the duty is capped. 0 with torque off.
  double loadBase = 0, loadPerSpeed = 0;
  void step(double dt) {
    double p0 = phys;
    phys += pushVel * dt;
    if (!torque) { phys += handVel * dt; if (loadBase > 0 || loadPerSpeed > 0) setW(0x3C, 0); return; }
    stepPowered(dt);
    if (loadBase > 0 || loadPerSpeed > 0) {
      double v = dt > 0 ? std::fabs(phys - p0) / dt : 0;
      double cap = w(0x22) ? (double)w(0x22) : 1000.0;
      setW(0x3C, (uint16_t)std::min(cap, loadBase + v * loadPerSpeed));
    }
  }
  void dropTorque() { if (springBack != 0) phys -= lastDir * springBack; }
  void stepPowered(double dt) {
    int mode = reg[0x21];
    if (mode == 1) {
      uint16_t sp = w(0x2E);
      double v;
      if (wheelRevBit == 0x8000) v = (sp & 0x8000) ? -(double)(sp & 0x7FFF) : (double)sp;
      else                       v = (sp & 0x0400) ? -(double)(sp & 0x03FF) : (double)(sp & 0x03FF);
      phys += std::min(std::fabs(v), maxSpeed) * (v < 0 ? -1 : 1) * dt;
      if (v != 0) lastDir = (v < 0 ? -1 : 1);
    } else {
      double base = std::floor(phys / 4096.0) * 4096.0;
      double tgt = base + w(0x2A);
      double sp = w(0x2E); if (sp == 0) sp = maxSpeed;
      sp = std::min(sp, maxSpeed);
      double d = tgt - phys, mx = sp * dt;
      if (std::fabs(d) <= mx) {
        if (holdDither > 0) { ditherHi = !ditherHi; phys = tgt + (ditherHi ? holdDither : -holdDither); }
        else phys = tgt;
      } else { phys += (d > 0 ? mx : -mx); lastDir = (d > 0 ? 1 : -1); }
    }
  }
};

struct SimBus {
  SimServo servo[8];           // ids 1..5 used
  bool echo = true;
  // Electrical limit of one resistor junction: with more than this many
  // servos wired to a port, nothing on that port hears the ESP32 (the
  // 2026-09-27 full-bus test: 4 servos fine, 5 -> base/elbow never answer).
  // 0 = unlimited, the ideal default.
  int junctionLimit = 0;
  int dropReads = 0;           // number of upcoming read requests to ignore
  bool dropAll = false;
  int corruptReads = 0;        // upcoming read replies to damage (one data byte flipped, checksum left stale)
  int foreignReads = 0;        // upcoming read replies to send back under the wrong servo ID
  long long writes = 0, reads = 0;
  std::vector<std::string> log;
  SimBus() { for (int i = 1; i <= 5; i++) servo[i].setId(i); }
  void feed(const uint8_t* d, size_t n);
};
extern SimBus g_sim;

class HardwareSerial {
 public:
  explicit HardwareSerial(int port) : port(port) {}
  int port;
  std::vector<uint8_t> rx;     // what this UART's RX pin has received
  bool begun = false;
  void begin(unsigned long, int = 0, int = -1, int = -1) { begun = true; }
  size_t write(const uint8_t* d, size_t n);
  void flush() {}
  int available() { sim_advance_us(5); return (int)rx.size(); }
  int read() { if (rx.empty()) return -1; int v = rx.front(); rx.erase(rx.begin()); return v; }
  size_t readBytes(uint8_t* buf, size_t len) { for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)read(); return len; }
};
extern HardwareSerial Serial1;
extern HardwareSerial Serial2;
