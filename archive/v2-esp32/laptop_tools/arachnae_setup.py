#!/usr/bin/env python3
"""
============================================================
  PROJET ARACHNAE v2 — Auto Setup & Calibration Script
  pip install pyserial
  python arachnae_setup.py

  Run this BEFORE flashing main.ino. The script talks
  directly to the Waveshare Serial Bus Servo Driver Board
  over its USB-C port (CH340 chip), bypassing the ESP32.

  Phases:
    1. Port detection & baud scan
    2. Servo ID assignment  (ONE servo at a time)
    3. Mode & baud config   (all connected)
    4. Verification report
    5. Homing smoke-test    (optional, arm moves!)
    6. Summary + next steps
============================================================
"""

import sys
import time
import struct
import json
from pathlib import Path
from typing import Optional, List

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("pyserial not found. Install with:  pip install pyserial")
    sys.exit(1)

# ── SCSerial register map (matches servo_control.h) ──────────
REG_ID             = 0x05
REG_BAUD           = 0x06
REG_OPERATING_MODE = 0x21
REG_TORQUE_LIMIT   = 0x22
REG_TORQUE_ENABLE  = 0x28
REG_GOAL_POSITION  = 0x2A
REG_GOAL_SPEED     = 0x2E
REG_PRESENT_POS    = 0x38
REG_PRESENT_SPEED  = 0x3A

BROADCAST_ID = 0xFE

# Feetech baud-rate register codes
BAUD_MAP = {
    1_000_000: 0,
    500_000:   1,
    250_000:   2,
    128_000:   3,
    115_200:   4,
    76_800:    5,
    57_600:    6,
    38_400:    7,
}
TARGET_BAUD = 1_000_000

# ── PROJET ARACHNAE servo configuration ──────────────────────
# id       : target servo ID
# name     : label
# mode     : 3 = extended position (multi-turn), 0 = standard (single-turn)
# torque   : torque limit (0–1000)
# ratio    : gear ratio (output shaft)
SERVO_CFG = [
    {"id": 1, "name": "BASE",     "mode": 3, "torque": 700, "ratio": 10.0},
    {"id": 2, "name": "SHOULDER", "mode": 3, "torque": 700, "ratio":  6.0},
    {"id": 3, "name": "ELBOW",    "mode": 3, "torque": 700, "ratio":  5.0},
    {"id": 4, "name": "FOREARM",  "mode": 3, "torque": 700, "ratio":  2.0},
    {"id": 5, "name": "GRIPPER",  "mode": 0, "torque": 350, "ratio":  1.0},
]

# Home angles for the smoke test (link-frame degrees, matches config.h)
HOME_DEG = {
    "BASE": 0.0, "SHOULDER": -60.0, "ELBOW": 120.0,
    "FOREARM": 30.0, "GRIPPER": 45.0
}
STEPS_PER_REV = 4096
STEPS_CENTRE  = 2048   # centre for standard-mode servo


