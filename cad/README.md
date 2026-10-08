# CAD

All the CAD for both generations of the arm. Everything here is in Git LFS. To get just the code, clone with
`GIT_LFS_SKIP_SMUDGE=1`, then `git lfs pull --include="cad/v2/assemblies/*"` (or any other path) for the parts you want.

Every file that was renamed, and why, is listed in [`RENAMES.md`](RENAMES.md).

| Format | Opens in | Notes |
|---|---|---|
| `.step` / `.stp` | FreeCAD, Fusion, Onshape, SolidWorks | the editable geometry |
| `.3mf` | Bambu Studio, OrcaSlicer, PrusaSlicer | print files, most of them already oriented and plated |
| `.stl` | any slicer | `*_print.stl` files are already oriented for printing |
| `.f3z` | Fusion 360 only | the native V1 archive |

## `v1/`: Arduino + NEMA 23 + hobby servos (Nov 2025 to Apr 2026)

| Folder | What |
|---|---|
| `assemblies/` | the whole V1 arm: `V1_full_arm_assembly_v1.step` (Feb 2026 export), `_v2.step` + `_v2.f3z` (Apr 2026) |
| `iteration_1/` | the first base: a 20T/60T spur-gear segment, then a NEMA 23 pulley turntable |
| `iteration_2/` | base casing, turntable, stepper case, shoulder gears, tendon lock (Dec 2025) |
| `iteration_2.5/` | a rebuilt base casing, bicep/shoulder parts, the tendon-driven hand and its second version, forearm with timing belts (Dec 2025 to Jan 2026) |
| `iteration_3/` | belt linkages for every segment, the electronics bay (Arduino, converters, PSU, stepper driver) and new pulleys (Jan to Feb 2026) |
| `print_plates/` | the first base casing (Nov 2025) and a full base-casing print plate |
| `pulley_experiments/` | outputs of a pulley generator I tried in March |

A part name that appears in several iterations (for example `Big_Shoulder_driving_gear`) is the same part redesigned;
the iteration folder is its version.

## `v2/`: XIAO ESP32-S3 + ST3215 serial servos (May 2026 to now)

| Folder | What |
|---|---|
| `assemblies/` | `Full_arm_v1/v2`, `Shoulder_v1/v2/v3`, `Elbow_v1/v2`, `Base_cycloidal_gearbox_v1/v2`, `Gripper_and_forearm_v2`, `Hand_module_ESP32CAM` (the first hand, built around an ESP32-CAM before I moved the camera onto the XIAO) |
| `printed_parts/` | the printable parts by segment: `base/`, `upper_arm/`, `elbow/`, `forearm/`, `gripper/`, `battery/`, `misc/` |
| `pulleys/` | HTD3M timing pulleys and belts. The elbow pulley went from 40T to 38T and the forearm driver from 36T to 34T in July/August |
| `base_drive/` | **the base gearbox story**, in the order I tried things (below) |
| `tool_modules/pencil_module/` | a twist-on pencil tool for the gripper's quarter-turn mount. Designed, not printed yet |

### `v2/base_drive/`

The base joint is driven servo → worm → gear → 10:1 cycloidal reducer → arm. It is the joint that lifts the whole
arm, and it is where most of my time went. The full story is in [`docs/BASE_GEARBOX.md`](../docs/BASE_GEARBOX.md).

| Folder | Status | What |
|---|---|---|
| `1_worm_4start/` | replaced | 4-start module-0.5 worm and 20T helical gear (5:1), and the first V-shaped gear-tip bracket. The teeth were too thin to print. |
| `2_worm_2start_and_face_gear/` | latest | 2-start module-1.0 worm (5:1 with a 10T gear, then 10:1 with a 20T face gear). Holds the helical stub gear, the throated gear, the face gear v1/v2, the thrust washers, the four-arm bracket that failed, and the Python generators and FEA scripts that produced the throated and face gears |
| `servo_holder_v2/` | built | a stiff servo holder that clamps flat to the case, lightened 14 % with topology optimisation |
| `cycloid_gearbox/` | built | the two cycloidal discs, and the one-piece double cam that replaced two separately pinned cams |

Each folder has its own README with the geometry, checks and printing notes.

## Frames and units

Millimetres throughout. Files in `base_drive/` use the `Base_cycloidal_gearbox_v2.step` assembly frame
(base axis +Z), except the worm-stage parts, which use the worm component's frame (explained in that folder's README).
`Full_arm_v2.stp` is modelled with the arm straight along the base axis. It is the source of the link lengths and belt
ratios in [`firmware/Final_Firmware_Main/config.h`](../firmware/Final_Firmware_Main/config.h): shoulder 6.667:1, elbow 6:1,
forearm 2.25:1, shoulder-to-elbow 189.2 mm, elbow-to-wrist 400 mm, wrist-to-grasp-centre about 475 mm.
