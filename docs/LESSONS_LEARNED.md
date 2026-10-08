# Lessons learned

The failures that taught me the most, what the symptom looked like, what was really going on, and what I changed.

---

## The arm that answered every question and wouldn't move

**Symptom (first powered bring-up, Sep 20).** All four joints reported healthy, held position, answered every read,
and refused to move. No error anywhere. Only the base ever moved.

**Cause.** Every servo write is answered by a status packet, and on my resistor bus the ESP32 also hears its own echo.
Nothing read either. The next write's pre-drain ran while that reply was still arriving, the reply collided with the
outgoing packet on the shared wire, and **the second of any two back-to-back writes silently vanished**. A move writes
speed then position: the speed landed, the position was destroyed, so each servo held its old goal. At boot the code
writes torque limit then torque enable: the enable only landed on the first servo, the base, because the bus had been
idle before it.

**How I found it.** `/teach/diag`, a page I added that reads back raw registers, showed `goal_pos` = 0 on every joint.
The firmware thought it had sent a target; the servo had never received one.

**Fix.** `_afterWrite()` waits out and discards the reply after every write. Every joint moved to its commanded angle
right after.

**Lesson.** Build the tool that shows what the hardware actually received, not what the software believes it sent.

## The joints that said they were powered

**Symptom.** `/teach/status` said `powered:true` on all four joints; three of them actually had torque off. Their
gearboxes don't backdrive, so they felt rigid by hand and nobody noticed.

**Fix.** Torque and mode writes are read back and retried, and "powered" comes from reading the register, not from
having sent a write.

**Lesson.** On a machine worn on a person, "I sent it" and "it happened" must never be confused.

## "Rotates some, goes back, rotates some"

**Symptom (first wear test, Sep 27).** The base and elbow would turn, reverse, then turn again. 254 simulated checks had
all passed.

**Cause.** In single-turn position mode, raw 0 and 4095 are opposite ends. The wheel-mode leg handed off at raw 0–3,
and a few steps of gearbox unwind while torque was off for the mode change put the servo back across the wrap. Its goal
was then ~4000 steps "away", so it drove the long way round until the watchdog stopped it. The simulator's servo was
ideal and never unwound, so the tests couldn't see it.

**Fix.** Hand off well past the wrap, re-read after every mode change, re-run the wheel leg if the settle lands in the
wrong revolution, keep goals off the wrap. I also taught the simulator to spring back and dither, and wrote a test that
fails on the old code.

**Lesson.** A simulator only catches the bugs its model can produce. Model the ugly parts of the real hardware.

## Two days hunting a loose wire that wasn't there

**Symptom (Sep 21 and Sep 27).** With the gripper connected, the base, shoulder and elbow went silent; joints flickered
on and off. Pressing on wires sometimes seemed to help.

**What I did wrong.** I chased loose connections for hours: pressing connectors, swapping a cable, resoldering. One night
I also shorted GND to 3V3 by mistake, which muddied things further.

**What it was.** One bus junction can only drive four servos. Four joints alone ran perfectly even while my father shook
the ESP32 wire; the gripper alone was fine; all five together failed. The logs already pointed there.

**Fix.** A second junction for the gripper. All five servos have been clean since.

**Lesson.** Decide what single test would settle the question, read the logs I already have, and only then touch
hardware. Unplugging and replugging connectors can create the very fault I'm looking for.

## The base: the weakest part always breaks

Covered in full in [BASE_GEARBOX.md](BASE_GEARBOX.md). The short version: each fix moved the failure to the next
weakest part (thin worm crests → the worm's overhang flank → the gear → the holder's rocking → the gear's axial
position → the cams). The useful moves were the measurements, not the guesses: noticing it only failed in one
direction, reading print-layer lines in close-ups, and measuring the holder's rotation from video against the audio of
the cracks.

**Lessons.** A worm can only reduce speed, never increase it, which I should have known before printing. Print
orientation is a design decision, not a slicer setting. Stiffness of the mount matters as much as strength of the teeth.

## Small ones

- **The XIAO wouldn't boot** with an unpowered servo board on its TX/RX lines.
- **3.3 V into the 3V3 pin does nothing**: it's a regulator output. 5 V goes into VUSB.
- **OTA crashed the board** mid-update: the `huge_app` partition table has no second app slot. `default.csv` fixed it.
- **The arm "disappeared" from Wi-Fi**: the phone hotspot had changed its subnet, my phone isolates hotspot clients,
  and once two hotspots with the same name were on at the same time. The arm now always falls back to its own network
  at a fixed address.
- **A cold servo reported 67 °C once** and cut torque. Safety trips now need three readings in a row.
- **The handoff never let go**: it waited for the load to drop and then closed the gripper. It now opens.
- **My CV frame rate was a lie**: the reader returned the same frame repeatedly.
- **The bottle was a vase**, as far as YOLO was concerned.
- **A pen-nib pin is not a key.** Two cams each pinned by their own loose nib drifted out of phase and made the
  cycloid lock up. One printed double cam, pinned once, fixed it.
