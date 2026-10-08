# Base worm stage: module 1.0, 2-start (2026-10-02)

Replaces `1_worm_4start/` (module 0.5). The ratio, centre distance, lead angle and every
mounting feature are the same. Only the teeth are twice the size.

Why: the module 0.5 worm's thread crests were one 0.4 mm extrusion line thick (~0.42 mm) and
flattened when the gear rode over them. The ~0.9 mm working depth was also small enough that
~1 mm of servo-mount spring-back let the teeth skip.

| | 4-start (old) | **2-start (this)** |
|---|---|---|
| Starts / gear teeth | 4 / 20 | **2 / 10** |
| Ratio | 5:1 | **5:1** (firmware unchanged) |
| Normal module | 0.5 | **1.0** |
| Pressure angle | 20° | **25°** (no undercut on 10 teeth) |
| Lead angle | 8.86° | **8.86°** (same self-locking) |
| Lead / axial pitch | 6.36 / 1.59 | 6.36 / **3.18** |
| Working depth | ~0.9 mm | **1.55 mm** |
| Worm crest thickness (normal) | ~0.42 mm | **~0.87 mm** |
| Worm tip / root Ø | 13.88 / 11.73 | **14.38** / 10.78 |
| Gear tip / root Ø | 11.02 / 8.87 | **11.82** / 8.22 |
| Gear tip land | | 0.60 mm |
| Backlash (normal) / tip clearance | 0.10 / 0.175 | 0.10 / 0.25 |
| Centre distance | 11.55 | **11.55** |

Copied unchanged from the 4-start parts:
- **Worm:** horn flange, 4 × Ø2.7 holes and Ø5 counterbores, Ø5.2 flange centre hole (3 mm),
  Ø12 body, Ø5.2 alignment-rod bore.
- **Gear:** D-bore (Ø5.2, flat 1.43 mm from centre, 12 mm deep), the 1.5 mm Ø8.6 stub hub,
  and the Ø4.9 × 4.3 mm pin hole (pin cut to 11.5 mm).
- **`1_worm_4start/Gear_support_bracket`** still fits.

Both parts are in the same frame as before (the `Input Worm` component frame of `Base_cycloidal_gearbox_v2.step`).

## Verified in CAD
- Both are single valid solids. The STLs are watertight, manifold, with no self-intersections.
- Backlash window at the gear is about 1.2° wide.
- Mesh rotated through a full tooth period (worm 0–180°, gear following at 1/5): no
  interference at any phase, constant 0.044 mm minimum gap.

## Not verified
- Fit of the Ø14.38 worm tip in the printed housing: 0.11 mm larger diameter than the 4-start
  worm, and the `Base_cycloidal_gearbox_v2.step` bore is Ø15.7.
- Self-locking under real PETG friction. The lead angle is the same as the 4-start;
  25° pressure angle slightly *raises* the locking margin.
- 25° teeth push the gear and worm apart ~30 % harder than 20° for the same load. The gear-tip
  bracket carries this on the gear side.

## Printing
PETG, 0.4 mm nozzle, solid (100 % / max walls), same orientation as the previous parts.

## v2 (2026-10-02): print flipped, easier screws, deeper centre hole

Files: `Worm_2start_v2.step` (assembly frame, same as before) and `Worm_2start_v2_print.stl`
(already oriented for printing). The gear is unchanged; keep the printed one.

**Why flipped.** The first 2-start worm was printed flange (screw end) down. Printed upright, each
thread's downward-facing flank is a ~65° overhang of sagging, poorly bonded strands. The arm's
+ direction (lift) loaded that flank: it peeled, then skipped at the damaged spot, in + only and
only ever on the worm. Printing **rod end down, flange up** puts the good, upward-facing flank on
the + (lift, heavy) side and leaves the overhang flank for − (lowering, gravity-assisted).

