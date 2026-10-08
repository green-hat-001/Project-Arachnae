# Base servo holder v2 (2026-10-02)

Replaces "Servo Casing for mount" (`Servo Mounting box v6`) in `cad/v2/assemblies/Base_cycloidal_gearbox_v2.step`.

Files:
- `Servo_holder_v2.step` and `Servo_backplate_v2.step`: assembly frame of `Base_cycloidal_gearbox_v2.step` (base axis +Z, worm along +Y).
- `Servo_holder_v2_print.stl` and `Servo_backplate_v2_print.stl`: already oriented for printing.

## Why

A slow-motion video of the base skipping (2026-10-02) showed the old holder rotating 1.5–2.2° relative to the
base case at every skip, then springing back. The audio bursts lined up with the same three moments. In `Base_cycloidal_gearbox_v2.step`
the old holder never touches the case (2.1 mm gap). It hangs from two thin diagonal arms (about 8 × 8 mm), clamped
between nuts on the two front-corner M5 rods about 5 mm below the case ears. All worm mesh load went through those
arms and rods in bending. The holder rocked, the worm walked out of mesh, and the teeth rode over each other: first
the weak worm broke, and now that the worm is strong, the helical gear breaks.

## What changed

- **Flat clamp face at z = 15** across the bosses, both wings and the front frame. The case ear undersides and the
  case floor are one flat plane at z = 15, so the holder now bolts up flat against the case. About 660 mm² sits under
  the housing floor plus about 260 mm² on the two ear pads, where before there was no contact at all.
- **Rod bosses Ø16** (were Ø8), from z −4.6 (unchanged, so the nut and bracket stack below stays the same) up to
  the ear underside. Rod holes Ø5.6.
- **Full-depth side wings** run from the box back to the bosses, replacing the thin arms.
- **Front frame** joins both wings and the box in front of the servo, with a Ø22 clearance hole for the horn and the
  worm flange (old horn clearance 0.51 mm, unchanged).
- **Two cheeks** hug the side walls of the worm cradle on the case (0.4 mm clearance, 3.15 mm thick, y 15.15 to 22),
  right at the worm axis. The mesh always pushes the worm sideways away from the gear, and the gear-side cheek takes
  that straight into the cradle. A 2 mm relief on their inner top edges clears the cradle-to-floor fillet.
- **It goes no lower than the old holder.** In front of the servo box, nothing is below the old boss bottom (z −4.6).
  The only new material below that level is the four bolt-ear blocks beside the back of the servo box, at the box's
  own existing bottom (z −9.4). The Holstex plate isn't in any CAD file, so this stays inside the space the old
  holder already used.
- **The servo now goes in from the back.** The front frame closes the old open front, so the back is open instead
  and closed by a separate **back plate**:
  - It has the same four rear-horn screw holes, centre hole and cable windows as the old back face.
  - It's thickened to 7 mm so it doesn't flex when the worm pulls the servo backwards. The servo screw holes are
    counterbored, so the same screws still grip 2.4 mm.
  - It's held to the holder by 4 × M3 bolts into side-entry nut slots.

The servo tube interior is the old one, unchanged.

Checked in CAD against `Base_cycloidal_gearbox_v2.step` plus the 2-start worm, 10T gear and gear support bracket (bracket placed lip up,
pocket on the gear axis, top at z −6.3):

| Check | Result |
|---|---|
| Case (2D slices every 0.25 mm, z −9 to 19.4) | no overlap; contact only on the z = 15 plane |
| Gear support bracket | 1.7 mm clearance |
| Below the old envelope | nothing in front of the servo box below z −4.6 |
| Worm swept envelope (r 9.7 in the frame, r 7.8 in the cradle) | clear |
| Servo horn | 0.51 mm (same as the old holder) |
| Servo body | same fit as the old holder |
| Back plate to holder | seats flush, no overlap |
| STLs | watertight, manifold, no self-intersections, volumes match the CAD |

## Hardware

- 4 × **M3 × 12 or M3 × 14** bolts and **4 × M3 nuts**, back plate to holder. The nuts drop into slots from the
  outside of each wing, 5 mm in from the back face. The bolt holes are Ø3.4 clearance.
