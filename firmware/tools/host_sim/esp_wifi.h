#pragma once
#include "Arduino.h"

// Host-side stub for the ESP-IDF Wi-Fi driver. The firmware only sets the
// regulatory country (so channels 12-13 are usable); there is no radio here.
typedef int esp_err_t;
static inline esp_err_t esp_wifi_set_country_code(const char*, bool) { return 0; }
