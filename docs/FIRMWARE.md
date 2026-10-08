# Firmware

The firmware runs on a Seeed XIAO ESP32-S3 Sense inside the arm. It drives five ST3215 serial bus servos, streams the
camera, serves the phone control pages, and runs inverse kinematics and the autonomous grab state machine. Source:
[`firmware/Final_Firmware_Main/`](../firmware/Final_Firmware_Main).

## Layout

The sketch is `Final_Firmware_Main.cpp` and every module is a header, included in a fixed order:

```
config.h → kinematics.h → servo_control.h → teach_mode.h → teach_server.h
```

| File | What it does |
|---|---|
| `config.h` | every physical constant: gear ratios, joint signs, link lengths, limits, speeds, safety thresholds, the body model. Anything marked `[VERIFY]` is a physical fact the firmware can't check for itself. |
| `kinematics.h` | pure maths, no I/O: forward and inverse kinematics, the body-collision model, the body and arm frames, camera pixel to 3D point |
| `servo_control.h` | the ST3215 bus driver, the per-joint motion state machine, revolution tracking, backlash, every watchdog |
| `teach_mode.h` | the 7 taught poses, the demo sequence, re-zeroing, all stored in flash |
| `teach_server.h` | the web pages (`/`, `/teach`, `/test`) and their JSON endpoints |
| `Final_Firmware_Main.cpp` | Wi-Fi, camera, OTA, the WebSocket protocol and the grab state machine |

I kept it header-only on purpose: the host simulator (see [TESTING.md](TESTING.md)) compiles the exact same sketch on a
PC by `#include`-ing it, so there is one copy of the code and nothing to keep in sync.

## Servo bus

This is the least forgiving part of the system.

**Wiring.** The ST3215s talk half-duplex serial at 1 Mbaud on a single data wire. Instead of a driver board I splice the
ESP32's TX and RX onto that wire with two resistors: TX through 300 Ω, RX through 10 kΩ, both to one solder node.
TX idles high through its 300 Ω, so no pull-up is needed. The cost: the ESP32 hears every byte it sends echoed back on
RX, and servo writes are never acknowledged.

**Two buses.** One junction can drive at most four servos. I measured it on 2026-09-27: the four joints alone are clean
even with the wire shaken hard, the gripper alone is clean, and all five on one junction fail (the base and elbow never
answer, so the firmware refuses to arm). So the joints are on `Serial1` (D6/D7) and the gripper has its own junction on
`Serial2` (D4/D5). Every access by servo ID goes through `busFor(id)`.

