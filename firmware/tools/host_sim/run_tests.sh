#!/usr/bin/env bash
# Hardware-free simulation of the firmware + the ID changer against a fake ST3215 bus.
# Needs g++ and git.
set -e
cd "$(dirname "$0")"
[ -d ArduinoJson ] || git clone --depth 1 https://github.com/bblanchon/ArduinoJson.git ArduinoJson
FW=../../Final_Firmware_Main
g++ -std=gnu++17 -O1 -g -Wno-missing-field-initializers -I. -IArduinoJson/src -I$FW \
    -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 test_main.cpp sim.cpp sim_nvs.cpp -o fwtest
./fwtest | grep -E "FAIL|passed"
g++ -std=gnu++17 -O1 -g -Wno-missing-field-initializers -I. test_setid.cpp sim.cpp sim_nvs.cpp -o setidtest
./setidtest | grep -E "FAIL|PASSED|FAILED"
