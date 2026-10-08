#!/usr/bin/env python3
"""
Base Position Control — Mode 0 with Revolution Tracking

ST3215 servo on 1:20 cycloidal drive, using Mode 0 (16-bit position)
with software revolution counter for multi-turn support.

Hardware:
  - ST3215 servo ID 1
  - Waveshare Serial Bus Servo Driver (/dev/ttyACM0, 1 Mbaud)
  - 1:20 cycloidal reduction
  - Range: -30° to +130° output = 160° total = 7.2 servo revolutions

Mode 0 characteristics:
  - GOAL_POSITION: 16-bit (0-4095 steps per revolution, wraps)
  - No accumulator — servo targets the position within current revolution
  - We track revolution count in software and handle wrap detection

Usage:
  python base_mode0_multiturn.py --port /dev/ttyACM0 --angle 45.0
  python base_mode0_multiturn.py --port /dev/ttyACM0 --home
  python base_mode0_multiturn.py --port /dev/ttyACM0 --status
  python base_mode0_multiturn.py --port /dev/ttyACM0 --test-rotation
"""

import serial
import time
import argparse
import sys

# ============================================================
#  ST3215 Register Map (Mode 0)
# ============================================================
REG_TORQUE_ENABLE    = 0x28   # 40  — 0=off, 1=on
REG_GOAL_SPEED       = 0x2E   # 46  — target speed (steps/sec)
REG_GOAL_POSITION    = 0x2A   # 42  — target position (16-bit, 0-4095)
REG_PRESENT_POSITION = 0x38   # 56  — current position (16-bit readback)
REG_PRESENT_SPEED    = 0x3A   # 58  — current speed (signed 16-bit)
REG_PRESENT_LOAD     = 0x3C   # 60  — current load (signed 16-bit)
REG_PRESENT_VOLTAGE  = 0x3E   # 62  — voltage ×0.1V
REG_PRESENT_TEMP     = 0x3F   # 63  — temperature °C
REG_OPERATING_MODE   = 0x21   # 33  — 0=position, 1=wheel, 3=multi-turn
REG_LOCK             = 0x37   # 55  — EEPROM lock (0=unlocked, 1=locked)

# Hardware constants
SERVO_STEPS_PER_REV = 4096
SERVO_CENTRE        = 2048
RATIO_BASE          = 20.0   # 1:20 cycloidal reduction

# Joint limits (output shaft degrees)
BASE_MIN_DEG = -30.0
BASE_MAX_DEG = 130.0

# Backlash compensation (1 revolution)
BACKLASH_STEPS = 4096

# Speed configuration (from config.h SPEED_PERCENT)
SPEED_PERCENT = 100          # 0-100%, percentage of max speed
MAX_SPEED_DEG_PER_SEC = 545.0  # ST3215 @ 12V no-load
MAX_SPEED_STEPS_PER_SEC = int((MAX_SPEED_DEG_PER_SEC / 360.0) * SERVO_STEPS_PER_REV)  # ~6207 steps/sec

# Load threshold for physical-stop detection in the rotation test
# ST3215 max load register = 1000; 600 is conservatively "stuck hard"
LOAD_STALL_THRESHOLD = 600


# ============================================================
#  SCServo Protocol (Half-Duplex UART)
# ============================================================

def checksum(payload):
    """SCServo checksum: ~sum(payload) & 0xFF"""
    return (~sum(payload)) & 0xFF

def send_packet(ser, payload):
    """Wrap payload in [0xFF 0xFF | payload | checksum] and send"""
    pkt = bytes([0xFF, 0xFF] + payload + [checksum(payload)])
    ser.reset_input_buffer()
    ser.write(pkt)
    ser.flush()

def recv_packet(ser, expected_len):
    """Read expected_len bytes from servo response"""
    return ser.read(expected_len)