**Reading safely.** `_send()` drains stale bytes before transmitting; `_read()` skips a leading echo of its own request
and then rejects any reply whose ID, length or checksum is wrong. It used to accept anything starting `FF FF`, and a
marginal bus fed garbage into the position trackers (the gripper's torque limit once read back as −14316).

**Writing safely.** Every write is answered by a status packet that nothing reads. `_afterWrite()` waits for that reply
and throws it away. Without it, the reply collides with the next outgoing packet and destroys it, so the second of any
two back-to-back writes silently vanishes. This was the bug behind the arm holding position, answering every read, and
refusing to move, with no error anywhere (see [LESSONS_LEARNED.md](LESSONS_LEARNED.md)).

**Verifying what matters.** Torque on, torque off and mode changes use `writeByteVerified()`: write, read back, retry.
I added it after the firmware reported `powered:true` on three joints that were actually limp.

## Multi-turn motion on a single-turn servo

The ST3215's position mode (Mode 0) only knows one revolution, 0 to 4095. Its multi-turn mode (Mode 3) reports itself
as active but still runs single-turn on my servos. My joints need many servo turns (the base is geared 50:1 or more), so:

- **A software revolution tracker** reads every encoder every 25 ms and folds the raw 0–4095 value into an absolute
  step count.
- **Leapfrogging.** A move longer than the current revolution first runs in wheel mode (Mode 1, continuous rotation)
  with direction and progress checked every tick, then drops into Mode 0 for the precise final approach.
- **Wrap safety.** In Mode 0, raw 0 and 4095 are opposite ends, not neighbours. A servo that dips across the wrap
  (hunting, gear unwind while torque is off for a mode change, gravity) sees its goal ~4000 steps away and drives the
  long way round. That was the "rotates some, goes back, rotates some" I saw on the first wear test. Now a wheel leg
  runs `WHEEL_HANDOFF_STEPS` past the wrap before handing off, the position is re-read after every mode switch, a
  settle that lands in the wrong revolution goes back to wheel mode (bounded, with a doubling margin), and no goal sits
  within `WRAP_GUARD_STEPS` of the wrap.
- **Non-blocking.** `armMoveToAngles()` and `jogJoint()` only start a move; `jointsTick()` advances each joint's
  `HOLD / WHEEL / SETTLE` state. Nothing blocks, so the safety monitor, the web server and the WebSocket keep running
  during every move.
- **Coordinated speed.** The joint with the furthest to go gets the commanded speed; the others are slowed so all
  arrive together. Every speed is multiplied by the safety scale (default 10 %).

Per joint, the firmware keeps the gear ratio, direction sign, zero reference, backlash and torque limit. All of them
can be changed at runtime from the Joint calibration card on `/teach` and are saved in flash, so measuring a ratio
never means reflashing or re-teaching.

**Backlash** is applied as the servo actually turns through the slack, not when the reversal is commanded. Applying it
at command time made the base read 18° off mid-reversal, and any reversal near a limit tripped the hard-limit E-STOP.

## Kinematics

Two frames share one origin, where the shoulder axis meets the base axis:

- **Arm frame** for all FK and IK: +Z along the base axis.
- **Body frame** for anything about the wearer: +X forward, +Y left, +Z up. The body model, "lift straight up", tool
  pitch and the height envelope all live here.

The mount rotation between them comes from a photo of the arm being worn: the base axis points straight back out of
the right shoulder blade, roughly level. HOME is (0, 30, −30, 0): hanging plumb, link 1 swung 30° back.

`solveIK()` returns a result type, not a bool: OK, outside the taught envelope, unreachable, or body collision are
different outcomes and callers handle them differently. Every link is checked against a model of the wearer: torso
(an elliptic cylinder), their own right arm, head, neck and both shoulders, with a 25 mm margin. IK also refuses
anything outside the envelope of the poses I have actually taught, independent of the hard joint limits.

Tool pitch is an elevation in the body frame (0 level, −90 straight down). For each target, `moveToXYZ()` tries up to
four pitches before giving up.

## Autonomous grab

`runFSM()` walks **IDLE → SCANNING → APPROACH → DESCEND → GRIP → LIFT → HANDOFF → HOME**.

- It only leaves IDLE in auto mode, which needs the arm armed, all 7 poses taught and no fault.
- SCANNING waits for a target that is recent and above 0.60 confidence.
- APPROACH and LIFT go to points straight above the target in the body frame.
- HANDOFF drives to the taught handoff pose, holds the object out for 3 s, opens the gripper, waits 1.5 s, then homes.
  (It used to wait for the load to drop and then close, so it never let go.)
- HOME does nothing when uncalibrated: the state machine must never invent a pose to drive to.

## Teaching

Seven named poses (home, forward reach, handoff, left and right limits, max height, max depth) are stored as servo
steps from the zero reference, with backlash removed. Fixing a gear ratio later therefore doesn't invalidate them.
The whole workflow is jog, then Set, with torque never dropped.

**Set this position as 0** (`/teach/setzero`) makes the current pose the reference. It exists because I never move the
arm by hand (see [SAFETY.md](SAFETY.md)), so I can't power up in a hand-placed pose.

**Demo sequence**: up to 16 steps, each a jogged pose plus a gripper action and a pause, played back without blocking.
It is the fallback grab that needs neither CV nor the base.

## Network and pages

- Wi-Fi tries the phone hotspot first, then falls back to its own access point at `192.168.4.1`. Every service starts
  either way, so the arm is never stranded without its control page or OTA.
- `arachnae.local` via mDNS. `WIFI_COUNTRY` is set so channels 12–13 work.
- Port 80: `/` (landing page), `/teach` (arm, jog, teach, calibrate, demo), `/test` (base spin test, camera settings,
  CV panel), `/teach/diag` (raw registers of every servo, the first thing I open when something misbehaves).
- Port 81: `/stream`, the MJPEG camera feed.
- Port 8765: WebSocket for targets, mode, E-STOP, status every 200 ms, and `wsLog()`, the only log once the arm is off USB.
- OTA updates, password from `secrets.h`. An OTA push cuts torque first.

## Build

```bash
cd firmware
cp Final_Firmware_Main/secrets.h.example Final_Firmware_Main/secrets.h   # then fill it in
pio run -e main_fw                  # compile
pio run -e main_fw -t upload        # flash over USB-C
tools/host_sim/run_tests.sh         # 940 hardware-free checks
```

The board uses the `default.csv` partition table. `huge_app.csv` has no second OTA slot, and OTA hard-crashes under it.
