#pragma once
#include "Arduino.h"
extern std::map<std::string, std::vector<uint8_t>> g_nvs;
class Preferences {
  std::string ns; bool ro = false;
  std::string K(const char* k) { return ns + "/" + k; }
  template <class T> void put(const char* k, T v) { auto& b = g_nvs[K(k)]; b.assign((uint8_t*)&v, (uint8_t*)&v + sizeof(T)); }
  template <class T> T get(const char* k, T d) { auto it = g_nvs.find(K(k)); if (it == g_nvs.end() || it->second.size() != sizeof(T)) return d; T v; memcpy(&v, it->second.data(), sizeof(T)); return v; }
 public:
  bool begin(const char* n, bool r = false) { ns = n; ro = r; return true; }
  void end() {}
  void clear() { for (auto it = g_nvs.begin(); it != g_nvs.end();) { if (it->first.rfind(ns + "/", 0) == 0) it = g_nvs.erase(it); else ++it; } }
  float getFloat(const char* k, float d = 0) { return get<float>(k, d); }
  size_t putFloat(const char* k, float v) { put(k, v); return 4; }
  int8_t getChar(const char* k, int8_t d = 0) { return get<int8_t>(k, d); }
  size_t putChar(const char* k, int8_t v) { put(k, v); return 1; }
  uint8_t getUChar(const char* k, uint8_t d = 0) { return get<uint8_t>(k, d); }
  size_t putUChar(const char* k, uint8_t v) { put(k, v); return 1; }
  uint16_t getUShort(const char* k, uint16_t d = 0) { return get<uint16_t>(k, d); }
  size_t putUShort(const char* k, uint16_t v) { put(k, v); return 2; }
  int16_t getShort(const char* k, int16_t d = 0) { return get<int16_t>(k, d); }
  int32_t getInt(const char* k, int32_t d = 0) { return get<int32_t>(k, d); }
  size_t putInt(const char* k, int32_t v) { put(k, v); return 4; }
  size_t putShort(const char* k, int16_t v) { put(k, v); return 2; }
  size_t putBytes(const char* k, const void* v, size_t n) { auto& b = g_nvs[K(k)]; b.assign((const uint8_t*)v, (const uint8_t*)v + n); return n; }
  size_t getBytes(const char* k, void* v, size_t n) { auto it = g_nvs.find(K(k)); if (it == g_nvs.end() || it->second.size() != n) return 0; memcpy(v, it->second.data(), n); return n; }
};
