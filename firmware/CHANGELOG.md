# Firmware changelog

Newest first. Builds are named by what they added (`suite-N-...`). The test count is the host simulator's
(`tools/host_sim/run_tests.sh`).

## 2026-10-08: public release

- The OTA password moved from the sketch into `secrets.h` (gitignored), with a placeholder in `secrets.h.example`.
  The simulator falls back to `tools/host_sim/secrets.h`, so a fresh clone runs the tests.
- Removed four PlatformIO environments whose source folders no longer exist.
- 940 checks pass; `main_fw` builds at 77.4 % flash.

## 2026-10-06: CV proof capture

- `/test` → Computer vision gets a **record** trigger: `POST /test/cv/capture {"secs":3-120}`. The firmware only
  forwards it to the laptop agent that announced itself (`agent_hello`, role `capture`); it never moves, arms or
  changes mode. `vision/cv_capture_agent.py` does the recording.
- WebSocket `hello` now reports the build name.

## 2026-10-04: base torque cap (`baseCapTick()`)

- While the base moves, its load is read every tick. At ≥ 95 % of its torque limit for 200 ms, a move faster than
  680 steps/s drops to that speed; one already that slow stops and holds, with no fault. Re-armed by every new base
  move or spin change, with a 300 ms acceleration grace. 917 checks (23 new).
- Why: the printed base gears were breaking under shock load, and I wanted the base to ease off before the limit
  instead of stalling against it.

## 2026-10-03: `suite-5-mount`, the worn mount (894 checks)

- **Two frames.** The arm frame for FK/IK; a body frame (+X forward, +Y left, +Z up) for the body model, "lift", tool
  pitch and the height envelope. The mount rotation comes from a photo of the arm worn: the base axis points straight
  back out of the right shoulder blade, level. HOME is now (0, 30, −30, 0): hanging plumb, link 1 swung 30° back.
- **Body model re-estimated**: torso, my own right arm, head, neck, both shoulders. Every link is checked against the
  torso.
- **Geometry from the CAD**: L1 150 → 189.2 mm, L3 350 → 475 mm (wrist to grasp centre), forearm ratio 2.5 → 2.25,
  base ratio 20 → 50.
- **Per-joint backlash and torque limit**, editable at runtime. Base backlash default 4096 → 0; base torque limit 350
  (others 700) to spare the printed gears. Base stall cut-off 500 ms (others 1 s). A failed read is never judged a stall.
- **Handoff fix.** It used to wait for the load to drop and then *close*, so it never let go. Now it holds the object
  out for 3 s, opens, waits 1.5 s and homes.
- **Demo sequence** on `/teach`: up to 16 jogged poses with a gripper action and a pause each, played back
  non-blocking. The fallback grab that needs neither CV nor the base.

## 2026-10-02: `suite-4-cv-ch13`

- **Base spin test is a free motor**: wheel mode at 10–100 % of the wheel ceiling, no target and no angle limit, live
  speed changes, a servo-turn counter. When a spin ends the base is re-referenced so the limit watchdog doesn't trip.
  E-STOP, encoder loss, the stall watchdog and the monitor all still apply. Why: I needed to watch the worm mesh turn
  continuously in one direction and to measure the real base ratio.
- **`WIFI_COUNTRY "AE"`** so channels 12–13 work. My phone's hotspot was on channel 12, which the ESP32's default
  channel set never joins.
- Camera image settings (`/test/cam`) are saved in flash and re-applied at boot.

## 2026-09-30: `suite-2-basetest` and `suite-3-cvtest`

- **`/test` page** with the base spin test.
- **Backlash fix.** The 4096-step backlash offset used to be applied when a reversal was *commanded*, so the base read
  18° past where it was, and any reversal within ~18° of a limit E-STOPped the whole arm. Backlash is now taken up as
  the servo actually turns through it (`blGoal` / `blOff`).
- **CV test panel.** The pipeline sends `cv_obs` (~5 Hz) in dry run; the firmware converts it, runs the same IK as the
  state machine, and shows the verdict. It never moves the arm. `/test/cv/approach` (approach point only, frozen target,
  fully gated) is the only CV-driven motion.

## 2026-09-28: temperature strikes

- A temperature E-STOP now needs 3 consecutive over-limit readings, and each high reading is logged with the load.
  Why: one "67 °C" reading from a cold elbow cut torque during a jog.

## 2026-09-27: the gripper gets its own bus; reads validated (591 checks)

- **Finding:** with all five servos on one resistor junction the bus fails: base 0 % and elbow 0 % clean reads,
  shoulder ~5 %, gripper ~35 %, forearm ~92 %. The base doesn't answer at boot, so the firmware refuses to arm. Four
  joints alone and the gripper alone are both clean, even with the ESP32 wire shaken.
- **Fix:** the gripper moves to a second junction on `Serial2` (D4/D5), wired the same way (`GRIPPER_OWN_BUS`,
  `busFor(id)`).
- **Fix:** `_read()` rejects replies with the wrong ID, length or checksum. It used to accept any frame starting
  `FF FF`, so the gripper's torque limit read as −14316, and a garbled position read could have moved a revolution
  tracker.
- **Fix:** the gripper's torque limit is 350; the boot loop was overwriting it with the joints' 700.

## 2026-09-27: first wear test, wheel legs reversing at the encoder wrap (558 checks)