# ============================================================
#  SCSerial protocol implementation
# ============================================================
class SCSerial:
    """
    Minimal SCSerial (Feetech) bus driver over a raw serial port.

    Packet format (write):
      FF FF  ID  LEN  0x03  REG  DATA...  CHKSUM
      LEN = 3 + n_data

    Packet format (read request):
      FF FF  ID  4  0x02  REG  N_BYTES  CHKSUM

    Response:
      FF FF  ID  LEN  ERR  DATA...  CHKSUM
      LEN = 2 + n_data  (ERR + data + chksum)
      Total bytes = 5 + n_data
    """

    def __init__(self, port: str, baud: int = TARGET_BAUD):
        self.ser = serial.Serial(port, baud, timeout=0.08)
        time.sleep(0.15)
        self.ser.reset_input_buffer()
        self._baud = baud

    def close(self):
        if self.ser.is_open:
            self.ser.close()

    def set_baud(self, baud: int):
        self.ser.baudrate = baud
        self._baud = baud
        time.sleep(0.05)
        self.ser.reset_input_buffer()

    # ── Low-level ─────────────────────────────────────────────
    @staticmethod
    def _checksum(payload: List[int]) -> int:
        return (~sum(payload)) & 0xFF

    def _send(self, payload: List[int]):
        """Send FF FF + payload + checksum."""
        pkt = bytes([0xFF, 0xFF] + payload + [self._checksum(payload)])
        self.ser.reset_input_buffer()
        self.ser.write(pkt)
        self.ser.flush()

    def _recv(self, n: int) -> bytes:
        """Read exactly n bytes with the current serial timeout."""
        return self.ser.read(n)

    # ── Ping ──────────────────────────────────────────────────
    def ping(self, id_: int) -> bool:
        """Return True if servo id_ responds to a PING instruction."""
        self._send([id_, 2, 0x01])
        resp = self._recv(6)
        return (len(resp) >= 6 and
                resp[0] == 0xFF and resp[1] == 0xFF and resp[2] == id_)

    # ── Write helpers ─────────────────────────────────────────
    def write_byte(self, id_: int, reg: int, val: int):
        self._send([id_, 4, 0x03, reg, val & 0xFF])
        time.sleep(0.012)   # EEPROM write settling time

    def write_word(self, id_: int, reg: int, val: int):
        lo, hi = val & 0xFF, (val >> 8) & 0xFF
        self._send([id_, 5, 0x03, reg, lo, hi])
        time.sleep(0.012)

    def write_dword(self, id_: int, reg: int, val: int):
        b = [(val >> (8 * i)) & 0xFF for i in range(4)]
        self._send([id_, 7, 0x03, reg] + b)
        time.sleep(0.012)

    # ── Read helpers ──────────────────────────────────────────
    def read_bytes(self, id_: int, reg: int, n: int) -> Optional[bytes]:
        """
        Read n bytes from register reg.
        Response: FF FF ID LEN ERR DATA[0..n-1] CHKSUM
        Total bytes to read: 5 + n  (2 hdr + ID + LEN + ERR + data + CHKSUM)
        """
        self._send([id_, 4, 0x02, reg, n])
        total = 6 + n          # 2 header + ID + LEN + ERR + n data + CHKSUM
        resp  = self._recv(total)
        if len(resp) < total:
            return None
        if resp[0] != 0xFF or resp[1] != 0xFF or resp[2] != id_:
            return None
        if resp[4] != 0:       # error byte
            return None
        return resp[5:5 + n]   # data only

    def read_byte(self, id_: int, reg: int) -> Optional[int]:
        r = self.read_bytes(id_, reg, 1)
        return r[0] if r else None

    def read_word(self, id_: int, reg: int) -> Optional[int]:
        r = self.read_bytes(id_, reg, 2)
        return (r[0] | (r[1] << 8)) if r else None

    def read_word_signed(self, id_: int, reg: int) -> Optional[int]:
        v = self.read_word(id_, reg)
        if v is None:
            return None
        return v - 0x10000 if v > 0x7FFF else v

    def read_dword(self, id_: int, reg: int) -> Optional[int]:
        r = self.read_bytes(id_, reg, 4)
        if r is None:
            return None
        v = r[0] | (r[1] << 8) | (r[2] << 16) | (r[3] << 24)
        return v - 0x100000000 if v >= 0x80000000 else v


# ============================================================
#  Utility helpers
# ============================================================

def deg_to_steps(deg: float, ratio: float, mode: int) -> int:
    """Convert output-shaft degrees to servo step count."""
    if mode == 0:
        # Standard mode: offset from centre (2048)
        return int(STEPS_CENTRE + round(deg * ratio * STEPS_PER_REV / 360.0))
    else:
        # Extended mode: signed, no offset
        return int(round(deg * ratio * STEPS_PER_REV / 360.0))


def steps_to_deg(steps: int, ratio: float, mode: int) -> float:
    if mode == 0:
        return (steps - STEPS_CENTRE) * 360.0 / (STEPS_PER_REV * ratio)
    else:
        return steps * 360.0 / (STEPS_PER_REV * ratio)


def scan_ports() -> List[str]:
    """Return ports that look like USB-serial adapters."""
    likely = []
    for p in serial.tools.list_ports.comports():
        desc = (p.description or "").lower()
        if any(kw in desc for kw in ["ch340", "cp210", "usb serial",
                                      "uart", "ft232"]):
            likely.append(p.device)
    return likely


def discover_ids(sc: SCSerial, max_id: int = 20) -> List[int]:
    """Ping IDs 1..max_id and return those that respond."""
    found = []
    for id_ in range(1, max_id + 1):
        if sc.ping(id_):
            found.append(id_)
    return found


