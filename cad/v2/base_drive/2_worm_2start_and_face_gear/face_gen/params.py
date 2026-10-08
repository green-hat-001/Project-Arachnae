"""Worm + face gear ("helicon") base stage: shared parameters.

Frame: the `Input Worm` component frame used by Worm_2start_v2.step and the throated gear
(assembly = this frame rotated +90 deg about z, then x -> asm x, z_asm = z - 1).
Gear (cycloid input) axis along z at (GX, GY); worm axis along x at y = WY, z = WZ.
Change WORM_* for another worm and rerun fc_export_face.py -> hob_face.py -> verify_face.py -> stl_face.py.
"""
import os
S = os.environ.get("FACE_SCRATCH", "/tmp/face_gen/")          # big intermediate files go here
CAD = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..") + "/"
# --- worm (existing Worm_2start_v2) ---
WORM_FILE = CAD + "base_worm_2start/Worm_2start_v2.step"
STARTS = 2
LEAD = 6.36            # mm per worm turn
WORM_TIP = 7.19        # real tip radius
WORM_ROOT = 5.39
ZONE = (6.95, 26.95)   # thread zone along the worm axis (x); body Ø12 for x < 6.95, worm ends at 26.95
# --- placement ---
GX, GY = 16.5, 2.2
WZ = 6.0
CD = 11.55             # gear axis to worm axis, in plane (fixed by holder v2 + far boss)
WY = GY + CD
# --- face gear ---
N = int(os.environ.get("FACE_N", 20))   # teeth; pitch radius r_p = N*LEAD/(STARTS*2*pi), stage ratio N/STARTS
CLR = 0.35             # cutter = worm scaled radially by (TIP+CLR)/TIP: tip/root clearance + backlash
SCALE = (WORM_TIP + CLR) / WORM_TIP
Z_TOP = float(os.environ.get("FACE_ZTOP", 0.1))   # blank top (tooth tips); the hob also trims it to the worm-root envelope
Z_DISC_BOT = -4.0      # web underside
Z_HUB_BOT = -4.5       # hub/pin end, same as the throated gear (same bracket + thrust washers)
Z_GEAR_TOP = 13.0      # same as the throated gear (sits on the case 625 inner race)
HUB_TOP_R = 3.5        # top 0.8 mm is Ø7: touches only the 625 inner race
HUB_CLR = 0.5          # hub to real worm-tip envelope
BORE_FLOOR = 0.525     # D-bore 12.5 deep (as Gear_10T_2start_throated_bore12.5)
PIN_TOP = -0.175       # Ø4.9 pin hole from the hub end
DZ = 0.05
OPEN = 0.22            # morphological opening of the hobbed slices: drops slivers < ~0.45 mm (one extrusion line)
# case (far-boss block) clearance: min radius of case material from the gear axis vs comp z (from Base.step)
CASE_RMIN = [(-2.8,15.32),(-2.4,14.45),(-2.0,13.89),(-1.6,13.51),(-1.2,13.16),(-0.8,12.90),(-0.4,12.69),
             (0.0,12.51),(0.4,12.36),(0.8,12.23),(1.5,12.03),(2.5,11.85),(4.0,11.71),(6.0,11.65),(13.5,11.61)]
CASE_CLR = 0.45
import math
V1 = os.environ.get("FACE_V1") == "1"   # v2 (2026-10-05, printability): FACE_V1=1 rebuilds the original shape
RP = N * LEAD / (STARTS * 2 * math.pi)
# v2: the same block re-measured every 0.1 mm (2026-10-05). Between the coarse points above it is convex, so
# interpolating them overstates the room (v1 measured 0.40 mm, not 0.45). Under the teeth its underside is shallow,
# so v2 keeps a true CASE_CLR2 normal gap (offset in the r-z plane), not a radial one. 30 = no case material.
CASE_RMIN_FINE = [(-3.3,19.43),(-3.2,19.14),(-3.1,18.93),(-3.0,18.76),(-2.9,15.69),(-2.8,15.31),(-2.7,15.04),
    (-2.6,14.81),(-2.5,14.61),(-2.4,14.44),(-2.3,14.28),(-2.2,14.14),(-2.1,14.01),(-2.0,13.89),(-1.9,13.78),
    (-1.8,13.67),(-1.7,13.58),(-1.6,13.48),(-1.5,13.40),(-1.4,13.31),(-1.3,13.24),(-1.2,13.16),(-1.1,13.09),
    (-1.0,13.02),(-0.9,12.96),(-0.8,12.90),(-0.7,12.84),(-0.6,12.79),(-0.5,12.73),(-0.4,12.68),(-0.3,12.63),
    (-0.2,12.59),(-0.1,12.54),(0.0,12.50),(0.1,12.46),(0.2,12.42),(0.3,12.38),(0.4,12.35),(0.5,12.31),
    (0.6,12.28),(0.7,12.25),(0.8,12.22),(0.9,12.19),(1.0,12.16),(1.5,12.03),(2.5,11.85),(4.0,11.71),
    (6.0,11.65),(13.5,11.61)]
CASE_CLR2 = 0.5
def _rcase(z):
    if z < -3.3: return 30.0
    for (z0, r0), (z1, r1) in zip(CASE_RMIN_FINE, CASE_RMIN_FINE[1:]):
        if z0 <= z <= z1: return r0 + (r1 - r0) * (z - z0) / (z1 - z0)
    return CASE_RMIN_FINE[-1][1]
def r_out(z):
    """outer radius allowed at comp z (case clearance), capped at 14.5"""
    if not V1:
        c = CASE_CLR2; n = 40
        return min(14.5, min(_rcase(z + c * k / n) - math.sqrt(max(0.0, c * c - (c * k / n) ** 2)) for k in range(-n, n + 1)))
    if z < CASE_RMIN[0][0]: return 14.5
    for (z0, r0), (z1, r1) in zip(CASE_RMIN, CASE_RMIN[1:]):
        if z0 <= z <= z1: return min(14.5, r0 + (r1 - r0) * (z - z0) / (z1 - z0) - CASE_CLR)
    return CASE_RMIN[-1][1] - CASE_CLR
HUB_OVH = math.radians(30)   # v2: max overhang of the hub flare above the worm (from vertical); v1 reached 44
def r_hub(z):
    """hub radius that clears the real worm tip by HUB_CLR; v2 caps the upper flare's overhang at HUB_OVH"""
    R = WORM_TIP + HUB_CLR; a = abs(z - WZ)
    r = CD - math.sqrt(R * R - a * a) if a < R else 99
    if not V1 and z > WZ:
        ak = R * math.sin(HUB_OVH)                       # where the circle reaches HUB_OVH
        if a > ak: r = min(r, CD - R * math.cos(HUB_OVH) + (a - ak) * math.tan(HUB_OVH))
    return min(r, r_out(z), 6.0)