- The servo rear-horn screws: same as before.
- The two M5 rods: same rods. Your stack was top cover / TPU washer / bottom cover / nut / holder / nut / nut /
  Holstex / nut. It becomes top cover / TPU washer / bottom cover / **holder** / nut / nut / Holstex / nut: **drop the
  nut between the bottom cover and the holder.** The boss bottom is at the same height as before, so everything
  from the nut under the holder downwards stays where it was.

## Assembly order

1. Screw the worm onto the servo horn, outside the holder.
2. Slide servo + worm into the holder **from the back, worm first**. It passes through the Ø22 front hole and the
   collar (worm flange Ø18.8, collar Ø19.5).
3. Drop the 4 M3 nuts into their slots. Fit the back plate: rear-horn screws into the servo, then 4 × M3 bolts into the nuts.
4. Slide the holder along the worm axis under the case: the worm goes into the cradle, the rod into the far boss,
   the collar over the cradle, and the top face flat against the case floor and ears.
5. Drop the M5 rods through the ears and bosses. Tighten a nut under each boss so the boss is pulled up hard against
   the ear. Then fit the gear bracket and its nuts below as before.

## Printing

- **Holder:** PETG, back end down (as in the STL), 0.2 mm layers, 5 walls, 40 % gyroid.
  - **Supports: on, build plate only.** They're needed for the front frame's ceiling inside the servo tube and for
    the four nut slots. The ceiling is reachable through the open back end.
  - About 85.7 × 28.8 × 56.8 mm on the plate. 38.1 cm³ of solid (about 48 g at 100 %, roughly 30 g with these settings).
  - The old holder was 14.3 cm³. The extra mass sits at the base, so it adds almost no moment on the arm.
- **Back plate:** PETG, flat, inner face down (as in the STL), counterbores up, no supports.


## Lightening (2026-10-02, late): topology optimisation → pockets

`Servo_holder_v2.step` / `_print.stl` are now the **lightened** holder. The solid one is kept as
`Servo_holder_v2_solid.*`.

Method: the holder was voxelised (1.5 mm) and solved as a linear-elastic FEA (hex8, PETG E = 2000 MPa)
under four load cases at the worm's servo end:
- both rotation directions, with the clamp face bearing on the case;
- both again with the clamp face lifted off, so the rods carry everything.

The loads are a heavy lift: 200 N worm thrust, 43 / 3 N sideways, 19 N up/down, 0.56 N·m servo torque.
The servo tube, rod bosses, 2.5 mm clamp-face skin, cradle cheeks, worm ring and M3 ear blocks were held solid.
A SIMP topology optimisation (30 % of the free material) showed:
- the left wing needs a strut along its outer edge from the boss to the back;
- the right wing only needs the boss-to-box-corner link;
- the block in front of the servo can go.

That was turned into printable CAD pockets, all opening on the underside, with 2 mm rounded corners and short
straight bridged ends: one per wing and one in the front frame. Then each pocket was checked on its own. The
front part of the left-wing pocket cost a lot of stiffness, so that pocket stops at y = 0. The outer wing edges
have 1.5 mm fillets, some bottom edges have 0.8 mm chamfers, and the bed face has a 0.5 mm elephant-foot chamfer.

| | solid | lightened |
|---|---|---|
| volume | 44.5 cm³ | **38.1 cm³ (−14 %)** |
| total compliance (4 load cases) | 4.42 | 5.31 (+20 %) |
| worm-end movement, worst case | 5.3 µm | 6.5 µm |
| peak von Mises at 200 N lift | 0.96 MPa | 1.10 MPa |
| peak at servo stall (~5×) | ~4.8 MPa | ~5.5 MPa (PETG layer strength ~20–30 MPa) |

The clash checks were re-run on the lightened geometry: case, bracket, worm, horn and servo are unchanged.
The optimiser's ideal (~31 cm³) needs organic shapes that don't print back-end down without supports. Most of the
remaining mass is the servo tube and the clamp face, which have to stay.

Renders: `renders/` (lightened), `renders/solid_version/` (before). The optimisation slices are in
`renders/5_topology_optimisation_slices.png`.

## Not verified

- That the real case matches `Base_cycloidal_gearbox_v2.step` on the two contact surfaces: flat ear undersides and floor at the same
  height, and the cradle outer profile. The design depends on both.
- Where the Holstex plate is. It's not in any CAD file, so the holder is kept inside the old holder's envelope.
- That a stiff holder stops the skipping by itself. If it still skips, the next suspects are the worm's far-end
  support in the case and the gear-tip bracket.