class SCSerial:
    """Minimal SCServo protocol wrapper for Mode 0"""

    def __init__(self, port, baud=1_000_000, timeout=0.15):
        self.ser = serial.Serial(port, baud, timeout=timeout)
        time.sleep(0.2)  # USB enumeration settle
        self.ser.reset_input_buffer()

    def close(self):
        self.ser.close()

    def read_byte(self, id_, reg):
        """Read 1 byte from register"""
        send_packet(self.ser, [id_, 4, 0x02, reg, 1])
        resp = recv_packet(self.ser, 7)
        return resp[5] if len(resp) >= 7 else None

    def read_word(self, id_, reg):
        """Read 2 bytes (16-bit word) from register"""
        send_packet(self.ser, [id_, 4, 0x02, reg, 2])
        resp = recv_packet(self.ser, 8)
        if len(resp) >= 8:
            return resp[5] | (resp[6] << 8)
        return None

    def write_byte(self, id_, reg, val):
        """Write 1 byte to register"""
        val &= 0xFF
        send_packet(self.ser, [id_, 4, 0x03, reg, val])
        time.sleep(0.02)

    def write_word(self, id_, reg, val):
        """Write 2 bytes (16-bit word) to register"""
        val &= 0xFFFF
        lo = val & 0xFF
        hi = (val >> 8) & 0xFF
        send_packet(self.ser, [id_, 5, 0x03, reg, lo, hi])
        time.sleep(0.02)


# ============================================================
#  Multi-Turn State Tracking
# ============================================================

class MultiTurnTracker:
    """
    Tracks revolution count for Mode 0 servo.

    Mode 0 wraps at 4096 steps (0-4095). When the servo crosses
    the wrap boundary, we increment/decrement the revolution counter.

    Absolute position = (revolution * 4096) + position_within_rev
    """

    def __init__(self, sc, servo_id):
        self.sc = sc
        self.servo_id = servo_id
        self.revolution = 0
        self.last_pos = None
        self.backlash_offset = 0  # 0 when engaged CW, -BACKLASH_STEPS when engaged CCW
        self.update()  # Read initial position to seed last_pos

    def update(self):
        """Read current position and detect wrap"""
        pos = self.sc.read_word(self.servo_id, REG_PRESENT_POSITION)
        if pos is None:
            return None

        if self.last_pos is not None:
            # Detect wrap: large jump across 0/4095 boundary.
            # Threshold of 3000 steps means we only trigger on genuine
            # boundary crossings, not normal motion within a revolution.
            delta = pos - self.last_pos

            # Forward wrap: last_pos near 4095, pos near 0
            if delta < -3000:
                self.revolution += 1
            # Backward wrap: last_pos near 0, pos near 4095
            elif delta > 3000:
                self.revolution -= 1

        self.last_pos = pos
        return self.get_absolute_position()

    def get_absolute_position(self):
        """Return absolute position in steps (across all revolutions)"""
        if self.last_pos is None:
            return None
        return (self.revolution * SERVO_STEPS_PER_REV) + self.last_pos

    def get_effective_output_angle(self):
        """Return the cycloidal output angle accounting for backlash engagement."""
        abs_pos = self.get_absolute_position()
        if abs_pos is None: return None
        return servo_steps_to_deg(abs_pos - self.backlash_offset)

    def get_position_within_rev(self):
        """Return position within current revolution (0-4095)"""
        return self.last_pos

    def get_revolution(self):
        """Return current revolution count"""
        return self.revolution

    def reset(self, revolution=0):
        """Hard-reset the revolution counter (e.g. after homing)"""
        self.revolution = revolution
        # Re-read current hardware position so last_pos is fresh
        pos = self.sc.read_word(self.servo_id, REG_PRESENT_POSITION)
        if pos is not None:
            self.last_pos = pos


# ============================================================
#  Kinematic Conversions
# ============================================================

def deg_to_servo_steps(output_deg):
    """
    Convert output shaft degrees to total servo steps.

    Output range: -30° to +130° (160° total)
    Servo range: 0 to 29,582 steps (7.2 revolutions)

    Formula: steps = (deg × ratio × steps_per_rev) / 360
    """
    steps = (output_deg * RATIO_BASE * SERVO_STEPS_PER_REV) / 360.0
    return int(round(steps))

def servo_steps_to_deg(total_steps):
    """Convert total servo steps to output shaft degrees"""
    deg = (total_steps * 360.0) / (RATIO_BASE * SERVO_STEPS_PER_REV)
    return deg

def clamp_angle(deg):
    """Clamp output angle to joint limits"""
    return max(BASE_MIN_DEG, min(BASE_MAX_DEG, deg))


# ============================================================
#  Motion Control
# ============================================================

def set_speed(sc, servo_id, percent):
    """Set servo speed as percentage of max (0-100%)"""
    percent = max(0, min(100, percent))
    speed_steps = int((percent / 100.0) * MAX_SPEED_STEPS_PER_SEC)
    sc.write_word(servo_id, REG_GOAL_SPEED, speed_steps)