Changes (thread zone x 6.95–26.95 is untouched, 0 mm³ removed there):
- **Screw pockets Ø5.0 → Ø5.6** for the Ø4.5 M2.5 heads (printed holes shrink; Ø5 jammed).
- **Driver channels:** each pocket continues as a Ø5.6 channel tilted **7° outward** from the head,
  so a Phillips driver sits on the head along a straight line and clears the Ø12 body and the
  thread tips (channel edge at r 7.92 at the thread start vs thread tip r 7.19). Hold the
  driver in the channel; it is the 7° angle the channel sets, not freehand.
- **Centre hole (horn screw head) 3 → 6 mm deep**, still Ø5.2. 6 mm of solid remains before the
  alignment-rod bore.
- Weakest torsion section (x −24 to −22) keeps 85 % of the old one's polar moment; the body at
  the threads is unchanged.

Printing: PETG, **0.1 mm layers** if possible (better thread overhangs), slow outer walls, full
cooling, 100 % / max walls, elephant-foot compensation on (the threads touch the bed). (Superseded 2026-10-03: run
the mesh **dry**, not greased — see the throated-wheel FEA section.) The ring
under the Ø18.8 flange is a ~55° overhang plus four ~5.6 mm bridges over the pockets: no
supports needed with good cooling. Run the mesh dry (see the FEA section: grease defeats self-locking).

Verified: single valid solid; STL watertight, manifold, no self-intersections, volume matches
the CAD (5628.5 vs 5630.1 mm³). Not verified: that the horn screw head sits flush at 6 mm
(depends on the head height plus how far the horn hub stands proud), and that the flipped
orientation cures the + skip on the real arm.

## Throated worm wheel (2026-10-03): `Gear_10T_2start_throated.stl`

Drop-in replacement for `Gear_10T_2start_stub` that wraps around the worm, so the worm's thread
touches it along a line instead of at a point. Use it with the same `Worm_2start_v2` worm.

**Why.** The straight helical gear crosses the worm at an angle, so each tooth touches the worm
thread at a single point. All the load goes through a ~2 mm patch on one thread crest, and the
thread is printed with its layers across that load. A hand estimate puts ~60 MPa on the thread
root at the arm's worst static load (arm straight out, 7.7 N·m at the base, cycloid 10:1),
against ~20–30 MPa layer-bond strength for PETG. That matches the repeated peeling and skipping.

**How it was made.** The same frame and features as `Gear_10T_2start_stub` (D-bore, Ø8.6 stub
hub, Ø4.9 pin hole, 16 mm face, ratio 5:1). The tooth zone is generated by "virtual hobbing":
each 0.05 mm horizontal slice of the blank is cut by the real `Worm_2start_v2` cross-section at
that height, rolled through the mesh in 0.25° gear steps. This is exact, because the worm/gear
relative motion keeps planes perpendicular to the gear axis in-plane. The cutter is the worm
scaled radially by (7.19+0.35)/7.19 about its axis, which gives 0.35 mm tip/root clearance and
the backlash. The blank wraps the worm ±3.8 mm about its axis (throat clearance radius 5.74 mm),
with 45° tapers beyond. Outside ±~5.6 mm from the worm axis the worm never reaches the old tip
radius, so the gear is a plain Ø11.82 ring there (no teeth needed).

Generator and FEA scripts: `throated_gen/` (FreeCAD `fc_export.py` → `hob2d.py` → `verify2d.py` → `stl2d.py`; FEA `patches.py`, `mesh_worm.py`, `worm_fea.py`, `gear_fea.py`; intermediate files go to `$THROAT_SCRATCH`, default `/tmp/throated_gen/`). Run them one at a time: they are memory-heavy.

**Verified (2D-slice simulation against the real, unscaled worm v2):**

