#pragma once
#include "Arduino.h"
enum http_method { HTTP_GET = 1, HTTP_POST = 3 };
typedef http_method HTTPMethod;
class WebServer {
 public:
  typedef std::function<void()> THandlerFunction;
  std::map<std::string, THandlerFunction> h;
  String body; int code = 0; String ctype, resp;
  WebServer(int) {}
  void on(const char* p, HTTPMethod m, THandlerFunction f) { h[std::string(m == HTTP_GET ? "G " : "P ") + p] = f; }
  void begin() {}
  void handleClient() {}
  bool hasArg(const char* a) { return !strcmp(a, "plain") && body.length() > 0; }
  String arg(const char*) { return body; }
  void send(int c, const char* t, const String& s) { code = c; ctype = t; resp = s; }
  void send(int c, const char* t, const char* s) { code = c; ctype = t; resp = s; }
  void send_P(int c, const char* t, const char* s) { code = c; ctype = t; resp = s; }
  // test hook
  String request(bool post, const char* path, const char* b = "") {
    body = b; code = 0; resp = "";
    auto it = h.find(std::string(post ? "P " : "G ") + path);
    if (it == h.end()) return "NOROUTE";
    it->second();
    return resp;
  }
};
