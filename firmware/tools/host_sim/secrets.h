#pragma once
// Placeholder credentials for the host simulator. The firmware includes
// "secrets.h"; when Final_Firmware_Main/secrets.h does not exist (a fresh
// clone), the compiler falls back to this file through -I. in run_tests.sh.
static const char* STA_SSID     = "SIM_HOTSPOT";
static const char* STA_PASS     = "SIM_PASSWORD";
static const char* AP_SSID      = "ARACHNAE";
static const char* AP_PASS      = "SIM_AP_PASSWORD";
static const char* OTA_PASSWORD = "SIM_OTA_PASSWORD";
