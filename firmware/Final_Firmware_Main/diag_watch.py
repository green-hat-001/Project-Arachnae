#!/usr/bin/env python3
"""
Continuously polls /teach/diag and prints a compact per-servo status line
every time something changes — built for chasing loose bus connections
(see servo_control.h and docs/FIRMWARE.md, "Servo bus"): watch this while wiggling,
pressing, or resoldering a joint and see live which servos it affects.

Stdlib only, nothing to install.

Usage:
    python3 diag_watch.py <arm-ip> [--interval 0.25] [--all]

    <arm-ip>      required — the XIAO's current IP (it moves; sweep the
                  subnet or check the AP fallback if you don't know it)
    --interval    poll period in seconds (default 0.25)
    --all         print every poll, not just changes (noisy; off by default)
"""
import argparse
import json
import sys
import time
import urllib.request
import urllib.error

NAMES = {1: "Base", 2: "Shoulder", 3: "Elbow", 4: "Forearm", 5: "Gripper"}


def poll(url):
    with urllib.request.urlopen(url, timeout=1) as r:
        return json.loads(r.read())


def summarize(diag):
    """One short token per servo: DEAD, or a couple of live fields."""
    parts = []
    for sv in diag["servos"]:
        name = NAMES.get(sv["id"], f"id{sv['id']}")
        if sv.get("mode", -1) == -1 and sv.get("torque", -1) == -1 and sv.get("pos", -1) == -1:
            parts.append(f"{name}=DEAD")
        else:
            bits = []
            if sv.get("torque", -1) != -1:
                bits.append("torque=" + ("ON" if sv["torque"] else "off"))
            if sv.get("pos", -1) != -1:
                bits.append(f"pos={sv['pos']}")
            if sv.get("load", -1) != -1:
                bits.append(f"load={sv['load']}")
            parts.append(f"{name}=" + (",".join(bits) if bits else "partial"))
    return "  ".join(parts)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ip", help="arm's current IP, e.g. 192.168.4.1")
    ap.add_argument("--interval", type=float, default=0.25, help="poll period in seconds (default 0.25)")
    ap.add_argument("--all", action="store_true", help="print every poll instead of only changes")
    args = ap.parse_args()

    url = f"http://{args.ip}/teach/diag"
    print(f"[diag_watch] polling {url} every {args.interval}s — Ctrl+C to stop", flush=True)

    last = None
    while True:
        ts = time.strftime("%H:%M:%S")
        try:
            diag = poll(url)
            line = summarize(diag)
            if args.all or line != last:
                print(f"[{ts}] {line}", flush=True)
                last = line
        except (urllib.error.URLError, OSError, TimeoutError) as e:
            if last != "UNREACHABLE":
                print(f"[{ts}] [diag_watch] unreachable: {e}", flush=True)
                last = "UNREACHABLE"
        except KeyboardInterrupt:
            sys.exit(0)
        time.sleep(args.interval)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