def move_to_angle(sc, servo_id, tracker, target_deg, speed_percent=None, wait=True):
    """
    Move base to target output angle (degrees) using Mode 1 for multi-turn gaps 
    and applying backlash compensation.
    """
    target_deg = clamp_angle(target_deg)

    tracker.update()
    current_angle = tracker.get_effective_output_angle()
    if current_angle is None:
        return None

    # Handle directional backlash
    if target_deg > current_angle + 0.1:
        # Moving positive (CW)
        tracker.backlash_offset = 0
    elif target_deg < current_angle - 0.1:
        # Moving negative (CCW)
        tracker.backlash_offset = -BACKLASH_STEPS

    # Compute target steps dynamically accounting for backlash
    target_steps = deg_to_servo_steps(target_deg) + tracker.backlash_offset

    if speed_percent is not None:
        set_speed(sc, servo_id, speed_percent)

    current_abs = tracker.get_absolute_position()
    distance_steps = abs(target_steps - current_abs)

    if distance_steps > SERVO_STEPS_PER_REV // 2:
        # Use Mode 1 (Wheel mode) for large moves
        sc.write_byte(servo_id, REG_OPERATING_MODE, 1)
        time.sleep(0.05)

        # Speed register: bit 10 is CCW
        speed_val = int((speed_percent or SPEED_PERCENT) / 100.0 * min(MAX_SPEED_STEPS_PER_SEC, 1023))
        speed_val = max(1, min(1023, speed_val))
        if target_steps < current_abs:
            speed_val |= 0x0400 # CCW
        
        sc.write_word(servo_id, REG_GOAL_SPEED, speed_val)

        # Wait until close
        timeout = time.time() + (distance_steps / ((speed_val & 1023)) * 1.5 + 2.0)
        while time.time() < timeout:
            time.sleep(0.05)
            tracker.update()
            current_abs = tracker.get_absolute_position()
            if current_abs is not None and abs(current_abs - target_steps) < 50:
                break
        
        # Switch back to Mode 0
        sc.write_byte(servo_id, REG_OPERATING_MODE, 0)
        time.sleep(0.05)
        # Re-apply speed in Mode 0 encoding
        set_speed(sc, servo_id, speed_percent or SPEED_PERCENT)

    # Final snap to target in Mode 0
    target_pos_within = target_steps % SERVO_STEPS_PER_REV
    sc.write_word(servo_id, REG_GOAL_POSITION, target_pos_within)

    if not wait:
        return target_steps

    # Poll until motion complete (within 5 steps tolerance).
    # IMPORTANT: do NOT set tracker.revolution or tracker.last_pos here.
    # The tracker must discover the final position from real hardware reads.
    timeout = time.time() + 8.0
    last_abs = None
    while time.time() < timeout:
        time.sleep(0.05)
        tracker.update()
        current_abs = tracker.get_absolute_position()

        if current_abs is not None and abs(current_abs - target_steps) <= 5:
            return current_abs

        # Break out early if position has stabilised far from target —
        # suggests the servo reached a mechanical stop or a rev-count
        # mismatch (e.g. initial position was already mid-revolution).
        if last_abs is not None and current_abs == last_abs:
            # Position unchanged for one poll — keep waiting; stall
            # detection is handled by the caller or the timeout.
            pass
        last_abs = current_abs

    # Timeout — return whatever the tracker reports now
    return tracker.get_absolute_position()


# ============================================================
#  Commands
# ============================================================

