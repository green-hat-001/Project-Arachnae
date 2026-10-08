# Firmware

Arduino/ESP32 firmware for the Seeed XIAO ESP32-S3 Sense that runs the arm. The design is explained in
[`docs/FIRMWARE.md`](../docs/FIRMWARE.md); this page is just how to build it.

| Folder | What |
|---|---|
| `Final_Firmware_Main/` | the main firmware (`main_fw` environment) |
| `servo_setid/` | one-off tool that gives a factory servo (always ID 1) its bus ID, with only that servo connected (`servo_setid` environment) |
| `tools/host_sim/` | the hardware-free simulator, see [`docs/TESTING.md`](../docs/TESTING.md) |
| `CHANGELOG.md` | what changed and why, newest first |

## Build and flash

Needs [PlatformIO](https://platformio.org/).

```bash
cd firmware
cp Final_Firmware_Main/secrets.h.example Final_Firmware_Main/secrets.h   # Wi-Fi + OTA passwords
pio run -e main_fw                 # compile
pio run -e main_fw -t upload       # flash over USB-C
pio device monitor -b 115200       # serial log (open it right after reset)
```

## Test without hardware

```bash
tools/host_sim/run_tests.sh        # needs g++; clones ArduinoJson on first run
```

Expected: `940 passed, 0 failed` and `SETID TESTS PASSED`. Run it after every change.

## Helper scripts

- `Final_Firmware_Main/wslog.py`: tails the arm's log over WebSocket (edit the address first).
- `Final_Firmware_Main/diag_watch.py <ip>`: prints only the changes in every servo's raw registers, live.