def wait_still(sc: SCSerial, ids: List[int],
               tol: int = 10, timeout: float = 8.0) -> bool:
    """Block until all servos report speed ≤ tol, or timeout."""
    t0 = time.monotonic()
    while time.monotonic() - t0 < timeout:
        speeds = [sc.read_word_signed(i, REG_PRESENT_SPEED) or 0
                  for i in ids]
        if all(abs(s) <= tol for s in speeds):
            return True
        time.sleep(0.05)
    return False


def move_servo(sc: SCSerial, cfg: dict, deg: float, speed: int = 400):
    """Move one servo to the given output-shaft angle."""
    id_   = cfg["id"]
    steps = deg_to_steps(deg, cfg["ratio"], cfg["mode"])
    sc.write_word(id_, REG_GOAL_SPEED, speed)
    if cfg["mode"] == 0:
        sc.write_word(id_, REG_GOAL_POSITION, steps & 0xFFFF)
    else:
        sc.write_dword(id_, REG_GOAL_POSITION, steps)


def configure_servo(sc: SCSerial, cfg: dict):
    """Write mode, baud, and torque limit to EEPROM."""
    id_ = cfg["id"]
    sc.write_byte(id_, REG_TORQUE_ENABLE,  0)                    # EEPROM needs torque off
    sc.write_byte(id_, REG_BAUD,           BAUD_MAP[TARGET_BAUD])
    sc.write_byte(id_, REG_OPERATING_MODE, cfg["mode"])
    sc.write_word(id_, REG_TORQUE_LIMIT,   cfg["torque"])
    sc.write_byte(id_, REG_TORQUE_ENABLE,  1)


def verify_servo(sc: SCSerial, cfg: dict) -> dict:
    id_    = cfg["id"]
    result = {"id": id_, "name": cfg["name"], "ok": True, "issues": []}

    if not sc.ping(id_):
        result["ok"] = False
        result["issues"].append("not responding to ping")
        return result

    baud = sc.read_byte(id_, REG_BAUD)
    if baud != BAUD_MAP[TARGET_BAUD]:
        result["ok"] = False
        result["issues"].append(
            f"baud code {baud} (expected {BAUD_MAP[TARGET_BAUD]})")

    mode = sc.read_byte(id_, REG_OPERATING_MODE)
    if mode != cfg["mode"]:
        result["ok"] = False
        result["issues"].append(f"mode {mode} (expected {cfg['mode']})")

    result["baud_code"] = baud
    result["mode"]      = mode
    return result


# ============================================================
#  Phase helpers
# ============================================================

def phase_header(n: int, title: str):
    print(f"\n{'─' * 58}")
    print(f"  PHASE {n}: {title}")
    print(f"{'─' * 58}")


def confirm(prompt: str, default: str = "") -> str:
    try:
        return input(prompt).strip().lower() or default
    except (KeyboardInterrupt, EOFError):
        print("\nAborted.")
        sys.exit(0)


# ============================================================
#  Main
# ============================================================

