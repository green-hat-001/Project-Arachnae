#pragma once
// Host-side stub of the Arduino/ESP32 core + an ST3215 servo bus simulator.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>
#include <cstdarg>
using std::min; using std::max;

#define PROGMEM
#define M_PI_F 3.14159265f
#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
#endif
#define radians(deg) ((deg)*DEG_TO_RAD)
#define degrees(rad) ((rad)*RAD_TO_DEG)
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define D6 43
#define D7 44
#define SERIAL_8N1 0
#define INT32_MIN_  (-2147483647-1)

struct __FlashStringHelper;
#define F(x) (reinterpret_cast<const __FlashStringHelper*>(x))

class String {
 public:
  std::string s;
  String() {}
  String(const char* c) : s(c ? c : "") {}
  String(const std::string& x) : s(x) {}
  String(const __FlashStringHelper* f) : s((const char*)f) {}
  String(char c) : s(1, c) {}
  String(int v) : s(std::to_string(v)) {}
  String(unsigned v) : s(std::to_string(v)) {}
  String(long v) : s(std::to_string(v)) {}
  String(unsigned long v) : s(std::to_string(v)) {}
  String(unsigned char v) : s(std::to_string((int)v)) {}
  String(short v) : s(std::to_string((int)v)) {}
  String(float v, int d = 2) { char b[40]; snprintf(b, sizeof b, "%.*f", d, v); s = b; }
  String(double v, int d = 2) { char b[40]; snprintf(b, sizeof b, "%.*f", d, v); s = b; }
  const char* c_str() const { return s.c_str(); }
  size_t length() const { return s.size(); }
  char operator[](size_t i) const { return s[i]; }
  void reserve(size_t n) { s.reserve(n); }
  String& operator+=(const String& o) { s += o.s; return *this; }
  String& operator+=(const char* o) { s += o; return *this; }
  String& operator+=(char c) { s += c; return *this; }
  bool concat(const char* c, size_t n) { s.append(c, n); return true; }
  bool concat(const char* c) { s += c; return true; }
  bool concat(char c) { s += c; return true; }
  bool startsWith(const char* p) const { return s.rfind(p, 0) == 0; }
  bool operator==(const char* o) const { return s == o; }
  bool operator==(const String& o) const { return s == o.s; }
  bool operator!=(const char* o) const { return s != o; }
  size_t find(const char* p) const { return s.find(p); }
  String substring(size_t a) const { return String(s.substr(a)); }
  String substring(size_t a, size_t b) const { return a >= s.size() || b <= a ? String("") : String(s.substr(a, b - a)); }
};
inline String operator+(const String& a, const String& b) { return String(a.s + b.s); }
inline String operator+(const char* a, const String& b) { return String(std::string(a) + b.s); }
inline String operator+(const String& a, const char* b) { return String(a.s + b); }

extern uint64_t g_simUs;
void sim_advance_us(uint64_t us);
inline uint32_t millis() { return (uint32_t)(g_simUs / 1000); }
inline void delay(uint32_t ms) { sim_advance_us((uint64_t)ms * 1000ULL); }
inline void delayMicroseconds(uint32_t us) { sim_advance_us(us); }
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return 0; }

struct SerialUSB {
  void begin(unsigned long) {}
  void setTxTimeoutMs(int) {}
  explicit operator bool() const { return true; }
  int available() { return 0; }
  int read() { return -1; }
  bool quiet = true;
  void print(const char* c) { if (!quiet) fputs(c, stdout); }
  void print(const String& c) { print(c.c_str()); }
  void print(const __FlashStringHelper* c) { print((const char*)c); }
  void print(char c) { if (!quiet) putchar(c); }
  void print(int v) { if (!quiet) printf("%d", v); }
  void println() { print("\n"); }
  void println(const char* c) { print(c); print("\n"); }
  void println(const String& c) { print(c); print("\n"); }
  void println(const __FlashStringHelper* c) { print(c); print("\n"); }
  void printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    if (quiet) return;
    va_list a; va_start(a, fmt); vprintf(fmt, a); va_end(a);
  }
};
extern SerialUSB Serial;

#include "HardwareSerial.h"
