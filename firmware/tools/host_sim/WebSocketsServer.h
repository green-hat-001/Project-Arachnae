#pragma once
#include "Arduino.h"
enum WStype_t { WStype_DISCONNECTED, WStype_CONNECTED, WStype_TEXT };
class WebSocketsServer {
 public:
  typedef std::function<void(uint8_t, WStype_t, uint8_t*, size_t)> Ev;
  Ev ev; int clients = 1; std::vector<std::string> sent; std::vector<int> sentTo;   // sentTo: client num, -1 = broadcast
  WebSocketsServer(int) {}
  void begin() {}
  void loop() {}
  void onEvent(Ev e) { ev = e; }
  int connectedClients() { return clients; }
  void broadcastTXT(const char* s) { sent.push_back(s); sentTo.push_back(-1); }
  void broadcastTXT(const char* s, size_t) { sent.push_back(s); sentTo.push_back(-1); }
  void sendTXT(uint8_t n, const char* s) { sent.push_back(s); sentTo.push_back(n); }
  void inject(const char* json, uint8_t num = 0) { ev(num, WStype_TEXT, (uint8_t*)json, strlen(json)); }
  void disconnect(uint8_t num) { ev(num, WStype_DISCONNECTED, nullptr, 0); }
};