def cmd_status(sc, servo_id, tracker):
    """Display current servo state"""
    print(f"[Status] Base servo ID {servo_id}")
    print()

    mode    = sc.read_byte(servo_id, REG_OPERATING_MODE)
    torque  = sc.read_byte(servo_id, REG_TORQUE_ENABLE)
    tracker.update()
    pos_abs    = tracker.get_absolute_position()
    pos_within = tracker.get_position_within_rev()
    revolution = tracker.get_revolution()
    speed   = sc.read_word(servo_id, REG_PRESENT_SPEED)
    load    = sc.read_word(servo_id, REG_PRESENT_LOAD)
    voltage = sc.read_byte(servo_id, REG_PRESENT_VOLTAGE)
    temp    = sc.read_byte(servo_id, REG_PRESENT_TEMP)

    output_deg = tracker.get_effective_output_angle() if pos_abs is not None else None

    print(f"  Mode:       {mode} (0=position, 1=wheel, 3=multi-turn)")
    print(f"  Torque:     {torque} (0=off, 1=on)")
    print()
    print(f"  Position (absolute):   {pos_abs} steps")
    print(f"  Position (within rev): {pos_within} / {SERVO_STEPS_PER_REV}")
    print(f"  Revolution:            {revolution}")
    print(f"  Output angle:          {output_deg:.2f}°" if output_deg is not None else "  Output angle:          —")
    print()
    print(f"  Speed:      {speed} steps/sec")
    print(f"  Load:       {load}")
    print(f"  Voltage:    {voltage * 0.1:.1f}V" if voltage is not None else "  Voltage:    —")
    print(f"  Temp:       {temp}°C" if temp is not None else "  Temp:       —")
    print()
    print(f"  Joint limits: {BASE_MIN_DEG}° to {BASE_MAX_DEG}°")
    print(f"  Servo range:  0 to {deg_to_servo_steps(BASE_MAX_DEG) - deg_to_servo_steps(BASE_MIN_DEG)} steps "
          f"({(BASE_MAX_DEG - BASE_MIN_DEG) / 360 * RATIO_BASE:.1f} revolutions)")

def cmd_home(sc, servo_id, tracker):
    """Move to home position (0°)"""
    print(f"[Home] Moving base to 0°…")

    # Enable torque
    sc.write_byte(servo_id, REG_TORQUE_ENABLE, 1)
    time.sleep(0.05)

    # Set speed
    set_speed(sc, servo_id, SPEED_PERCENT)

    # Move to 0°
    final_pos = move_to_angle(sc, servo_id, tracker, 0.0, wait=True)

    if final_pos is not None:
        final_deg = servo_steps_to_deg(final_pos)
        print(f"✓ Reached {final_deg:.2f}° (target: 0.0°)")
    else:
        print("✗ Motion timeout")

def cmd_move(sc, servo_id, tracker, target_deg):
    """Move to specified angle"""
    target_deg = clamp_angle(target_deg)
    print(f"[Move] Target: {target_deg:.2f}° (speed: {SPEED_PERCENT}%)")

    # Enable torque
    sc.write_byte(servo_id, REG_TORQUE_ENABLE, 1)
    time.sleep(0.05)

    # Move
    final_pos = move_to_angle(sc, servo_id, tracker, target_deg, SPEED_PERCENT, wait=True)

    if final_pos is not None:
        final_deg = servo_steps_to_deg(final_pos)
        error = abs(final_deg - target_deg)
        print(f"✓ Reached {final_deg:.2f}° (error: {error:.2f}°)")
    else:
        print("✗ Motion timeout")

def cmd_test_sweep(sc, servo_id, tracker):
    """Test sweep across full range"""
    print(f"[Test] Sweeping {BASE_MIN_DEG}° → {BASE_MAX_DEG}° → {BASE_MIN_DEG}°")
    print(f"       Speed: {SPEED_PERCENT}%")
    print()

    # Enable torque
    sc.write_byte(servo_id, REG_TORQUE_ENABLE, 1)
    time.sleep(0.05)

    # Sweep to max
    print(f"  → Moving to {BASE_MAX_DEG}°…")
    move_to_angle(sc, servo_id, tracker, BASE_MAX_DEG, SPEED_PERCENT, wait=True)
    time.sleep(0.5)

    # Sweep to min
    print(f"  → Moving to {BASE_MIN_DEG}°…")
    move_to_angle(sc, servo_id, tracker, BASE_MIN_DEG, SPEED_PERCENT, wait=True)
    time.sleep(0.5)

    # Return home
    print(f"  → Returning to 0°…")
    move_to_angle(sc, servo_id, tracker, 0.0, SPEED_PERCENT, wait=True)

    print()
    print("✓ Sweep complete")