| Check | Old helical | Throated |
|---|---|---|
| Interference over a full tooth period (1.5° steps) | none | none, min gap 0.115 mm |
| Backlash at the gear | 1.17° (0.10 mm) | 2.9° (0.26 mm) → ~0.3° at the base |
| Contact area at equal 0.03 mm penetration | 1× | **4.0×** |
| Contact length along the gear axis | ~2 mm (z 4.8–7.1) | **~9 mm** (z 1.4–10.3) |
| Teeth sharing load | 1–2 | 1–3 |
| Gear printed/seated 0.3 mm high or low | — | still clear (0.027 mm) |
| Centre distance 0.15 mm short / 0.3 mm long | — | clear (0.048 / 0.226 mm) |
| Tooth tip land, 0.25 mm below tip | — | 0.47–0.90 mm |
| Max outer radius | 5.91 | 6.94 (case wall ≥ 11.48 around the gear in `Base_cycloidal_gearbox_v2.step`) |
| Overhangs on teeth / throat | — | none steeper than 45° (only the hub step and pin-hole bridge, both as before) |
| STL | — | consistent normals, no non-manifold edges, slices to closed loops matching the design (±0.2 %). Has stair-step T-junctions (0.05 mm layers), so FreeCAD calls it "not solid"; slicers handle it. Volume 1364 mm³. |

Backlash is deliberately larger than the old 0.10 mm: it is what absorbs print error in gear
height and centre distance (with the old value, ±0.3 mm of height error would collide). The arm's
weight keeps one flank loaded, so it rarely shows.

**Thread stress, re-estimated with the 4× contact area:** ~15 MPa at the worst static arm load,
~23 MPa with ×1.5 dynamics, which is now under or at the PETG layer strength rather than 2–3× over it.
If the base jams at the current torque limit (700 = 70 % stall, ~960 N on the tooth), the thread
still sees ~75 MPa: no printed worm survives that. A lower base torque limit is what protects against jams.

**Not verified:** real printed tolerances; that the gear seats at the same height as the old one
(it should: hub, bore and pin are copied unchanged); real PETG contact behaviour.

**Printing:** PETG, hub down (as in the STL), 0.08–0.1 mm layers, 100 % infill / max walls,
no supports, slow outer walls. For layer bonding (the worm cares more than the gear): hotter
nozzle (~245–250 °C) and part fan ~30–50 %. **Run the mesh dry, do not grease it** — see
"Self-locking" under the FEA section: grease drops PETG friction below the self-locking threshold.

## FEA of the base worm stage, worn orientation (2026-10-03)

**Setup.** Base axis horizontal (cycloid plane vertical, as worn on the back), so the base lifts the arm
and the worm mesh carries the arm's full gravity moment about that axis. The other gravity components go
into the cycloid bearings and are not analysed here.
- **Worm v2:** 98k quadratic tets (gmsh + scikit-fem). Clamped 6 mm past the thread zone on the
  flange side; the steel alignment rod is modelled as rigid in the Ø5.2 bore.
- **Gear:** a ±60° sector, 0.15 mm voxels built straight from the hob slices (old gear from its STEP slices).
  Fixed at the rod/pin bores and the sector cuts.
- **Load:** pressure on the actual worst-phase contact patch, taken from the mesh simulation at 0.03 mm
  penetration. Old patch z 5.35–6.95; new z 1.75–10.5.
- **Material:** printed PETG, transversely isotropic with the layer normal along the build axis (the worm
  and gear are both printed axis-vertical). E 2000 in-layer, 1700 across, G 600 across layers.
  Allowables: 45 MPa in-layer tension, 25 MPa across-layer tension, 22 MPa interlayer shear.
  Layer bonding is very sensitive to print settings: ±40 %.
- Stresses are 99.9th percentiles **excluding elements within 0.4 mm of the contact patch** (local contact
  peaks yield and redistribute; they are not what peels a thread). Interlayer shear is ×1.25 for dry
  friction (μ ≈ 0.25), which the model applies only as normal pressure.

**Stress per 100 N of gear-tangential load (= worm axial thrust):**

| | old helical | throated | gain |
|---|---|---|---|
| worm, interlayer shear | 23.4 | 10.7 | 2.2× |
| worm, across-layer tension | 15.0 | 10.3 | 1.5× |
| gear root, in-layer bending | 30.1 | 14.7 | 2.0× |
| gear, across-layer tension | 22.8 | 6.5 | 3.5× |
| gear, interlayer shear | 12.9 | 9.5 | 1.4× |
| radial tooth separation (worm + gear) | — | 0.005 mm | |