def main():
    print("=" * 58)
    print("  PROJET ARACHNAE v2 — Auto Setup & Calibration")
    print("=" * 58)
    print()
    print("Connect the Waveshare Serial Bus Servo Driver Board via")
    print("its USB-C port. Ensure 12 V power is connected to the")
    print("board's power terminals before scanning.\n")

    # ── Phase 1: Port selection & baud scan ───────────────────
    phase_header(1, "Port Detection & Baud Scan")

    all_ports = [p.device for p in serial.tools.list_ports.comports()]
    likely    = scan_ports()

    if not all_ports:
        print("No serial ports found. Connect the board and try again.")
        sys.exit(1)

    print("Available serial ports:")
    for i, p in enumerate(all_ports):
        tag = "  ← likely servo board" if p in likely else ""
        print(f"  [{i}] {p}{tag}")

    choice = confirm(f"\nSelect port [0–{len(all_ports)-1}]: ")
    try:
        port = all_ports[int(choice)]
    except (ValueError, IndexError):
        print("Invalid selection.")
        sys.exit(1)

    print(f"\nScanning {port} at supported baud rates...")
    working_baud  = None
    found_by_baud = {}

    for baud in [1_000_000, 500_000, 115_200, 57_600]:
        try:
            sc = SCSerial(port, baud)
            ids = discover_ids(sc)
            sc.close()
            if ids:
                found_by_baud[baud] = ids
                tag = " ✓ TARGET" if baud == TARGET_BAUD else ""
                print(f"  {baud:>9} baud → found IDs: {ids}{tag}")
                if working_baud is None:
                    working_baud = baud
        except serial.SerialException as e:
            print(f"  {baud:>9} baud → error: {e}")

    if not found_by_baud:
        print("\nNo servos detected at any baud rate.")
        print("Check:")
        print("  • 12V power supply is on")
        print("  • At least one servo is plugged into the TTL port")
        print("  • USB-C cable is data-capable (not charge-only)")
        sys.exit(1)

    # ── Phase 2: ID assignment ─────────────────────────────────
    phase_header(2, "Servo ID Assignment  (one servo at a time)")
    print("You will connect each servo alone and assign its target ID.")
    print("Factory default ID is 1 for all servos.")
    print()

    sc = SCSerial(port, working_baud)

    for cfg in SERVO_CFG:
        target_id = cfg["id"]
        name      = cfg["name"]
        print(f"  ┌─ {name} servo (target ID: {target_id}) ─────────────────")
        confirm(f"  │  Connect ONLY the {name} servo, then press Enter: ")

        ids_now = discover_ids(sc)
        if not ids_now:
            print("  │  ⚠ No servo detected. Check power and TTL connector.")
            if confirm("  │  Retry? [y/n]: ") == "y":
                ids_now = discover_ids(sc)
        if not ids_now:
            print(f"  └  SKIP — could not detect {name}\n")
            continue

        current_id = ids_now[0]
        if current_id == target_id:
            print(f"  │  ✓ Already ID {target_id}")
        else:
            print(f"  │  Detected as ID {current_id} → reassigning to {target_id}")
            sc.write_byte(current_id, REG_TORQUE_ENABLE, 0)
            time.sleep(0.05)
            sc.write_byte(current_id, REG_ID, target_id)
            time.sleep(0.15)
            ok = sc.ping(target_id)
            print(f"  │  {'✓ ID set' if ok else '✗ Assignment failed — try again'}")

        # If servo was at a non-target baud, migrate it now
        if working_baud != TARGET_BAUD:
            print(f"  │  Setting baud to {TARGET_BAUD}...")
            sc.write_byte(target_id, REG_TORQUE_ENABLE, 0)
            sc.write_byte(target_id, REG_BAUD, BAUD_MAP[TARGET_BAUD])
            time.sleep(0.15)
            print("  │  Baud updated (takes effect after power cycle)")

        print(f"  └  Done with {name}\n")

    sc.close()

    # If baud migration happened, servo needs a power cycle
    if working_baud != TARGET_BAUD:
        print("  ⚡ Baud rate was changed on one or more servos.")
        print("     Power-cycle the board NOW (unplug 12V, wait 3s, reconnect).")
        confirm("  Press Enter once power is restored: ")

    # Reconnect at target baud with all servos
    sc = SCSerial(port, TARGET_BAUD)

    # ── Phase 3: Mode & torque configuration ──────────────────
    phase_header(3, "Mode & Torque Configuration")
    print("Connect ALL five servos now.")
    confirm("Press Enter when all servos are on the bus: ")

    all_ids = discover_ids(sc)
    print(f"  Detected IDs: {all_ids}")
    missing = {c["id"] for c in SERVO_CFG} - set(all_ids)
    if missing:
        print(f"  ⚠ Missing: {missing} — check wiring, continuing anyway")

    for cfg in SERVO_CFG:
        id_ = cfg["id"]
        if not sc.ping(id_):
            print(f"  ✗ {cfg['name']} (ID{id_}): not responding — skip")
            continue
        print(f"  Configuring {cfg['name']} (ID{id_})  "
              f"mode={cfg['mode']}  torque_limit={cfg['torque']}...", end="")
        configure_servo(sc, cfg)
        print(" ✓")

    # ── Phase 4: Verification ─────────────────────────────────
    phase_header(4, "Verification")

    all_ok = True
    for cfg in SERVO_CFG:
        r   = verify_servo(sc, cfg)
        sym = "✓" if r["ok"] else "✗"
        print(f"  {sym} {r['name']:<10} ID{r['id']}  "
              f"mode={r.get('mode','?')}  "
              f"baud_code={r.get('baud_code','?')}")
        for iss in r.get("issues", []):
            print(f"      ⚠ {iss}")
        if not r["ok"]:
            all_ok = False

    if all_ok:
        print("\n  All servos verified ✓")
    else:
        print("\n  ⚠ Issues found — review above. Re-run script if needed.")

    # ── Phase 5: Homing smoke test ────────────────────────────
    phase_header(5, "Homing Smoke Test  (optional)")
    print("The arm will move to mechanical home. Angles:")
    for cfg in SERVO_CFG:
        print(f"  {cfg['name']:<10}: {HOME_DEG[cfg['name']]:+.1f}°")
    print()
    print("⚠  CLEAR THE ARM OF ALL OBSTACLES BEFORE PROCEEDING.")

    if confirm("Type 'go' to move to home, or Enter to skip: ") == "go":
        print("  Moving...", end="", flush=True)
        for cfg in SERVO_CFG:
            move_servo(sc, cfg, HOME_DEG[cfg["name"]], speed=300)

        ids = [c["id"] for c in SERVO_CFG]
        done = wait_still(sc, ids, timeout=10.0)
        print(" done." if done else " TIMEOUT — check for binding.")

        print("  Actual positions:")
        for cfg in SERVO_CFG:
            id_ = cfg["id"]
            if cfg["mode"] == 0:
                steps = sc.read_word(id_, REG_PRESENT_POS)
            else:
                steps = sc.read_dword(id_, REG_PRESENT_POS)
            if steps is not None:
                deg = steps_to_deg(steps, cfg["ratio"], cfg["mode"])
                print(f"    {cfg['name']:<10}: {steps:+6d} steps → {deg:+7.2f}°  "
                      f"(target {HOME_DEG[cfg['name']]:+.1f}°)")
            else:
                print(f"    {cfg['name']:<10}: READ ERROR")
    else:
        print("  Skipped.")

    # ── Phase 6: Summary ──────────────────────────────────────
    phase_header(6, "Summary & Next Steps")

    report = {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "port": port,
        "baud": TARGET_BAUD,
        "all_ok": all_ok,
        "servos": [
            {
                "id":   c["id"],
                "name": c["name"],
                "mode": c["mode"],
                "present": sc.ping(c["id"]),
            }
            for c in SERVO_CFG
        ],
    }
    report_path = Path("arachnae_calibration.json")
    report_path.write_text(json.dumps(report, indent=2))
    print(f"  Calibration report saved → {report_path}")

    sc.close()

    print("""
  ┌─ NEXT STEPS ────────────────────────────────────────────┐
  │                                                          │
  │  1. Disconnect the Waveshare board from your PC.        │
  │     Reconnect its TTL header to the ESP32 (TX→D6,      │
  │     RX→D7, GND→GND). No direction pin needed.          │
  │                                                          │
  │  2. Flash main.ino to the main ESP32-S3.                │
  │     Libraries needed (Arduino Library Manager):         │
  │       • WebSockets  by Markus Sattler                   │
  │       • ArduinoJson by Benoit Blanchon                  │
  │                                                          │
  │  3. Flash esp32cam_stream.ino to the XIAO ESP32S3       │
  │     Sense. Board: "XIAO_ESP32S3 Sense" in IDE.         │
  │     Configure its WiFi to match the arm's network.      │
  │                                                          │
  │  4. Power on the arm. Connect to WiFi "ARACHNAE"        │
  │     (pass: <AP password>) on your phone/PC.              │
  │                                                          │
  │  5. Open the teach UI:  http://192.168.4.1/teach        │
  │     Follow the on-screen prompts in order:              │
  │       HOME → FWD_REACH → HANDOFF → LEFT_LIMIT →        │
  │       RIGHT_LIMIT → HEIGHT_MAX → DEPTH_MAX             │
  │     Press "Lock & Save" when all 7 positions are set.   │
  │                                                          │
  │  6. Start the CV pipeline on your phone/PC:             │
  │       python cv_pipeline.py \\                           │
  │         --cam http://192.168.4.2/stream \\               │
  │         --arm ws://192.168.4.1:8765                     │
  │     (XIAO ESP32S3 Sense IP may differ — check Serial)   │
  │                                                          │
  └──────────────────────────────────────────────────────────┘
""")

    print("Setup complete.")


if __name__ == "__main__":
    main()
