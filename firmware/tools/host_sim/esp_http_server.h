#pragma once
#include "esp_camera.h"
#include "WebServer.h"
typedef void* httpd_handle_t;
struct httpd_req_t {};
struct httpd_config_t { int server_port; int max_uri_handlers; int core_id; };
#define HTTPD_DEFAULT_CONFIG() httpd_config_t{80, 8, 0}
struct httpd_uri_t { const char* uri; http_method method; esp_err_t (*handler)(httpd_req_t*); void* user_ctx; };
inline esp_err_t httpd_resp_set_type(httpd_req_t*, const char*) { return 0; }
inline esp_err_t httpd_resp_set_hdr(httpd_req_t*, const char*, const char*) { return 0; }
inline esp_err_t httpd_resp_send_chunk(httpd_req_t*, const char*, size_t) { return 0; }
inline esp_err_t httpd_resp_send(httpd_req_t*, const char*, size_t) { return 0; }
inline esp_err_t httpd_resp_send_500(httpd_req_t*) { return 0; }
inline esp_err_t httpd_start(httpd_handle_t*, const httpd_config_t*) { return 0; }
inline esp_err_t httpd_register_uri_handler(httpd_handle_t, const httpd_uri_t*) { return 0; }