Governing mode: **worm thread root, interlayer shear**, in both designs.

**Load cases** (arm 2 kg, 7.7 N·m with the arm straight and horizontal — a CAD-volume estimate; cycloid η 0.8).
Minimum safety factor, throated gear:

| case | cycloid 10:1 (Ft2) | SF | cycloid 20:1 (Ft2) | SF |
|---|---|---|---|---|
| A arm 45° below horizontal | 135 N | 1.2 | 67 N | 2.4 |
| B arm straight, horizontal | 190 N | 0.9 | 95 N | 1.7 |
| C B + 0.5 kg bottle at 0.8 m, or walking ×1.5 | 287 N | 0.6 | 144 N | 1.1 |
| E B + 10 N push at the fingertip | 437 N | 0.4 | 219 N | 0.75 |
| F servo jam at torque limit 700 | 961 N | 0.17 | 961 N | 0.17 |
| old helical, case B | 190 N | 0.4 | 95 N | 0.8 |

These absolute SFs are probably conservative. The original module-0.5, 1-start worm carried the same
gear force and lifted the arm through its full range without failing, which this model says it should
not have. Trust the **ratios** (throated ≈ 2× stronger on the governing mode) more than the absolutes.

**Skipping.** Tooth-level separation is negligible (~0.005 mm per 100 N). Separation comes from the
supports:
- rod-in-bore and far-boss clearances, ≤ 0.1 mm each,
- rod bending, ~0.06 mm at 210 N separating force,
- servo output-shaft play, ~0.05–0.1 mm,
- gear-tip bracket flex, unmodelled, est. 0.05–0.15 mm,
- holder v2, 0.007 mm.

Total ≈ 0.3–0.6 mm at case E, against ~1.0–1.2 mm before the 1.55 mm working depth rides over: SF ≈ 2.
A jam (case F) reaches the skip threshold.

**Self-locking.** Lead 8.86°, normal pressure angle 25° → self-locking needs μ > tan 8.86° × cos 25° = **0.14**.
Dry PETG on PETG (μ ≈ 0.2–0.3) locks. **Greased PETG (μ ≈ 0.05–0.1) does not**, so the arm could drive the
base down and backdrive the servo on E-STOP / OTA / power loss. Run the mesh dry.

**What would raise the margin, in order of cost:**
1. Measure the cycloid ratio (spin-test turn counter): 20:1 doubles every SF above.
2. Print the worm for layer strength: ~250 °C, part fan 20–30 %, slow outer walls, 100 % solid threads.
3. Demo poses that avoid the arm straight and horizontal with a payload; low speeds.
4. A lower base torque limit and fast stall detection, against jams (case F).
5. Real fix, later: a bigger stage, or a tougher worm material (PC, nylon, CF-nylon).

**Not modelled:** fatigue, impact, the explicit friction field, printed-flank defects (overhang side), the cycloid,
the case/bracket/bolted-joint compliance (estimated by hand above), the real arm mass (CAD-volume estimate).

## Failure 2026-10-03 and fix: the throated gear must be held along its axis

**What happened.** First run on the bench, base lying flat. It ran, then strained, then skipped. The gear's
throat lips at the D-bore end were folded over and torn; the pin end was nearly clean. The worm survived.

**Cause (about 75 % confidence).** A throated gear only clears the worm while its throat stays centred on
the worm axis: the 2D check gives about **±0.3 mm** of tolerance along the gear axis. Nothing held it there:
- The D-bore (Ø5.2) slides freely on the Ø5 rod.
- Hub to bracket bearing: 2.0 mm (0.8 mm gap + 1.2 mm lip).
- Gear top to case bearing: 0 or 1 mm. The case pocket is a 4 mm through-hole (asm z 13–17) holding a
  5 mm wide 625, and the gear top is at asm z 12.

