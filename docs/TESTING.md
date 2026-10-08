# Testing

## Why a simulator

Testing on the real arm is slow, risky (it is a 2 kg arm on a person) and has no rewind. So before any firmware goes
near the arm, it runs in a hardware-free simulator on my laptop.

[`firmware/tools/host_sim/test_main.cpp`](../firmware/tools/host_sim/test_main.cpp) `#include`s the real sketch,
`Final_Firmware_Main.cpp`, unchanged, and links it against fake `Arduino.h`, `WiFi.h`, `WebServer.h`,
`esp_camera.h` and `Preferences.h` headers plus `sim.cpp`, a simulated ST3215 bus with servo physics: position,
speed, acceleration, the Mode 0 wrap, wheel mode, the reverse bit, torque and load, temperature, and the status reply
every servo sends after a write.

```bash
cd firmware
tools/host_sim/run_tests.sh       # 940 passed, 0 failed
./tools/host_sim/fwtest           # every section, verbose
```

The first run clones ArduinoJson into `tools/host_sim/ArduinoJson` (it needs the network once). A second binary,
`setidtest`, covers the servo ID tool.

## What it covers

The whole boot sequence runs twice, with the bus **echo on and off**, because the resistor-splice echo is the
hardest thing for the bus driver to get right. Then 29 sections:

| # | Section |
|---|---|
| 1–2 | boot tracks encoders, holds in place, disarmed; nothing powered moves while disarmed |
| 3–4 | jogs across the encoder wrap, backlash reversal, coordinated multi-joint moves over many revolutions |
| 5–6 | hard limits clamp commands; E-STOP cuts torque, keeps tracking, and reset holds with no snap-back |
| 7–9 | teach 7 poses, survive a reboot, replay; a wrong boot pose is flagged and needs an explicit override |
| 10–11 | a wrong wheel-direction bit is detected and the joint holds; lost encoder feedback mid-move E-STOPs |
| 12–13 | the taught envelope limits IK while the body model still applies; calibration edits don't invalidate taught poses |
| 14–15 | WebSocket gating; a full autonomous grab from a tool-frame target |
| 26–29 | reverse-bit edge cases; never re-energise to a stale goal; spring-back and hold dither at the wrap; corrupted and foreign reply frames are rejected |
| 30–31 | the gripper on its own junction makes five servos work where one junction drives only four; gripper jogs and taught open/closed |
| 32 | one bad temperature reading warns, a sustained one E-STOPs |
| 33 | the base spin test: free motor, live speed changes, clean stops, re-referencing |
| 34 | the CV panel never moves the arm, and its IK verdict matches the state machine's |
| 35–36 | the worn-mount frames; runtime backlash and torque limits; the faster base stall cut-off |
| 37–39 | the handoff actually releases; the demo sequence; CV proof capture never moves, arms or enters auto |

The count grew from 254 checks (Sep 20) to 940 (Oct 6) because I added a regression section for every bug found on
the real arm, written so it fails on the old code first.

## Making the simulator realistic enough to catch real bugs

An ideal simulated servo hid the worst bug I had. On the first wear test the base and elbow drove "the long way round"
at the encoder wrap; all 254 checks had passed, because the simulated servo never moved while its torque was off. I
added two switches that model the real drivetrain:

- `springBack`: the gearbox unwinds a few steps when torque drops for a mode change.
- `holdDither`: Mode 0 hunts around its goal.

Section 28 runs the jog and leapfrog paths with both on, from 0 to 400 steps of unwind. It fails on the old code and
passes on the new.

The simulator also models the bus physically: each UART has its own receive buffer, `SimServo::port` says which
junction a servo is on, and `g_sim.junctionLimit` reproduces the four-servo limit I measured on the real bus.

## What it can't prove

- Real ST3215 behaviour: reverse-bit quirks, dead band, mode-switch timing.
- My real gear ratios and joint directions (every `[VERIFY]` constant in `config.h`).
- Real load and temperature numbers, Wi-Fi range, mechanical failures.

Green tests mean the firmware does what I meant. Only the arm can tell me whether what I meant is right.

## On the real arm

- `/teach/diag` dumps raw registers of every servo. It separates "the firmware sent a command" from "the servo accepted
  it", which is how I found the bus-collision bug.
- `firmware/Final_Firmware_Main/diag_watch.py <ip>` prints only changes in the diagnostics, live, which I used to
  find intermittent connections.
- `wsLog()` streams every log line over the WebSocket; `wslog.py` tails it.