- **Symptom:** the base and elbow went some way, went back, then went some way again.
- **Cause:** Mode 0 is single-turn. The wheel leg handed off the instant the tracker saw the target's revolution, at
  raw 0–3. A few steps of unwind while torque was off for the mode write put the servo back across the wrap, the Mode 0
  goal was then ~4000 steps the other way, and it drove backwards until the watchdog aborted. The simulated servo was
  ideal, so all 254 checks passed.
- **Fix:** the wheel leg runs 256 steps past the wrap before handing off; the position is re-read after every mode
  switch; a settle in the wrong revolution re-runs the wheel leg (bounded, margin doubling); goals stay 12 steps off the
  wrap; the settle watchdog measures progress toward the target.
- **Simulator:** `springBack` and `holdDither` model the real drivetrain; the new section fails on the old code.

## 2026-09-25: the reverse bit

- Fixed the wheel-mode reverse-bit handling and its persisted value, which ran jogs that crossed a revolution boundary
  in reverse the wrong way. Pushed over OTA.

## 2026-09-21: the arm is always reachable, and a watchdog gap

- **Settle watchdog.** The no-progress watchdog covered the wheel phase but not settle. A settle that never converged
  locked out every future command with no fault. Both phases are covered now.
- **Network.** `secrets.h` was never included, so the access-point fallback never existed, and every network service
  started only inside the "hotspot connected" branch: without the hotspot the arm had no control page and no OTA. Now
  the AP fallback is real (fixed `192.168.4.1`) and every service starts unconditionally. mDNS `arachnae.local`,
  auto-reconnect, Wi-Fi sleep off.
- **Landing page** at `/` with live status and links.
- **Boot no longer pretends to move:** it says plainly that it is disarmed and waiting.

## 2026-09-20: first powered bring-up

**All four joints reported healthy, held position, answered every read, and would not move.**

- **Root cause: back-to-back bus writes destroyed each other.** Every write is answered by a status packet, and on the
  resistor bus the ESP32 also hears its own echo. Nothing consumed either, so the reply collided with the next outgoing
  packet. A move writes speed then position: the speed landed, the position vanished. At boot, torque limit then torque
  enable: the enable only landed on ID 1, the base, the only joint that ever moved. Fixed with `_afterWrite()`: wait out
  the reply and discard it. Confirmed on hardware: `goal_pos` read 0 on every joint before, the right target after.
- **Verified writes** for torque on, torque off and mode: write, read back, retry. Measured on the arm: three joints
  read `torque=0` while the status said `powered:true` for all four.
- **Torque limit rewritten on every re-energise.** The ST3215 clamps it when its overload protection trips, and a
  stalled joint couldn't be recovered without a reboot.
- **Grab state machine:** the grip step blocked for ~3.3 s without pumping the safety monitor. Fixed.
- **Taught data was never loaded at boot.** Fixed.
- **New `/teach/diag`**: raw registers of every servo. It is what found the root cause.
- **New "Set this position as 0"** (`/teach/setzero`), because I never move the arm by hand, so I can't power up in a
  hand-placed pose.
- **OTA works**: `huge_app.csv` has no second app slot and crashed every update; switched to `default.csv`.
- **Remote log** (`wsLog()`) over WebSocket, since USB is unreachable once the board is in the arm.

## 2026-09-20: rebuilt the servo layer for the resistor bus (254 checks)

- **Bus.** The old firmware had a compile-time switch between the Waveshare driver board and the resistor splice. The
  new driver handles both: it drains stale bytes, skips a leading echo of its own request, and parses whatever real
  reply follows. The simulator runs everything with echo on and off; the first rewrite failed the echo-on half.
- **Base: open-loop timed spin → closed-loop tracking.** The old base spun in wheel mode for a calculated time at a
  calibrated speed, with no feedback, and refused to move until a calibration constant was measured. Now every joint's
  encoder is read every 25 ms into a multi-revolution absolute count, and long moves leapfrog through wheel mode with
  direction and progress checked every tick.
- **Registers.** Present/goal position are 2-byte registers. My rewrite briefly wrote them with the 4-byte API, which
  would have corrupted the adjacent goal-time register. Back to 2-byte reads and writes everywhere. Load is at 0x3C,
  pushing voltage to 0x3E and temperature to 0x3F; an earlier version read temperature from the voltage register.
- **Teach data** stored as steps from the boot reference instead of angles, with a bitmask for which of the 7 slots are
  set; "calibrated" needs all 7. A boot-pose signature blocks arming after waking up in a different pose. The taught
  limits now actually constrain IK.
- **Arm gate, non-blocking motion, per-joint watchdogs, an E-STOP that broadcasts and then verifies, re-energise at the
  current position, runtime joint calibration.**
- **Build:** PlatformIO only discovers a `.ino` in the project root, so the sketch inside its folder was never compiled.
  Renamed to `Final_Firmware_Main.cpp` and switched every environment to `build_src_filter`.

## 2026-09-08 to 09-17: before the rewrite (`archive/v2-base-multiturn-test/`)

- The ST3215's multi-turn Mode 3 accepts the mode write and reports it, but the servo still behaves single-turn. I
  switched to Mode 0 with a software revolution tracker.
- The EEPROM lock register is at 0x37, not 0x30. Writing the wrong one meant mode changes never reached flash.
- Proved the servo turns continuously with no hard stop, which made the multi-turn approach possible.
- About one servo revolution of slack at the base on every reversal; added backlash compensation.
- The teach page showed blank angles: a `NaN` from a corrupted register broke `JSON.parse`. Gone with Mode 0.