While driving, the worm pushes the gear along its axis with about 0.3–0.4 × the tooth force (about 30–80 N
under the arm, up to about 150 N at a jam). The gear slid until its lips hit the worm. The lips took the
thrust, folded, and the worm climbed out. A plain gear only lets its contact patch wander if it slides,
which is why the earlier gears did not fail this way. This was not checked when the throated gear was
designed.

**Fix: sandwich the gear between the two 625 inner races.**
- `thrust_washers/Thrust_washer_*.stl`: Ø7.0 × Ø5.4 rings that touch only the inner race. Thicknesses
  0.5 / 1.0 for the top and 1.6–2.0 for the bottom. Print in PETG at 0.10 mm layers so the thicknesses
  come out exact.
- **Top**, on the rod between the gear top face and the case 625:
  - If the bearing sticks about 1 mm down into the gear cavity (designed), use no washer.
  - If it is flush with the cavity ceiling, use 1.0 mm.
  - The gear must sit hard against it.
- **Bottom**, on the pin between the hub and the bracket 625: the CAD gap is 2.0 mm.
  - Use the thickest washer (1.9 → 1.8 → 1.7) that still lets the bracket seat flat on its bolts without
    being forced.
  - Target axial play: 0.05–0.15 mm.
  - With the worm in place you cannot feel this play, because the throat masks it. Judge by the bracket
    seating, or measure with calipers before fitting the bracket: hub face to bracket seat, and bracket
    seat to inner-race face.
- **Glue both 625 outer races in** with a small drop of thin CA at the rim. Do not let it wick into the
  balls. Thrust pushes the case bearing toward the cycloid and the bracket bearing away from its lip, and
  both pockets rely on press fit alone.
- Gear: reprint the same `Gear_10T_2start_throated.stl` in PETG. Worm: reuse v2 if undamaged. Mesh
  **dry**.
- First run: `/test` spin at 10 % for a few base turns each way, base flat, filmed. Then inspect the lip
  edges at both ends. They should be untouched.

The worn-orientation margin from the FEA section still applies: about 0.9 SF with the arm straight out at 10:1.

## D-bore 0.5 mm deeper (2026-10-04): `Gear_10T_2start_throated_bore12.5.stl`

Same throated gear with the D-bore (top face, Ø5.2, flat 1.43 mm from centre) 12.5 mm deep instead
of 12.0: I wanted the bore slightly deeper for the replacement prints. Only the bore's floor moved, from z 1.025 to 0.525 in the gear frame.
The bore walls were extended by copying the existing 0.05 mm wall layer, so the vertices match
exactly. Nothing else changed: removed volume 8.80 mm³ = 17.60 mm² × 0.5; no non-manifold edges,
same normals and self-intersection flags as the original (the stair-step T-junctions); the
cross-sections at z 0.55–0.95 show the same D.

**The limit:** the pin hole (Ø4.9 from the hub end) ends at z −0.175, so only **0.7 mm** of floor
now separates the two (it was 1.2). That floor stops the pin from sliding into the bore. Going 1 mm
deeper would leave 0.2 mm, effectively a through-hole. Print it solid as before.

## Face gear, 20 teeth (2026-10-04): `Face_gear_20T_2start*.stl` — plan A, replaces the worm wheel

A "helicon" stage: the **same `Worm_2start_v2` worm** lies across the top face of a face gear on the cycloid
input axis. The face gear sits **under** the worm, where the throated wheel's lower half was. It is a drop-in
reprint: same worm, holder v2, rod, Ø4.9 pin, gear bracket, thrust washers and case 625. **No case cutting is
needed.**

Files:
- `Face_gear_20T_2start.stl`: in the `Input Worm` component frame, the same frame as the throated gear STL.
- `Face_gear_20T_2start_print.stl`: same part with the flat underside on z = 0, ready to print.
- Generator, checks and FEA: `face_gen/` (see its README).