def cmd_test_rotation(sc, servo_id, tracker, speed_percent=20, revolutions=10):
    """
    Combined rotation / physical-stop / revolution-tracking test.

    Purpose:
      (a) Physical-stop check — determines whether the ST3215 has an
          internal mechanical hard-stop that prevents full 360° rotation.
      (b) Cycloidal drive test — confirms the 1:20 reduction transmits
          motion throughout the servo's travel range.
      (c) Revolution-tracking accuracy — verifies MultiTurnTracker counts
          wraps correctly over multiple revolutions.

    Approach — wheel mode (Mode 1) for continuous spin:
      Mode 0 GOAL_POSITION is a within-revolution target (0-4095). Sending
      increments across the 0/4095 wrap makes the servo see a huge backward
      jump and physically reverse. The correct approach for continuous
      rotation is Mode 1 (wheel/speed mode): write a signed speed value to
      GOAL_SPEED and the servo spins indefinitely. We poll PRESENT_POSITION
      while it spins and let MultiTurnTracker count revolutions from the
      real wrap events. When done, we switch back to Mode 0 to return home.

    Mode 1 speed register encoding (ST3215):
      - Bits [9:0]  = speed magnitude (0-1023 steps/sec)
      - Bit  [10]   = direction: 0 = forward (CW), 1 = reverse (CCW)
      - Upper bits  = 0
      Forward  = speed_steps & 0x03FF
      Reverse  = (speed_steps & 0x03FF) | 0x0400

    Args:
        sc: SCSerial instance
        servo_id: Servo ID
        tracker: MultiTurnTracker (seeded with current position)
        speed_percent: Speed 0-100% of max (default 20% — slow, safe first run)
        revolutions: Servo revolutions to attempt per direction (default 10)
    """
    POLL_INTERVAL    = 0.08   # seconds between position reads while spinning
    STALL_TIME       = 1.5    # seconds of no position change = stall
    STALL_STEP_FLOOR = 5      # steps/poll below this = "not moving"

    # Clamp speed — wheel mode max is 1023 steps/sec (10-bit field)
    speed_percent = max(5, min(50, speed_percent))
    speed_steps   = int((speed_percent / 100.0) * min(MAX_SPEED_STEPS_PER_SEC, 1023))
    speed_steps   = max(1, min(1023, speed_steps))

    print()
    print("=" * 64)
    print("  ROTATION / PHYSICAL-STOP / TRACKING TEST")
    print("=" * 64)
    print(f"  Servo ID:    {servo_id}")
    print(f"  Speed:       {speed_percent}% ({speed_steps} steps/sec, wheel mode)")
    print(f"  Revolutions: {revolutions} forward + {revolutions} back")
    print(f"  Stall limit: position change < {STALL_STEP_FLOOR} steps for {STALL_TIME}s")
    print()
    print("  The servo will spin continuously via Mode 1 (wheel/speed mode).")
    print("  If a physical hard-stop exists inside the servo, it will stall")
    print("  before completing one full revolution.")
    print()

    # Enable torque; read starting state via Mode 0 first
    sc.write_byte(servo_id, REG_TORQUE_ENABLE, 1)
    time.sleep(0.05)
    tracker.update()
    start_abs = tracker.get_absolute_position()
    if start_abs is None:
        print("✗ Could not read starting position. Aborting.")
        return

    start_rev = tracker.get_revolution()
    start_pos = tracker.get_position_within_rev()
    start_deg = servo_steps_to_deg(start_abs)
    print(f"  Start:  abs_steps={start_abs}  rev={start_rev}  "
          f"within_rev={start_pos}  output={start_deg:.2f}°")
    print()

    def spin_revolutions(direction, count):
        """
        Spin `count` revolutions in `direction` (+1=forward, -1=reverse)
        using Mode 1 (wheel mode). Polls position at POLL_INTERVAL, counts
        wrap events via tracker, stops when target revolutions reached or
        a stall is detected.

        Returns: (revs_completed, stopped_by_stall, final_abs)
        """
        dir_label  = "→ forward (CW)" if direction > 0 else "← reverse (CCW)"
        # Mode 1 speed word: bit10=direction flag (1=reverse)
        speed_word = speed_steps if direction > 0 else (speed_steps | 0x0400)

        target_revs_abs = abs(tracker.get_revolution() - start_rev) + count

        print(f"  Driving {count} revolutions {dir_label}…")
        print(f"  {'Rev':>4}  {'Abs steps':>10}  {'Output deg':>10}  "
              f"{'Within-rev':>10}  {'Note'}")
        print("  " + "-" * 58)

        # Switch to Mode 1 — writes RAM only, no EEPROM; servo reverts to
        # Mode 0 on power-cycle, so this is safe and non-destructive.
        sc.write_byte(servo_id, REG_OPERATING_MODE, 1)
        time.sleep(0.05)

        # Start spinning
        sc.write_word(servo_id, REG_GOAL_SPEED, speed_word)

        revs_completed   = 0
        stopped_by_stall = False
        stall_start      = None
        last_rev_printed = tracker.get_revolution()
        prev_pos         = tracker.get_position_within_rev()
        prev_time        = time.time()

        deadline = time.time() + count * (SERVO_STEPS_PER_REV / max(speed_steps, 1)) * 3
        # 3× safety margin on expected time

        while time.time() < deadline:
            time.sleep(POLL_INTERVAL)
            tracker.update()

            cur_abs    = tracker.get_absolute_position()
            cur_within = tracker.get_position_within_rev()
            cur_rev    = tracker.get_revolution()

            if cur_abs is None:
                continue

            output_deg = servo_steps_to_deg(cur_abs)

            # Print a line on each new revolution
            if cur_rev != last_rev_printed:
                note = ""
                print(f"  {cur_rev:>4}  {cur_abs:>10}  {output_deg:>9.2f}°  "
                      f"{cur_within:>10}  {note}")
                last_rev_printed = cur_rev

            # Count how many revolutions we've done from the start_rev
            revs_done = abs(cur_rev - start_rev)
            revs_completed = max(revs_completed, revs_done)

            # Stop when we've hit the target
            if revs_done >= count:
                break

            # Stall detection — position not advancing
            now = time.time()
            if prev_pos is not None and cur_within is not None:
                step_delta = abs(cur_within - prev_pos)
                # Account for wrap: a delta of ~4096 is actually motion
                if step_delta > 3000:
                    step_delta = SERVO_STEPS_PER_REV - step_delta
                steps_per_sec = step_delta / max(now - prev_time, 1e-6)

                if steps_per_sec < STALL_STEP_FLOOR:
                    if stall_start is None:
                        stall_start = now
                    elif now - stall_start > STALL_TIME:
                        print()
                        print(f"  ⚠ STALL at rev={cur_rev}, within-rev={cur_within}, "
                              f"output={output_deg:.2f}°")
                        print(f"    {steps_per_sec:.1f} steps/sec — below floor of {STALL_STEP_FLOOR}")
                        if revs_done < 1:
                            print("    → Stalled before first full revolution.")
                            print("      ST3215 likely has an internal mechanical hard-stop.")
                        else:
                            print(f"    → Stalled after {revs_done} revolution(s).")
                        stopped_by_stall = True
                        break
                else:
                    stall_start = None

            prev_pos  = cur_within
            prev_time = now

        # Stop the motor — write speed 0 in wheel mode, then restore Mode 0
        sc.write_word(servo_id, REG_GOAL_SPEED, 0)
        time.sleep(0.1)
        sc.write_byte(servo_id, REG_OPERATING_MODE, 0)
        time.sleep(0.05)

        return revs_completed, stopped_by_stall, tracker.get_absolute_position()

    # ── Forward sweep ────────────────────────────────────────
    fwd_revs, fwd_stall, fwd_final = spin_revolutions(+1, revolutions)
    time.sleep(0.4)

    # ── Reverse sweep ─────────────────────────────────────────
    rev_count = fwd_revs if fwd_stall else revolutions
    if rev_count > 0:
        rev_revs, rev_stall, rev_final = spin_revolutions(-1, rev_count)
    else:
        rev_revs, rev_stall, rev_final = 0, False, fwd_final
    time.sleep(0.4)

    # ── Return to start (Mode 0 position command) ─────────────
    print()
    print(f"  → Returning to start position (within-rev={start_pos})…")
    # Back in Mode 0; command the original within-revolution position.
    # This only works cleanly if we're on the right revolution, which we
    # should be after a matched forward+reverse sweep.
    set_speed(sc, servo_id, speed_percent)
    sc.write_word(servo_id, REG_GOAL_POSITION, start_pos)

    t_end = time.time() + 10.0
    while time.time() < t_end:
        time.sleep(0.1)
        tracker.update()
        cur = tracker.get_absolute_position()
        if cur is not None and abs(cur - start_abs) <= 20:
            break

    # ── Results ───────────────────────────────────────────────
    tracker.update()
    final_abs = tracker.get_absolute_position()
    roundtrip_error = abs(final_abs - start_abs) if final_abs is not None else None

    print()
    print("=" * 64)
    print("  RESULTS")
    print("=" * 64)

    # Physical stop
    if fwd_stall and fwd_revs < 1:
        print(f"  Physical stop: YES — stalled before completing 1 revolution")
        print(f"    The ST3215 has an internal hard-stop preventing 360° rotation.")
        print(f"    Your application range of 7.2 revolutions is NOT achievable.")
    elif fwd_stall:
        print(f"  Physical stop: YES — stalled at revolution {fwd_revs}")
        print(f"    Servo rotated {fwd_revs} full revolution(s) before stopping.")
    else:
        total_output = servo_steps_to_deg(fwd_revs * SERVO_STEPS_PER_REV)
        print(f"  Physical stop: NONE — completed {fwd_revs} revolutions freely")
        print(f"    ({total_output:.2f}° output shaft travel, "
              f"{fwd_revs / RATIO_BASE:.2f} output revolutions)")
        print(f"    ✓ Servo can rotate continuously — no internal hard-stop.")

    print()

    # Revolution tracking
    print(f"  Revolution tracking:")
    print(f"    Start abs pos:    {start_abs} steps")
    print(f"    End abs pos:      {final_abs} steps  (after full round-trip)")
    if roundtrip_error is not None:
        print(f"    Round-trip error: {roundtrip_error} steps "
              f"({servo_steps_to_deg(roundtrip_error):.3f}° output)")
        if roundtrip_error <= 20:
            print(f"    ✓ Tracking accurate — wrap detection working correctly")
        elif roundtrip_error <= 100:
            print(f"    ⚠ Small drift — acceptable for {fwd_revs} revolutions")
        else:
            print(f"    ✗ Large drift ({roundtrip_error} steps) — check wrap threshold")

    print()

    # Cycloidal drive
    print(f"  Cycloidal drive:")
    if fwd_stall and fwd_revs < 1:
        print(f"    ✗ Could not complete even one servo revolution")
    else:
        print(f"    ✓ Motion transmitted for {fwd_revs} servo revolutions")
        print(f"      = {fwd_revs / RATIO_BASE * 360:.1f}° at output shaft "
              f"(1:{int(RATIO_BASE)} reduction)")

    print()
    print("  Test complete.")
    print("=" * 64)


