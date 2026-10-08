# Safety

This arm is worn on the body, is about 1 m long and weighs about 2 kg. Every safety rule below comes from that.

## The rules I work by

1. **The physical 12 V cutoff switch is the primary stop.** Nothing moves unless someone is at it. The firmware
   E-STOP is the backup.
2. **Never move the arm by hand.** Forcing an ST3215's output shaft turns its motor into a generator and can destroy the
   servo's internal driver board. Every pose is reached by commanding it: jog, then Set. That is why the firmware has
   *Set this position as 0* instead of "power up in a hand-placed pose", and why the old hand-teaching mode is off-limits.
3. **Nothing moves at boot.** The arm boots disarmed, every joint holding where it is, until I press **Arm**.
4. **Speed starts at 10 %.** The safety scale defaults to 0.10 and is only raised from the UI after a stage runs clean.
   I never raise the default in code.
5. **OTA cuts torque.** Updates are pushed with the arm resting, never while worn.

## What the firmware does

| Mechanism | Why |
|---|---|
| **Arm gate** (`g_armed`) | No powered motion until I confirm the arm is safe to move. A boot-pose signature (raw encoders at boot vs. at calibration) warns if the arm woke up somewhere unexpected. |
| **E-STOP** | Torque-off broadcast to every servo, then verified servo by servo, then a latched fault. The position trackers keep running, so the reported angles never go stale. |
| **Fault reset holds** | Clearing a fault re-energises each joint at its *current* position and never moves. Any re-energise writes the goal first, so nothing snaps to a stale target. |
| **Verified writes** | Torque on, torque off and mode changes are read back and retried. A dropped torque-off would leave a joint live while I think it is free. |
| **Hard joint limits** | ±8° of slack beyond each limit, checked even while holding against an outside force. |
| **Encoder-loss watchdog** | 300 ms without a successful read during a wheel-mode leg → E-STOP. A failed read is never mistaken for a stall. |
| **Wrong-way / no-motion watchdog** | Covers both the wheel and the settle phase. Aborts and holds only that joint, without latching a full E-STOP. |
| **Temperature, load, voltage monitor** | Round-robin, 3 consecutive bad readings needed. A single "67 °C" reading from a cold elbow once cut torque, and a false E-STOP while worn drops the arm. |
| **Base torque cap** | The base's torque limit is 350 (others 700) to spare the printed gears. At ≥ 95 % of the limit for 200 ms, a fast move drops to a slow speed; one already slow stops and holds. |
| **Torque limit rewritten on every re-energise** | The ST3215 clamps it when its overload protection trips, and it stays clamped until rewritten. |
| **Body-collision model in IK** | Every link is checked against a model of my torso, head, neck, shoulders and right arm. |
| **Taught envelope** | IK refuses any target outside the poses I have actually taught. |
| **CV gate** | The vision pipeline is a dry run unless started with `--auto`, and auto mode needs armed + all 7 poses taught + no fault. If the WebSocket drops, the firmware cancels auto and homes, and the pipeline exits instead of reconnecting. |

## Known gaps

- The shoulder and elbow are belt-driven and can be backdriven by gravity. An E-STOP with the arm extended lets them sag.
- Every `[VERIFY]` constant in `config.h` (gear ratios, joint directions, body dimensions) is an estimate until
  measured. Commanded angles match *reported* angles, which proves the firmware is self-consistent, not that 5°
  commanded is 5° of real rotation.
- `secrets.h` holds the OTA and access-point passwords. Anyone on the arm's network who knows them can drive or flash it.