**Why.** Every worm-wheel version failed by the mesh opening sideways, or by the wheel sliding along its axis.
With the contact under the worm, the separating force acts along the gear axis instead. It pushes the gear down
onto the bracket bearing and the worm up. **Correction (after the first print failed): sitting low is NOT harmless.** It clears the worm, but the
engagement shrinks: tips at z 0.1, worm tip reaches −1.1, so the engagement is ≈ 1.2 mm − (drop). The top ~0.4 mm of each
tooth is the thin (≈ 0.45 mm) tip land. A 0.5–1 mm drop leaves only those tips in mesh, and they shear off. The
separating force pushes the gear down, so **any gap under it is taken up under load**.

| | throated wheel | **face gear 20T** |
|---|---|---|
| Stage ratio | 5:1 | **10:1** → base **100** (cycloid 10:1, confirmed) |
| Base speed at the same servo speed | 1× | **0.5×** |
| Tooth force at case B (arm straight out, 7.7 N·m) | 190 N | **95 N** |
| Separating force, case B | ≈ 90 N **sideways** | ≈ 45 N **along the gear axis** (down); sideways only ≈ 9 N + sliding friction (≈ ±25 N, reverses with direction) |
| Axial position tolerance | ±0.3 mm (lips fold) | **+0.25 mm up; down costs engagement 1:1** (0.5 mm low = half depth, on the thin tips) |
| Backlash at the gear | 2.9° | 1.3° (0.23 mm) → ~0.13° at the base |
| Self-locking | λ 8.9°, μ > 0.14 | same worm, contact radius 6.5–7.2 → λ 8.5–9°, μ > 0.14: **run dry** |

**Geometry** (component frame; asm z = z − 1):
- Pitch radius r_p 10.12 mm (20 × 3.18 / 2π). Contact runs along the worm, 1.43 mm inside the worm-axis line
  toward the gear axis, at most 12° off straight below. It spans about 15 mm along the worm, with 1–3 teeth in mesh.
- Teeth: generated by virtual hobbing with the real worm section, scaled radially by (7.19 + 0.35)/7.19, as for
  the throated wheel. Tips at z 0.1 (working depth about 1.1 mm). Every slice is filtered so nothing thinner
  than about 0.45 mm (one extrusion line) is left.
- 18–23 teeth were compared. 20 has the best tooth stress at load and the longest contact path. 18 is about
  10 % faster but contacts 2.4 mm off-axis (≈ 17 N sideways). 23 would sit exactly under the axis, but its
  teeth reach past the far-boss block (case cutting) and under the worm's Ø12 body.
- Web from z −4.5 to the teeth, flat underneath out to r 10 with a 45° edge. Outer radius follows the far-boss
  block at **0.45 mm design clearance (0.80 mm measured min. gap)**.
- Hub: rises along the rod, clearing the real worm tip by 0.5 mm. It is **Ø7.7 at the worm-axis height**,
  1.26 mm wall around the D-bore, the thinnest section of the part. It flares to Ø12 above and below.
  The top 0.8 mm is Ø7 so it touches only the 625 inner race.
- **Same seats as the throated gear:** top face z 13.0, D-bore Ø5.2 with the flat 1.43 mm from centre, 12.5 mm
  deep, Ø4.9 × 4.3 pin hole from the bottom (0.7 mm floor between them), bottom face z −4.5. So the bracket,
  thrust washers and pin stack are unchanged. The underside sits 0.8 mm above the bracket top (as the old hub did).

**Verified** (2D slices every 0.05 mm against the real, unscaled worm; scripts in `face_gen/`):

| Check | Result |
|---|---|
| Interference over a full tooth period | none, min gap 0.108 mm |
| Gear 0.3 / 0.6 mm **low** | no interference (0.23 / 0.36 mm), backlash 3.0° / 4.6°, **but engagement only ≈ 0.9 / 0.6 mm** |
| Gear 0.3 mm **high** | just touches (0.005 mm³) — **don't seat it high** |
| Centre distance −0.2 / +0.3 mm | clear (0.06 / 0.11 mm) |
| Case (`Base_cycloidal_gearbox_v2.step`), holder v2, 4-arm bracket | min gap 0.80 mm (far-boss block); bracket 0.8 mm below; holder not near |
| STL | one watertight, consistently wound manifold solid (manifold3d union of the slices), 2297 mm³ |

