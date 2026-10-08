# Operating the arm

How I bring the arm up, calibrate it and teach it. Read [SAFETY.md](SAFETY.md) first.

## Connect

1. Power the arm from the 12 V switch. It boots **disarmed**; nothing moves.
2. It tries the hotspot in `secrets.h` for 12 s. If that isn't there, it starts its own network, **`ARACHNAE`**, at
   **`http://192.168.4.1/`**. Every service starts either way.
3. Open the landing page: live status plus links to **Teach & Control** (`/teach`), **Tests** (`/test`), the camera
   stream and the servo diagnostics.

My working setup: the laptop's Wi-Fi on the arm's `ARACHNAE` network, and my phone USB-tethered to the laptop for
internet. (My phone's own hotspot isolates its clients, so the laptop can't reach the arm through it.) A phone hotspot
also changes its subnet on every restart, so either use `arachnae.local` or the fixed `192.168.4.1`.

## First bring-up

1. Open `/teach/diag`: every servo should answer with mode 0, torque on, goal = present position, ~12 V.
2. Press **Arm**. Keep the speed scale at **10 %**.
3. Jog each joint a few degrees. Check it moves the way the firmware thinks; flip any that don't in **Joint calibration**.
4. **Jog** into the home pose (hanging plumb, link 1 swung 30° back) and press **Set this position as 0**. Never place
   the arm by hand.
5. Measure the gear ratios: jog a joint 90° and check with a phone inclinometer. Set ratio, sign, backlash and torque
   limit in **Joint calibration**; they save to flash and don't invalidate taught poses.

## Teach

Jog to each of the 7 poses and press **Set**: home, forward reach, handoff, left limit, right limit, max height,
max depth. Check each with **Go** before moving on. The gripper's open and closed positions are taught the same way.
All 7 are needed for auto mode, and together they define the envelope IK is allowed to use.

## Demo sequence (no CV, no base needed)

On `/teach` → **Demo sequence**: jog to a pose, choose a gripper action (keep / open / close until contact) and a pause,
**Add step**, repeat (up to 16), then **Play**. Stop, E-STOP, any fault, teach or auto mode end playback and the arm holds.

## Tests page

- **Base spin**: runs the base as a free motor at 10 to 100 % until switched off. I used it to watch the gear mesh and,
  with its servo-turn counter, to measure the real base ratio. Afterwards the base angle is re-referenced and is
  meaningless until re-zeroed.
- **Camera**: image settings (brightness, flip, ...) saved on the arm.
- **Computer vision**: what the pipeline sees and whether IK could reach it. **Approach only** is the single CV-driven
  motion it allows, and only with everything taught and a fresh, confident detection.

## Updating the firmware over Wi-Fi

With the arm **resting** (an update cuts torque):

```bash
python3 ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py \
  -i 192.168.4.1 -p 3232 -a <OTA_PASSWORD> -f .pio/build/main_fw/firmware.bin -r
```
