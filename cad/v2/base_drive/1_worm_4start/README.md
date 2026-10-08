# Base worm stage — 4-start replacement (2026-09-30)

Drop-in replacement for `Component53` (worm) and `Component54/002` (20T gear) in `Base_cycloidal_gearbox_v2.step`.
Both files are in the **same coordinate frame as the `Input Worm` component** in `Base_cycloidal_gearbox_v2.step`,
so they import into the Fusion assembly in place.

## What changed
| | Old | New |
|---|---|---|
| Worm starts | 1 | **4** |
| Worm stage ratio | 20:1 | **5:1** |
| Lead angle | 2.2° | **8.86°** |
| Lead | 1.57 mm | 6.36 mm |
| Normal module / pressure angle | 0.5 / 20° | 0.5 / 20° |
| Gear teeth | 20 | 20 |
| Hand | right | right (unchanged, so rotation direction is unchanged) |
| Worm tip / root Ø | 14.27 / 12.0 | 13.88 / 11.73 |
| Gear tip / root Ø | 11.0 / 8.75 | 11.02 / 8.87 |
| Gear teeth concentric with bore | no (0.11 mm off) | **yes** |
| Designed backlash (normal) | 0 | 0.10 mm |
| Tip-to-root clearance | ~0 | 0.175 mm |

Unchanged (copied from the original solids, not redrawn): the worm's horn flange, 4 × Ø2.7 screw
holes and Ø5 counterbores, Ø12 body, the Ø5.2 alignment-rod bore (43 mm deep),
the thread zone (x = 6.95 … 26.95), the gear's D-bore (Ø5.2, flat 1.43 mm from centre, 12 mm
deep, 4 mm floor), the 16 mm face width, and the **11.55 mm centre distance** (worm axis to rod
axis).

Changed on request: the centre hole in the flange face was widened from Ø3.2 to **Ø5.2**
(1 mm larger all round) so the horn screw head sits flush. Depth is unchanged at 3 mm, and the
wall to each counterbore is 1.9 mm.

## Verified in CAD
- Both are single valid solids, and the STL meshes are watertight with no self-intersections.
- Mesh simulated over a full worm revolution in 15° steps, with the gear turning at worm/5:
  no contact at any phase, a constant 0.046 mm minimum gap, and a 1.1° gear-rotation backlash
  window (matching the designed 0.10 mm).
- Every new surface lies inside the envelope of the old parts, so there are no new clashes with
  the housing.

## Not verified
- Self-locking. At an 8.86° lead angle, PETG on PETG is expected to lock, but with little
  margin. Lubricant reduces the margin further.
- Real printed tolerances.

## Printing
PETG, 0.4 mm nozzle, same orientation and settings as the old parts.

## Firmware after fitting
- Base ratio = 5 × cycloid ratio (50 if the cycloid is 10:1, 100 if it is 20:1). Set it in
  `/teach` → Joint calibration.
- `BACKLASH_BASE_STEPS` (currently 4096 = one servo rev, measured on the old stage) must be
  changed before running. On the new stage, 4096 steps is about 7° at the base.
- `SIGN_BASE` is unchanged: same hand, same direction.

## Gear tip support (2026-10-01): fixes the skipping under load
Under arm load the worm pushes the gear sideways by about 30–60 N. The gear was held by only
the one 625 bearing at its top, so its free tip leaned away and the teeth skipped. These parts
support the tip too.

- `Gear_20T_4start_stub.step/.stl`: the same gear (same teeth, same D-bore, same position)
  plus a **Ø8.6 × 1.5 mm hub** on the bottom (halved 2026-10-01 for clearance to the
  Holstex plate) and a **Ø4.9 × 4.3 mm press hole** for a Ø5 steel rod **cut to 11.5 mm**. 1.2 mm of floor separates it from the D-bore, so the drive rod still stops where it
  did. Same frame as `Gear_20T_4start.step`.
- `Gear_support_bracket.step/.stl`: a V bracket that holds a **625-2RS (5×16×5)** in a Ø15.7
  pocket (the same fit as the case's existing pocket) under a 1.2 mm lip (Ø12 opening, which
  touches only the outer race).
  - Two arms run to the **two front corner M5 bolts**. From `Base_cycloidal_gearbox_v2.step`: 49.3 mm from the gear
    axis, 90° apart, 69.7 mm between bolts.
  - The holes are 6 × 9 mm slots along each arm (±1.5 mm adjustment).
  - 6.2 mm thick. The STL is oriented lip face down, ready to print.
- Stack below the gear tip: 1.5 mm hub, 0.8 mm gap, 6.2 mm bracket. **The bracket's bottom
  face is 8.5 mm below the gear tip**, and the pin end (11.5 mm pin) is 0.2 mm below that.
- Verified in CAD: both are single valid solids, and the STLs are watertight with no
  self-intersections. No contact between bracket, gear and pin. The bearing lies 2 mm below the
  hub.