**FEA** (`face_gen/fea_face.py`): a ±38° sector of the gear, voxels 0.15 mm, PETG printed flat (layer normal =
gear axis), worst phase. Load is 100 N tangential + 47 N axial, on the 0.1 mm-penetration patch (3 teeth;
measured tooth deflection ≈ 0.08 mm per 100 N, so 0.1 mm is the realistic sharing). Per 100 N, 99.9th
percentile, > 0.45 mm from contact:
- interlayer shear **25.7 MPa** (governing; allowable 22),
- in-layer 31.4 (allowable 45),
- across-layer tension 12.6 (allowable 25).

At 0.03 mm penetration (1–2 teeth, too pessimistic) it is about 1.6× higher.

| case (cycloid 10:1) | tooth force | face gear tooth SF | throated: worm thread SF (was governing) |
|---|---|---|---|
| A arm 45° below horizontal | 67 N | **1.3** | 1.2 |
| B arm straight, horizontal | 95 N | **0.9** | 0.9 |
| C B + 0.5 kg bottle, or walking ×1.5 | 143 N | **0.6** | 0.6 |
| E B + 10 N push at the fingertip | 219 N | **0.4** | 0.4 |
| jam at base torque limit 350 | ≈ 360–480 N | **≈ 0.2** | 0.17 (at limit 700) |

So the stage is **about as strong as the throated wheel was. It is not stronger.** What it adds:
- It can't fail the way the wheels did: no sideways opening, no lips to fold. Axial slop is not harmless, though: see the correction above.
- The weak point is now a 3 g gear printed flat, not the worm.
- The worm thread carries half the force. Its stress under this shorter contact was not re-analysed, so its
  per-newton stress should fall between the throated value (10.7 MPa/100 N) and the helical one (23.4).

As before, the absolute SFs are probably conservative. Keep demo poses away from "arm straight out with a
payload" and keep speeds low.

**Axial stiffness of the bracket now sets the skip margin.** The gear pushes down on the bracket bearing:
about 45 N at case B, against 66 N for the throated wheel. Scaled from the 4-arm bracket FEA, that's
0.25–0.35 mm of sag at B and 0.6–0.8 mm at E, against a working depth of about 1.1 mm. Worm lift (rod/far-boss
clearance, servo shaft play) adds about 0.1–0.2 mm. B is fine; E approaches skipping. The bracket's sideways
load drops from ~90 N to ~10–35 N.

**Hub torsion.** At the Ø7.7 throat, the torque that enters above it gives about 7.5 MPa interlayer shear at
case B (SF ≈ 3). This assumes the rod reaches the bore floor, as it did with the 12.5 mm bore. If the rod only
engaged above the worm, it would be about 13 MPa (SF 1.6).

**Fitting** (same as the throated gear, see "Failure 2026-10-03 and fix" above):
- Pin, bottom thrust washer and bracket 625 as before. Choose the **thickest bottom washer that lets the bracket
  seat flat without forcing**, **zero gap under the gear** (that is what the gear rests on under load). Up is limited by the top washer.
- Top washer to the case 625 as before: none if the bearing sticks about 1 mm down, 1.0 mm if it's flush.
  Keep axial play at 0.05–0.15 mm.
- Glue both 625 outer races in. Mesh **dry**.
- The rod must go all the way down into the D-bore.
- First run: `/test` base spin at 10 %, base flat and filmed, a few worm turns each way. Then look at the tooth
  tips under the worm.