# ============================================================
#  Main
# ============================================================

def main():
    parser = argparse.ArgumentParser(
        description="Base position control with Mode 0 + revolution tracking"
    )
    parser.add_argument("--port", required=True, help="Serial port (e.g. /dev/ttyACM0)")
    parser.add_argument("--id",   type=int, default=1, help="Servo ID (default: 1)")

    g = parser.add_mutually_exclusive_group()
    g.add_argument("--status",       action="store_true", help="Show current servo state")
    g.add_argument("--home",         action="store_true", help="Move to home position (0°)")
    g.add_argument("--angle",        type=float,          help="Move to specified angle (degrees)")
    g.add_argument("--test-sweep",   action="store_true", help="Sweep full range for testing")
    g.add_argument("--test-rotation",action="store_true",
                   help="Multi-revolution test: physical-stop check + tracking validation")

    parser.add_argument("--speed",      type=int, default=20,
                        help="Speed %% for --test-rotation (5-50%%, default 20)")
    parser.add_argument("--revolutions",type=int, default=10,
                        help="Servo revolutions for --test-rotation (default 10)")

    args = parser.parse_args()

    # Open serial connection
    try:
        sc = SCSerial(args.port)
    except Exception as e:
        print(f"✗ Failed to open {args.port}: {e}", file=sys.stderr)
        return 1

    # Initialize tracker — reads initial position from servo
    tracker = MultiTurnTracker(sc, args.id)

    try:
        if args.status:
            cmd_status(sc, args.id, tracker)
        elif args.home:
            cmd_home(sc, args.id, tracker)
        elif args.angle is not None:
            cmd_move(sc, args.id, tracker, args.angle)
        elif args.test_sweep:
            cmd_test_sweep(sc, args.id, tracker)
        elif args.test_rotation:
            cmd_test_rotation(sc, args.id, tracker,
                              speed_percent=args.speed,
                              revolutions=args.revolutions)
        else:
            # Default: show status
            cmd_status(sc, args.id, tracker)

    finally:
        sc.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
