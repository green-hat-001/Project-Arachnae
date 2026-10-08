# face_gen: worm + face gear generator

Generates the face gear that the worm `WORM_FILE` drives (`params.py`). The worm is a parameter, so a
different worm (for example a 3-start) is a rerun, not a redesign: change `WORM_FILE`, `STARTS`, `LEAD`,
`WORM_TIP`, `WORM_ROOT`, `ZONE` and `N` (stage ratio = N / STARTS; contact sits at r_p = N·LEAD/(2π·STARTS)
from the gear axis, so keep r_p a little inside `CD` = 11.55). Run one at a time (the laptop can't take
parallel jobs). Intermediate files go to `$FACE_SCRATCH` (default `/tmp/face_gen/`).

```
export FACE_SCRATCH=/tmp/face_gen/            # FACE_N, FACE_ZTOP, FACE_PEN override params for experiments
freecadcmd -c "exec(open('fc_export_face.py').read())"   # worm sections (real + scaled cutter); set FACE_GEN=$PWD
uv run --with shapely python hob_face.py                  # virtual hobbing -> slices (~30 s)
uv run --with shapely python verify_face.py               # interference, backlash, misalignment, contact, tooth size
uv run --with scikit-fem --with pyamg --with scipy --with numpy --with shapely python fea_face.py   # ~1 min, ~2 GB
uv run --with shapely --with numpy --with manifold3d --with trimesh --with scipy --with networkx python stl_face.py out.stl [print]
uv run --with shapely --with matplotlib python plot_face.py -1.2,-0.6,0.0   # slice pictures
```

- Hobbing works in planes perpendicular to the gear axis. In each one the worm's section moves along the worm
  axis as the worm turns (helicoid). It is tiled from one clean axial pitch, then clipped to the thread zone.
  The Ø12 body beyond the threads is a fixed solid of revolution. The gear turns (N/STARTS)× slower.
- The case clearance (`CASE_RMIN`) is the far-boss block's minimum radius from the gear axis, measured from
  `Base_cycloidal_gearbox_v2.step` (`Import.insert` + global placements). Re-measure it if the case changes.
- v2 (2026-10-05) is the default: straight rim from the bed, hub flare capped at 30°, case clearance from the
  0.1 mm table `CASE_RMIN_FINE` with a 0.5 mm r–z offset. `FACE_V1=1` rebuilds v1 (45° rim, coarse table).
- `fea_face.py` reuses `../throated_gen/fea_common.py`.