**Printing:** PETG, flat underside on the bed (as in `_print.stl`), teeth and hub up, no supports. Use
0.08–0.1 mm layers (the teeth are only ~1.1 mm tall), 100 % infill / max walls, slow outer walls, nozzle
245–250 °C and part fan 30–50 % for layer bonding. The hub throat's overhang is ≤ 45°. About 3 g.

**Firmware after fitting:**
- Base ratio **100** in `/teach` → Joint calibration (NVS holds 50).
- Base backlash ≈ 150 servo steps (1.3° at the gear × 10 → 13° of worm). Measure it.
- The base direction should be unchanged: the contact is on the same side of the gear axis as the wheel's.
  Still confirm with a jog.
- Keep the torque limit at 350: the jam force per unit servo torque has not changed.
- The base is now half as fast.

**Not verified:** how cleanly 1.1 mm FDM teeth print; real PETG friction (self-locking); the worm thread's
stress under the new contact; bracket sag under real load; that the rod reaches the bore floor.

## Face gear v2 (2026-10-05): `Face_gear_20T_2start_v2*.stl` — printability, same mesh

v1 failed to print: the rim was flat underneath out to r 10 and then **flared out at 45° straight off the bed**,
and in PETG that ring curled up and blobbed. The hub above the worm also flared out, reaching 44°. Neither
overhang was needed: the case's far-boss block only comes within 15 mm of the gear axis above comp z −2.9,
and the bracket sits flat 0.8 mm below.

| | v1 | **v2** |
|---|---|---|
| Rim | flat to r 10, then 45° out | **straight up from the bed at r 14.5**, then stepping *in* with the case (no overhang) |
| Hub flare above the worm | up to 44° | **capped at 30°** (`HUB_OVH`); the Ø7.7 throat is unchanged |
| Overhangs steeper than 30° | rim ring + hub flare (116 mm²) | **only the Ø4.9 pin-hole ceiling** (a 4.9 mm bridge, same as v1) |
| Case gap, measured on the STL | 0.40 mm (not the 0.80 noted above) | **0.48 mm** (designed 0.5 normal) |
| Bracket gap | 0.80 | 0.80 |
| Volume / mass | 2298 mm³ / ≈ 2.9 g | 2886 mm³ / ≈ 3.7 g |

The case clearance was re-measured every 0.1 mm (`CASE_RMIN_FINE`). Interpolating the coarse table overstated
the room, and under the teeth the block's underside is shallow, so v2 offsets the case profile by a true 0.5 mm
in the r–z plane instead of subtracting a radial clearance. The generator rebuilds v1 with `FACE_V1=1`.

**The teeth and every seat are unchanged.** Same worm, hobbing, Z_TOP, clearance, D-bore, pin hole, top/bottom
faces and fitting procedure. `verify_face.py` gives results identical to v1 (no interference, min gap 0.108 mm,
backlash 1.27–1.36°, same ±0.3 mm axial / centre-distance results, contact band r 9.8–12.2, same tooth
thickness). The outer tooth ends past r ≈ 12.6 are trimmed by up to 0.15 mm by the new case offset, which is
outside the contact band.

**Tooth strength: no geometric gain is available with this worm.** The hob already leaves the largest tooth the
worm can clear. The tips above z 0.1 would be thinner than one extrusion line, so raising Z_TOP adds nothing.
Shrinking the cutter clearance (`CLR`) gains about 5 % thickness at the cost of the fit. The strength lever
left is layer bonding (interlayer shear governs, see the FEA above), which v2 makes easier because nothing
overhangs any more, so the part fan can stay low:

**Printing v2:** PETG, `_v2_print.stl` as is (flat underside down), no supports, no brim needed (the bed face is
now a full Ø29 disc). Layers 0.08–0.1 mm, 100 % infill, nozzle 250 °C, **part fan 20–30 %**, outer walls
≈ 20 mm/s. Each layer of the teeth is tiny, so give it time to cool without fan: **print two copies about 40 mm
apart** (you get a spare), or set the slicer's minimum layer time to about 10 s with "slow down" rather than
more fan. Clean the tooth tips with a blade, not by snapping off strings.
