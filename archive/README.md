# Archive

Older code, kept so the history of the project is visible. None of it runs the current arm. The current code is in
[`firmware/`](../firmware) and [`vision/`](../vision).

| Folder | Period | What it was | Why I moved on |
|---|---|---|---|
| `v1-arduino/` | Nov 2025 to Apr 2026 | V1: an Arduino Uno R4 WiFi driving a NEMA 23 stepper (TB6600 driver) for the base, DS3218 servos for the arm and MG996R servos pulling rubber-band tendons in the hand, through a PCA9685 board. Includes the wiring schematic and the V1 bill of materials. | I wanted servos that report their own position and have a much higher torque ceiling. The ST3215 serial bus servo does both, and its angle can be read at any time, so the arm no longer has to guess where it is at power-up. |
| `v2-esp32/` | May to Aug 2026 | The first V2 code: ESP32 arm firmware with IK and a teach page, a separate ESP32-CAM streaming sketch, and laptop tools (`arachnae_setup.py` set servo IDs and modes through a Waveshare servo driver board over USB; `cv_pipeline.py` was the first YOLO detector). | I merged the camera onto the arm's own XIAO ESP32-S3 Sense, replaced the Waveshare driver board with a two-resistor splice on the bus after the board stopped talking to the servos in mid-September, and rebuilt the servo layer around non-blocking motion and verified writes. |
| `v2-base-multiturn-test/` | Sep 2026 | The Python bench script I used to prove the ST3215 could turn continuously and to track it across revolutions, with its own changelog. | Its approach (Mode 0 plus a software revolution counter plus wheel-mode legs) went into the firmware's `servo_control.h`. |

Credentials in these files are replaced with placeholders.
