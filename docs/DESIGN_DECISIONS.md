# Design decisions

The decisions that shaped the arm, why I made them, and how each turned out. The dated, day-by-day version is in
[TIMELINE.md](../TIMELINE.md).

---

### 1. A worn arm instead of a socket prosthetic

**Context.** I started this for a friend who doesn't have an arm. Powered prosthetic arms cost more than many people
earn in a year, and many users give them up.
**Decision.** Build an arm that is worn on a back plate and reaches around the body, instead of one that attaches to
a residual limb.
**Why.** It doesn't need a custom-fitted socket, it can be carried by the back, and anyone can try it on, which made it
something I could actually build and test. <!-- CONFIRM: reason inferred, check wording -->
**Outcome.** The arm has been worn, on me and held against my father, and has picked up and released an object.

### 2. V1 → V2: serial bus servos instead of hobby servos and a stepper

**Context.** V1 used an Arduino Uno R4, a NEMA 23 stepper for the base, DS3218 servos for the arm and MG996R servos
pulling tendons in a five-finger hand.
**Decision.** Rebuild around ST3215 serial bus servos.
**Why.** Hobby PWM servos can't report where they are. The ST3215 reports position, load, voltage and temperature at
any time and has a much higher torque ceiling, so the arm can know its own pose at power-up and protect itself.
**Outcome.** The right call. Almost every safety feature in the firmware depends on reading the servos back.

### 3. One board for everything

**Decision.** A Seeed XIAO ESP32-S3 Sense runs the servos, the camera, Wi-Fi and the web pages. Early V2 had a separate
ESP32-CAM in the hand.
**Why.** One board means one power supply, one firmware, one network address, and a camera already wired to the
computer that drives the arm. <!-- CONFIRM: reason inferred, check wording -->
**Outcome.** Works. The cost is that everything shares one CPU, which is why all motion is non-blocking (decision 7).

### 4. Two resistors instead of a servo driver board

**Context.** The servos share one half-duplex data wire. A Waveshare driver board handled that in hardware, until it
stopped talking to the servos in mid-September.
**Decision.** Splice the ESP32 onto the bus with a 300 Ω resistor on TX and a 10 kΩ resistor on RX.
**Why.** Two resistors fit inside the arm and need no extra power. The firmware can handle the side effects (it hears its
own echo, and writes are unacknowledged). <!-- CONFIRM: reason inferred, check wording -->
**Outcome.** It works, but it made the bus driver the hardest part of the firmware. See decisions 5 and 8.

### 5. A second bus junction for the gripper

**Context.** With all five servos on one junction, the base and elbow never answered. I first suspected loose wiring
and spent two days on it; the logs, and four joints running cleanly while the wire was shaken, said otherwise.
**Decision.** Put the gripper on its own junction and UART (`Serial2`), wired exactly like the first.
**Why.** It was the cheapest, fastest fix: two resistors and a firmware change, no new parts.
**Outcome.** All five servos give clean reads (20 of 20 diagnostic polls after the update).

### 6. Multi-turn motion done in software

**Context.** The joints need many servo turns, but the ST3215's position mode is single-turn and its multi-turn mode
reports as active while still behaving single-turn.
**Decision.** Stay in single-turn position mode, track revolutions in software, and cross revolutions in wheel mode
("leapfrogging"), handing back to position mode for the final approach.
**Why.** It is the only combination I could verify on the real servo.
**Outcome.** Works, after a hard lesson about the encoder wrap (see [LESSONS_LEARNED.md](LESSONS_LEARNED.md)).

### 7. Non-blocking motion everywhere

**Decision.** Commands only start moves; a per-joint state machine advances them every tick.
**Why.** The old design waited inside motion loops, which froze the safety monitor and the network for the whole move.
On a worn arm, the monitor must never stop.
**Outcome.** The grab state machine, the demo player and the web pages all run while the arm moves.

### 8. Trust nothing the bus says, verify anything that matters

