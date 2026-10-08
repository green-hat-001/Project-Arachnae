# Gear support bracket, 4 arms (2026-10-04)

Replaces `1_worm_4start/Gear_support_bracket` (the V bracket). The old one hung from the two
front-corner M5 rods only, so the hub under the gear tip was supported from one side. This one
runs an arm to **all four** case-corner M5 holes, so it is an X centred on the gear axis.

Files:
- `Gear_support_bracket_4arm.step`: in the **`Base_cycloidal_gearbox_v2.step` assembly frame**, the same frame as
  `servo_holder_v2/` (base and gear axis at (0, 50), +Z up towards the case, worm along +Y). It is
  placed where the old bracket sat: lip up, top face at z −6.3.
- `Gear_support_bracket_4arm_print.stl`: oriented for printing, lip face down.
- `top_view.png`: new against old, with the case, holder, worm and gear.

## Geometry

| | Old (V) | New (X) |
|---|---|---|
| Arms | 2, to corners A, B (holder side) | **4**, to A, B, C, D |
| Arm section | 10 wide × 6.2 thick | **12 wide × 5.0 thick** |
| Hub | Ø24 × 6.2 | Ø24 × 6.2 (unchanged) |
| Bearing | 625 in Ø15.7 × 5 pocket, Ø12 × 1.2 lip | unchanged |
| Bolt holes | 6 × 9 radial slots, ±1.5 mm | same, at all four |
| Arm-to-hub fillets | — | R5 |
| Volume | 7.4 cm³ | 12.2 cm³ |

The four corners are all 49.31 mm from the gear axis, 90° apart: A (33.5, 13.82), B (−36.18, 16.5),
C (36.18, 83.5), D (−33.5, 86.18).

**Why the hub is still 6.2 mm thick:** it is the 5 mm bearing pocket plus the 1.2 mm lip. Only the arms
are thinner. Their top face is flush with the hub top (z −6.3), so the gear, hub and thrust-washer
stack above is exactly as before, and the part prints flat with no supports.

**Why 5 mm and not thinner:** this bracket is what holds the throated gear axially, and the throat
only tolerates about ±0.3 mm (that is how the last gear's lips tore). Out-of-plane bending stiffness
goes as width × thickness³. Four 12 × 5 arms give about **1.26×** the old two 10 × 6.2 arms
(4·12·5³ = 6000 against 2·10·6.2³ = 4766). Four 12 × 4.5 arms would drop to about 0.92×, and four
12 × 4 to about 0.64×. In-plane (the sideways mesh load) it is much stiffer, because the load is
now carried by arms on both sides instead of cantilevering from the front.

## Checked in CAD

All are against `Base_cycloidal_gearbox_v2.step` (case moved by (0, 50, 20) into this frame: bearing pocket at z 13–17,
floor and ear undersides at z 15), `servo_holder_v2`, `Worm_2start_v2` and `Gear_10T_2start_stub`.

| Check | Result |
|---|---|
| Case | no overlap, 1.90 mm min gap (same as the old bracket) |
| Servo holder | 1.70 mm (same) |
| Worm solid / swept envelope r 9.7 | 4.11 mm / 0.60 mm (same) |
| Gear and hub | 1.88 mm (same) |
| Far corners C, D: Ø9.2 nut column from the bracket to the ear underside | completely clear, and the case already has the Ø5.4 through-holes |
| Solid / STL | one valid solid; STL watertight, manifold, no self-intersections |

## Not verified

- The Holstex plate isn't in any CAD file. The arms sit in the same z band as the old bracket
  (and their bottom is 1.2 mm *higher*), but whether the plate is under the far corners, where the
  new arms and nuts are, is unknown.
- The real printed tolerances, and whether the real far-corner holes line up. The slots give ±1.5 mm.

## Hardware and fitting

- **Far corners C and D need longer M5 rods.** Rod path from the ear underside down: a nut tight under
  the ear, 21.3 mm of free rod down to the bracket top, a nut above the boss, the 5 mm boss, and a nut
  below. So each rod needs about **33 mm or more below the ear underside**. That's 21.3 + 5 + a 4 mm
  nut + about 2 threads.
- **Front corners A and B:** same stack as before. The boss is 1.2 mm thinner, from the bottom (top
  face unchanged), so the nut under it moves up 1.2 mm.
- Thrust washers between the hub and the bracket bearing: unchanged. The guidance in
  `../README.md` still applies, because the hub top is in the same place.
- Fit order: put the bearing in (glued as before), then the washer, then the bracket over the four rods.
  Run the nuts up so the bracket sits flat on all four **without being forced**. Snug the front pair
  first, then bring the far pair's upper nuts down to touch and lock them with the lower nuts.

## Printing

PETG, lip face down as in the STL, no supports, solid (100 % or max walls) like the other
drivetrain parts. About 15 g.

## FEA: how much it flexes (2026-10-04)

`bracket_fea.py` (voxel hex8, linear static, printed PETG E 2000 / 1700 MPa across layers) compares the
old V with this X. `bracket_fea_slice.py` makes its input. Run it with
`mkdir -p /tmp/bracket_fea && freecadcmd -c "exec(open('bracket_fea_slice.py').read())"`, then
`uv run --with scikit-fem --with pyamg --with scipy --with numpy --with shapely python bracket_fea.py /tmp/bracket_fea/slices.json ../throated_gen`.

**Loads, case B** (arm straight out, cycloid 10:1, Ft2 = 190 N), applied on the bearing pocket wall:
- radial 0.44 × (Ft2 along the worm axis + 89 N separating). 0.44 is this bearing's share: case
  bearing z 15, mesh z 4, this bearing z −10.
- axial 0.35 × Ft2 = 66.5 N, pushing the gear down onto this bearing. This happens in one rotation
  direction only.

Bolts are clamped over the nut (or washer) footprint. The far rods are modelled as 21.3 mm M5 springs
in-plane (3.8 kN/mm) and rigid axially.

| Bearing seat movement, case B | old V | new X, bare nut | new X, Ø10 washer | new X, Ø15 penny washer |
|---|---|---|---|---|
| **axial (along the gear)** | **1.9 mm** | **0.50 mm** | 0.47 mm | 0.37 mm |
| sideways | 0.04–0.08 mm | 0.02 mm | 0.02 mm | 0.02 mm |
| von Mises p99.9 | 18 MPa | 9 MPa | 8.5 MPa | 8 MPa |

The results are linear, so scale them for other cases: case E (437 N) ×2.3, servo jam (961 N) ×5.1.

**What it means**
- Sideways it's now about 3× stiffer and negligible: 0.02 mm at B, 0.05 mm at E. The earlier skipping
  budget guessed 0.05–0.15 mm for this bracket.
- **Axially it's about 4× better than the old V, but still too soft.** The throated gear tolerates
  about ±0.3 mm along its axis, and the thrust-washer play uses 0.05–0.15 mm of that. So the bracket
  should flex no more than about **0.15 mm at case B**. It flexes 0.37–0.50 mm, and ~1.1 mm at case E.
- The old V would have let the gear drop ~1.9 mm under arm load.
- Stress isn't the problem: about 20 MPa p99.9 at case E. A jam (~45 MPa) is at PETG's limit,
  but a jam already breaks the gear first.
- To reach about 0.15 mm the arms need roughly 3× the out-of-plane stiffness. That means about
  7–7.5 mm thick (stiffness goes as thickness³), or a rib along each arm.

**Not modelled:** the hub-to-gear washer and bearing stack (only the bracket is solved), creep under
sustained load, how compliant the case ears and holder bosses are, and the real stiffness of the
print (±20 %).
