# Base cycloid gearbox (2026-10-04)

Printable parts for the base's 10:1 cycloidal reducer (11 ring pins, 10-lobe cycloids), plus the new
one-piece double cam.

| File | What | Print |
|---|---|---|
| `Double_cam.step` / `Double_cam_print.stl` | **NEW**: cam 1 + cam 2 in one piece, 180° apart | cam 1 down, supports from the build plate |
| `Cycloid_1.step` / `Cycloid_1_print.stl` | cycloid 1 (lower, on cam 1), unchanged from `Base_cycloidal_gearbox_v2.step` | flat |
| `Cycloid_2.step` / `Cycloid_2_print.stl` | cycloid 2 (upper, on cam 2), unchanged from `Base_cycloidal_gearbox_v2.step` | flat |
| `make_double_cam.py`, `export_cycloids.py` | FreeCAD scripts that make the files above | |
| `renders/` | exploded view and an assembled section | |

The STEP files are in the `Base_cycloidal_gearbox_v2.step` assembly frame (cycloid axis at (0, 50)); the STLs sit flat on z = 0.
The two cycloids are not identical in the CAD: cycloid 1 has about 4 % more material. So print the one you're
replacing from its own file. Print them like the originals; PETG at 100 % suits the cams.

## The double cam

Replaces `CAM1` + `CAM 2` of `Base_cycloidal_gearbox_v2.step`: both eccentrics in **one printed part, 180° apart**, pinned to the
Ø5 input rod with the existing pen nib. The cycloid bearing locks the nib, so no glue is needed. Nothing else in the cycloid changes. The output, output fork, cycloids, bearings,
output pins and ring pins all stay.

Files:
- `Double_cam.step`: in the `Base_cycloidal_gearbox_v2.step` assembly frame (cycloid axis at (0, 50); cam 1 at z 20–25, cam 2 at
  z 26.2–31.2).
- `Double_cam_print.stl`: cam 1 face on the bed.
- `make_double_cam.py`: FreeCAD script that builds both.

## Why

The two cams were each held to the rod by their own pen-nib pin. Neither pin went fully into its
(off-centre) hole, and both had play. So each cam could slip on the rod until its nib dropped back into
its hole:
- **≈ 20° of output play.** A cam turning loose for part of a revolution shows up at the output ÷ 10
  (a full cam revolution would be 36°).
- **Intermittent lock-up.** When one cam slips, the two eccentrics are no longer 180° apart. The two cycloids
  then fight each other through the output pins, and the gearbox goes rigid ("like a perfect cycloid"). The
  rod keeps turning inside the jammed cams with no strain, until a nib catches again and it loosens.

With both eccentrics in one part, the phase can't drift. With the nib locked under the bearing, it can't come
out of its hole.

## Geometry

| | Old CAM1 / CAM 2 | **Double cam** |
|---|---|---|
| Bearing seats | Ø10 × 5, eccentric ±1.2, 180° apart | identical (0.00 mm² difference against the CAD; cam 2 within 0.01 mm) |
| Between the cams | 1.2 mm gap | a 1.2 mm **lens-shaped bridge** that stays inside both cam circles, so it only touches the inner races, which turn with it |
| Bore | Ø5 + 2 mm cross slot | **Ø5.15** (prints ≈ Ø5.0: a snug slide fit on the rod with no reaming) |
| Pin | separate nib per cam | **tapered window** into each cam from +y, down to the rod: 1.0 mm wide at the rod, ≈ 1.5 at the cam surface, 3.1 mm tall around the old pin heights (z 22.5, 28.7). Only the window that lines up with a rod hole is used. |
| Lead-in | — | 0.3 mm chamfer on the bottom edge of cam 1 and the top edge of cam 2 |
| Volume | 2 × 274 mm³ | 596 mm³, one valid solid; the STL is closed and manifold |

**No glue: the bearing locks the pin.** The tapered pen nib goes through the window into the rod's existing
hole. When the cycloid's bearing slides over the cam, its inner race covers the window:
- It pushes the nib inward, wedging it into the taper. That removes the play.
- The nib can't back out, which is how the old nibs came loose.

Because the cam is one piece, both cams stay exactly 180° apart whatever the rod hole positions are, so the
cycloids can't fight each other any more.

**Bridge torsion** (it carries cam 2's half of the torque): about 7 MPa interlayer shear at case B and
16 MPa at case E (allowable 22).

**Capacity (rough):**
- One 1 mm nib in single shear ≈ 0.4 N·m on the input. The base worst case (arm straight out) needs 0.96 N·m.
- If the second rod hole happens to line up with cam 2's window, use a second nib: ≈ 0.8 N·m.
- That's about what the two old cams held when their nibs were engaged. It is fine for the arm hanging and for
  slow moves; it is not enough for the arm held straight out with a load.
- If the nib shears, the cam slips there before the face gear strips.
- The stronger fix: drive the cam straight from the face gear through the case. That means swapping the case
  625 for a 10 mm-ID bearing and a printed sleeve/dog coupling, so torque no longer goes through the rod.

## Printing

PETG (same as the old cams), **cam 1 down** as in the STL, 100 % solid, 0.1–0.12 mm layers for round seats.
Cam 2 overhangs the bridge by a 2.4 mm crescent: use **supports, build plate only**. They reach the bed
straight down past cam 1. You can try without supports with good cooling, but check that cam 2's lower
seat edge is clean. About 20 min. Print two.

If your old cams were printed with a fit tweak for the bearings (scale or hole offset), apply the same one.
The seats here are the nominal CAD Ø10.

## Fitting (the output and fork stay on the arm; work from below with the case off)

1. **Before removing the old cams, mark the rod** at the bottom face of the lower cam (CAM1) with a fine
   marker. That height must stay the same.
2. Remove the cycloids and the old cams. Slide the new cam onto the rod (snug), cam 1 down, its bottom face on
   the mark.
3. Turn the cam until a rod hole shows in a window (cam 1's or cam 2's). Push a nib through the window,
   tip first, into the rod hole until it wedges.
   - The nib's back end must sit **at or just below the cam surface**. Cut or file it if it sticks out,
     or the bearing won't slide on.
   - If the nib is too fat and stops high, open the window a little with a hot nib or a knife. If it's too
     thin and sinks, the bearing still traps it; it just won't wedge.
   - If the other rod hole also lines up with the other window, put a nib there too.
4. Cycloid 1 + its bearing onto cam 1, sliding on over the rod's bottom (D) end. Cycloid 2 + its bearing onto
   cam 2, over the rod's top end. The bearing that covers the nib locks it.
5. Then the stack onto the output pins and into the ring pins as before. The rod's top end goes into the
   output's centre bearing. Reassemble the case and the face gear stage.
6. First run: `/test` spin at 10–30 %, then check the output play. It should be about 1–2°, down from 20°.

**Any slip changes the base angle.** After any episode where the input slipped, the firmware's base angle is
wrong. Re-zero before teaching.

## Not verified

- The real bearing fit on the printed seats (same nominal size as the cams that already worked).
- How well the real nib's taper wedges in the window.
- That the rod hole lines up with a window. The windows cover ±1.55 mm around the old pin heights.
- What's left of the play once the cams are fixed. The CAD clearances suggest about 1–2° at the output.