**Decision.** Wait out and discard the reply after every write; reject replies with a wrong ID, length or checksum;
read back and retry torque and mode writes.
**Why.** Each rule came from a real failure: writes destroying each other, garbage values in the trackers, the firmware
believing limp joints were powered.
**Outcome.** The bus has been reliable since.

### 9. Never backdrive a servo, so zero it in software

**Decision.** The arm is never moved by hand. Every pose is jogged into, and **Set this position as 0** defines the
reference.
**Why.** Forcing an ST3215's shaft makes its motor a generator and can kill its driver board.
**Outcome.** The whole calibration and teaching workflow runs with torque on.

### 10. Taught poses stored as servo steps from the zero reference

**Why.** Angles depend on gear ratios I hadn't measured yet. Steps don't, so correcting a ratio later never
invalidates what I've taught.

### 11. IK limited by what I've taught and by a model of my body

**Decision.** IK refuses targets outside the envelope of the taught poses and any pose where a link would hit my torso,
head, neck, shoulders or right arm.
**Why.** A wrong gear ratio or a bad CV target should fail safely, not swing a 2 kg arm into my head.

### 12. A self-locking worm for the base

**Decision.** Keep a worm stage in front of the cycloid even when it fought me.
**Why.** It self-locks, so when torque is cut the arm holds instead of dropping and backdriving the servo.
**Outcome.** Six rounds of redesign; not solved yet. The full story is in [BASE_GEARBOX.md](BASE_GEARBOX.md).

### 13. Header-only firmware and a simulator

**Decision.** Every module is a header, so a PC test harness can `#include` the exact sketch and run it against a
simulated servo bus.
**Why.** Real-arm testing is slow and risky. The simulator lets me reproduce a hardware bug, prove the fix, and keep it
fixed. It grew from 254 to 940 checks.

### 14. Vision on the laptop

**Decision.** Run YOLO on the laptop and send targets to the arm over WebSocket.
**Why.** The ESP32 can't run a detector at a useful rate, and my phone's hotspot blocks the phone from reaching the arm.
**Outcome.** ~60 ms per frame on the CPU, faster than the camera delivers frames.

### 15. Dry run by default, and never re-enable auto silently

**Decision.** The pipeline only observes unless started with `--auto`; in auto it exits on a disconnect.
**Why.** The firmware cancels auto mode and homes when the link drops. A pipeline that reconnected and resumed would
switch it back on without anyone deciding to.

### 16. Accept "vase" as a bottle

**Why.** Testing showed YOLO calls my demo bottle a vase far more often than a bottle. Matching what the model actually
sees beat retraining it.

### 17. Keep the gripper as the demo, not handwriting

**Context.** On Sep 27, with the base unreliable, I considered pivoting the demo to writing with a pen module.
**Decision.** Keep grabbing as the lead demo; the pencil module stays a future tool.
**Why.** A gripper tolerates being a centimetre off because the jaws cover it. Handwriting needs about 1 mm and smooth
paths, which the arm can't do yet.

### 18. A fallback demo decided in advance

**Decision.** Before the deadline weekend I decided: if the base wasn't solid by Sunday midday, lock it and demo with the
other four joints using a scripted sequence of taught poses.
**Why.** Deciding under pressure is when bad calls get made.
**Outcome.** The base wasn't ready, so I used the fallback, which is why the demo sequence feature exists.

### 19. Safety thresholds that need more than one bad reading

**Decision.** Temperature and load trips need 3 consecutive bad readings.
**Why.** A single "67 °C" reading from a cold elbow cut torque once. A false E-STOP while worn drops the arm, which is
its own hazard.

### 20. A base that slows itself down

**Decision.** At ≥ 95 % of its torque limit for 200 ms, a fast base move drops to a slow speed, and a slow one stops and
holds.
**Why.** The printed base gears break under shock load. Easing off before the limit protects them without a fault.
