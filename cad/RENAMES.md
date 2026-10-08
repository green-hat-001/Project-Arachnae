# CAD rename log

Every CAD file in this repo, where it came from on my laptop, and why its name changed.
Files that kept their name are listed too, so nothing is untraceable. Dates are the file's last-modified date.

**Same name, different file.** Where two or more different files had the same name
(the shoulder, elbow, base gearbox and full-arm assemblies, the forearm top cover, the camera housing
parts, the axle end cap), I compared them by hash, date and contents and numbered them `_v1`, `_v2`, ...
in date order. The reason for each change is in the note column when I know it.

The old repo root had `Base V3.step` / `Base V3.f3z`. Despite the name, that file is the **whole V1 arm**,
not a base, so it is now `cad/v1/assemblies/V1_full_arm_assembly_v2.*`.

| Original file | Now | Date | Note |
|---|---|---|---|
| `Base V3.step`, `Base V3.f3z` (old repo root) | `cad/v1/assemblies/V1_full_arm_assembly_v2.step` / `.f3z` | 2026-04-07 | the whole V1 arm, not just a base |
| `Misc/Project Arachnae.step` | `cad/v1/assemblies/V1_full_arm_assembly_v1.step` | 2026-02-24 | was Misc/Project Arachnae.step; earlier export (2026-02-24) of the same V1 assembly as v2 |
| `V1/IronSpider files/Version 1/Base (Segment 1) v5.step` | `cad/v1/iteration_1/Base_segment_1_spur_gear_v5.step` | 2025-11-08 | renamed: 20T/60T spur-gear base |
| `V1/IronSpider files/Version 1/Base V2.step` | `cad/v1/iteration_1/Base_NEMA23_turntable_v2.step` | 2025-12-05 | renamed: NEMA 23 pulley turntable base |
| `V1/IronSpider files/Version 2/Base Belt.stl` | `cad/v1/iteration_2/Base_Belt.stl` | 2025-12-16 | name tidied |
| `V1/IronSpider files/Version 2/Base Bushing.3mf` | `cad/v1/iteration_2/Base_Bushing.3mf` | 2025-12-13 | name tidied |
| `V1/IronSpider files/Version 2/Base Casing.3mf` | `cad/v1/iteration_2/Base_Casing.3mf` | 2025-12-13 | name tidied |
| `V1/IronSpider files/Version 2/Base Cover .3mf` | `cad/v1/iteration_2/Base_Cover.3mf` | 2025-12-10 | name tidied |
| `V1/IronSpider files/Version 2/Base tendon lock.3mf` | `cad/v1/iteration_2/Base_tendon_lock.3mf` | 2025-12-15 | name tidied |
| `V1/IronSpider files/Version 2/Bevel Gear cover (1).3mf` | `cad/v1/iteration_2/Bevel_Gear_cover.3mf` | 2025-12-10 | name tidied |
| `V1/IronSpider files/Version 2/Big Shoulder driving gear.3mf` | `cad/v1/iteration_2/Big_Shoulder_driving_gear.3mf` | 2025-12-15 | name tidied |
| `V1/IronSpider files/Version 2/Servo Lck.3mf` | `cad/v1/iteration_2/Servo_Lock.3mf` | 2025-12-15 | name tidied |
| `V1/IronSpider files/Version 2/Small Shoulder gear.3mf` | `cad/v1/iteration_2/Small_Shoulder_gear.3mf` | 2025-12-15 | name tidied |
| `V1/IronSpider files/Version 2/Stepper Motor Case.step` | `cad/v1/iteration_2/Stepper_Motor_Case.step` | 2025-12-10 | name tidied |
| `V1/IronSpider files/Version 2/Stepper Pulley gear.3mf` | `cad/v1/iteration_2/Stepper_Pulley_gear.3mf` | 2025-12-10 | name tidied |
| `V1/IronSpider files/Version 2/Turntable.3mf` | `cad/v1/iteration_2/Turntable.3mf` | 2025-12-15 |  |
| `V1/IronSpider files/Version 2.5/Band tensioner.3mf` | `cad/v1/iteration_2.5/Band_tensioner.3mf` | 2026-01-10 | name tidied |
| `V1/IronSpider files/Version 2.5/Base Casing V2.5.3mf` | `cad/v1/iteration_2.5/Base_Casing_V2.5.3mf` | 2025-12-17 | name tidied |
| `V1/IronSpider files/Version 2.5/Base Cover .3mf` | `cad/v1/iteration_2.5/Base_Cover.3mf` | 2026-01-10 | name tidied |
| `V1/IronSpider files/Version 2.5/Base pulley plug .3mf` | `cad/v1/iteration_2.5/Base_pulley_plug.3mf` | 2026-01-10 | name tidied |
| `V1/IronSpider files/Version 2.5/Turntable .3mf` | `cad/v1/iteration_2.5/Turntable.3mf` | 2026-01-10 | name tidied |
| `V1/IronSpider files/Version 2.5/Shoulder/Bicep Bottom cover.3mf` | `cad/v1/iteration_2.5/shoulder/Bicep_Bottom_cover.3mf` | 2025-12-20 | name tidied |
| `V1/IronSpider files/Version 2.5/Shoulder/Bicep Bottom.3mf` | `cad/v1/iteration_2.5/shoulder/Bicep_Bottom.3mf` | 2025-12-20 | name tidied |
| `V1/IronSpider files/Version 2.5/Shoulder/Big Shoulder driving gear.3mf` | `cad/v1/iteration_2.5/shoulder/Big_Shoulder_driving_gear.3mf` | 2025-12-17 | name tidied |
| `V1/IronSpider files/Version 2.5/Shoulder/Forearm Timing Belt.stl` | `cad/v1/iteration_2.5/shoulder/Forearm_Timing_Belt.stl` | 2026-01-14 | name tidied |
| `V1/IronSpider files/Version 2.5/Shoulder/Lettering.3mf` | `cad/v1/iteration_2.5/shoulder/Lettering.3mf` | 2025-12-22 |  |
| `V1/IronSpider files/Version 2.5/Shoulder/Middle Bicep Cover.3mf` | `cad/v1/iteration_2.5/shoulder/Middle_Bicep_Cover.3mf` | 2025-12-21 | name tidied |
| `V1/IronSpider files/Version 2.5/Shoulder/Middle Bicep.3mf` | `cad/v1/iteration_2.5/shoulder/Middle_Bicep.3mf` | 2025-12-21 | name tidied |
| `V1/IronSpider files/Version 2.5/Shoulder/Shoulder Small pulley .3mf` | `cad/v1/iteration_2.5/shoulder/Shoulder_Small_pulley.3mf` | 2025-12-17 | name tidied |
| `V1/IronSpider files/Version 2.5/Shoulder/Small Shoulder gear.3mf` | `cad/v1/iteration_2.5/shoulder/Small_Shoulder_gear.3mf` | 2026-01-14 | name tidied |
| `V1/IronSpider files/Version 2.5/Shoulder/Top Bicep Cover.3mf` | `cad/v1/iteration_2.5/shoulder/Top_Bicep_Cover.3mf` | 2025-12-22 | name tidied |
| `V1/IronSpider files/Version 2.5/Shoulder/Top Bicep.3mf` | `cad/v1/iteration_2.5/shoulder/Top_Bicep.3mf` | 2025-12-22 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Bearing Cage.3mf` | `cad/v1/iteration_2.5/hand/Bearing_Cage.3mf` | 2025-12-30 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Finger Mount V2.5.3mf` | `cad/v1/iteration_2.5/hand/Finger_Mount_V2.5.3mf` | 2025-12-30 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Finger.3mf` | `cad/v1/iteration_2.5/hand/Finger.3mf` | 2025-12-30 |  |
| `V1/IronSpider files/Version 2.5/Hand/Fingertip sleeve.3mf` | `cad/v1/iteration_2.5/hand/Fingertip_sleeve.3mf` | 2025-12-31 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Hand Mount V2 .3mf` | `cad/v1/iteration_2.5/hand/Hand_Mount_V2.3mf` | 2025-12-30 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Locking Pin.3mf` | `cad/v1/iteration_2.5/hand/Locking_Pin.3mf` | 2025-12-30 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Pulley assembly.3mf` | `cad/v1/iteration_2.5/hand/Pulley_assembly.3mf` | 2025-12-31 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Tensioner.3mf` | `cad/v1/iteration_2.5/hand/Tensioner.3mf` | 2025-12-30 |  |
| `V1/IronSpider files/Version 2.5/Hand/Hand V2/Base.3mf` | `cad/v1/iteration_2.5/hand/hand_v2/Base.3mf` | 2026-01-11 |  |
| `V1/IronSpider files/Version 2.5/Hand/Hand V2/Bearing cage.3mf` | `cad/v1/iteration_2.5/hand/hand_v2/Bearing_cage.3mf` | 2026-01-06 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Hand V2/Filler peice.3mf` | `cad/v1/iteration_2.5/hand/hand_v2/Filler_piece.3mf` | 2026-01-06 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Hand V2/Finger Mount.3mf` | `cad/v1/iteration_2.5/hand/hand_v2/Finger_Mount.3mf` | 2026-01-06 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Hand V2/Finger Servo Lock Peice.3mf` | `cad/v1/iteration_2.5/hand/hand_v2/Finger_Servo_Lock_Piece.3mf` | 2026-01-15 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Hand V2/Finger mount inner race .3mf` | `cad/v1/iteration_2.5/hand/hand_v2/Finger_mount_inner_race.3mf` | 2026-01-11 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Hand V2/Locking Pin.3mf` | `cad/v1/iteration_2.5/hand/hand_v2/Locking_Pin.3mf` | 2026-01-14 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Hand V2/Locking Pins.3mf` | `cad/v1/iteration_2.5/hand/hand_v2/Locking_Pins.3mf` | 2026-01-11 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Hand V2/Pulley Assembly.3mf` | `cad/v1/iteration_2.5/hand/hand_v2/Pulley_Assembly.3mf` | 2026-01-06 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Hand V2/Tensioner.3mf` | `cad/v1/iteration_2.5/hand/hand_v2/Tensioner.3mf` | 2026-01-06 |  |
| `V1/IronSpider files/Version 2.5/Hand/Hand V2/Top cover.3mf` | `cad/v1/iteration_2.5/hand/hand_v2/Top_cover.3mf` | 2026-01-15 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Hand V2/Top lock 1.3mf` | `cad/v1/iteration_2.5/hand/hand_v2/Top_lock_1.3mf` | 2026-01-15 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Hand V2/Top lock 2.3mf` | `cad/v1/iteration_2.5/hand/hand_v2/Top_lock_2.3mf` | 2026-01-15 | name tidied |
| `V1/IronSpider files/Version 2.5/Hand/Hand V2/Top lock pin.3mf` | `cad/v1/iteration_2.5/hand/hand_v2/Top_lock_pin.3mf` | 2026-01-15 | name tidied |
| `V1/IronSpider files/Version 2.5/Forearm/Big Forearm Pulley.stl` | `cad/v1/iteration_2.5/forearm/Big_Forearm_Pulley.stl` | 2026-01-16 | name tidied |
| `V1/IronSpider files/Version 2.5/Forearm/Forearm Bottom Cover.3mf` | `cad/v1/iteration_2.5/forearm/Forearm_Bottom_Cover.3mf` | 2025-12-31 | name tidied |
| `V1/IronSpider files/Version 2.5/Forearm/Forearm Bottom.3mf` | `cad/v1/iteration_2.5/forearm/Forearm_Bottom.3mf` | 2025-12-31 | name tidied |
| `V1/IronSpider files/Version 2.5/Forearm/Forearm Pulley.3mf` | `cad/v1/iteration_2.5/forearm/Forearm_Pulley.3mf` | 2025-12-24 | name tidied |
| `V1/IronSpider files/Version 2.5/Forearm/Forearm Top Cover.3mf` | `cad/v1/iteration_2.5/forearm/Forearm_Top_Cover.3mf` | 2026-01-01 | name tidied |
| `V1/IronSpider files/Version 2.5/Forearm/Forearm Top.3mf` | `cad/v1/iteration_2.5/forearm/Forearm_Top.3mf` | 2026-01-01 | name tidied |
| `V1/IronSpider files/Version 2.5/Forearm/Forearm belt.stl` | `cad/v1/iteration_2.5/forearm/Forearm_belt.stl` | 2025-12-26 | name tidied |
| `V1/IronSpider files/Version 2.5/Forearm/Small Forearm Pulley.stl` | `cad/v1/iteration_2.5/forearm/Small_Forearm_Pulley.stl` | 2026-01-16 | name tidied |
| `V1/IronSpider files/Version 3/15 teeth pulley.3mf` | `cad/v1/iteration_3/15_teeth_pulley.3mf` | 2026-02-06 | name tidied |
| `V1/IronSpider files/Version 3/20 teeth servo pulley.3mf` | `cad/v1/iteration_3/20_teeth_servo_pulley.3mf` | 2026-02-01 | name tidied |
| `V1/IronSpider files/Version 3/30 teeth pulley.3mf` | `cad/v1/iteration_3/30_teeth_pulley.3mf` | 2026-02-06 | name tidied |
| `V1/IronSpider files/Version 3/Base Casing.3mf` | `cad/v1/iteration_3/Base_Casing.3mf` | 2026-01-24 | name tidied |
| `V1/IronSpider files/Version 3/Big Shoulder driving gear.3mf` | `cad/v1/iteration_3/Big_Shoulder_driving_gear.3mf` | 2026-01-28 | name tidied |
| `V1/IronSpider files/Version 3/Component319.3mf` | `cad/v1/iteration_3/Servo_housing_bracket.3mf` | 2026-02-06 | renamed from Component319 (identified from its plate preview) |
| `V1/IronSpider files/Version 3/Small Shoulder gear.3mf` | `cad/v1/iteration_3/Small_Shoulder_gear.3mf` | 2026-01-28 | name tidied |
| `V1/IronSpider files/Version 3/Tendon mount fix.3mf` | `cad/v1/iteration_3/Tendon_mount_fix.3mf` | 2026-01-27 | name tidied |
| `V1/IronSpider files/Version 3/Top cover.3mf` | `cad/v1/iteration_3/Top_cover.3mf` | 2026-02-01 | name tidied |
| `V1/IronSpider files/Version 3/Top lock 1.3mf` | `cad/v1/iteration_3/Top_lock_1.3mf` | 2026-02-01 | name tidied |
| `V1/IronSpider files/Version 3/Turntable.3mf` | `cad/v1/iteration_3/Turntable.3mf` | 2026-01-20 |  |
| `V1/IronSpider files/Version 3/Linkage/Forearm/Forearm Bottom.3mf` | `cad/v1/iteration_3/linkage/forearm/Forearm_Bottom.3mf` | 2026-01-22 | name tidied |
| `V1/IronSpider files/Version 3/Linkage/Forearm/Forearm Top .3mf` | `cad/v1/iteration_3/linkage/forearm/Forearm_Top.3mf` | 2026-01-22 | name tidied |
| `V1/IronSpider files/Version 3/Linkage/Forearm/Forearm Top Cover (1).3mf` | `cad/v1/iteration_3/linkage/forearm/Forearm_Top_Cover_v2.3mf` | 2026-01-22 | two different files shared this name; assigned v2 |
| `V1/IronSpider files/Version 3/Linkage/Forearm/Forearm Top Cover.3mf` | `cad/v1/iteration_3/linkage/forearm/Forearm_Top_Cover_v1.3mf` | 2026-01-22 | two different files shared this name; assigned v1 |
| `V1/IronSpider files/Version 3/Linkage/Middle/Middle Bicep Bottom.3mf` | `cad/v1/iteration_3/linkage/middle/Middle_Bicep_Bottom.3mf` | 2026-01-22 | name tidied |
| `V1/IronSpider files/Version 3/Linkage/Middle/Middle Bicep Cover 1.3mf` | `cad/v1/iteration_3/linkage/middle/Middle_Bicep_Cover_1.3mf` | 2026-01-20 | name tidied |
| `V1/IronSpider files/Version 3/Linkage/Middle/Middle Bicep Cover 2.3mf` | `cad/v1/iteration_3/linkage/middle/Middle_Bicep_Cover_2.3mf` | 2026-01-20 | name tidied |
| `V1/IronSpider files/Version 3/Linkage/Middle/Middle Bicep Top.3mf` | `cad/v1/iteration_3/linkage/middle/Middle_Bicep_Top.3mf` | 2026-01-22 | name tidied |
| `V1/IronSpider files/Version 3/Linkage/Middle/Second linkage pulley.3mf` | `cad/v1/iteration_3/linkage/middle/Second_linkage_pulley.3mf` | 2026-01-20 | name tidied |
| `V1/IronSpider files/Version 3/Linkage/Middle/Servo Pulley.3mf` | `cad/v1/iteration_3/linkage/middle/Servo_Pulley.3mf` | 2026-01-20 | name tidied |
| `V1/IronSpider files/Version 3/Linkage/Bottom/Bicep Bottom upper half.3mf` | `cad/v1/iteration_3/linkage/bottom/Bicep_Bottom_upper_half.3mf` | 2026-01-21 | name tidied |
| `V1/IronSpider files/Version 3/Linkage/Bottom/Bicep Bottom.3mf` | `cad/v1/iteration_3/linkage/bottom/Bicep_Bottom.3mf` | 2026-01-21 | name tidied |
| `V1/IronSpider files/Version 3/Linkage/Bottom/Bottom Cover 1.3mf` | `cad/v1/iteration_3/linkage/bottom/Bottom_Cover_1.3mf` | 2026-01-20 | name tidied |
| `V1/IronSpider files/Version 3/Linkage/Bottom/Bottom Cover 2.3mf` | `cad/v1/iteration_3/linkage/bottom/Bottom_Cover_2.3mf` | 2026-01-20 | name tidied |
| `V1/IronSpider files/Version 3/Linkage/Bottom/Pizza Slice Tensioner.3mf` | `cad/v1/iteration_3/linkage/bottom/Pizza_Slice_Tensioner.3mf` | 2026-01-20 | name tidied |
| `V1/IronSpider files/Version 3/Linkage/Bottom/Second linkage pulley.3mf` | `cad/v1/iteration_3/linkage/bottom/Second_linkage_pulley.3mf` | 2026-01-20 | name tidied |
| `V1/IronSpider files/Version 3/Linkage/Bottom/Second linkage servo pulley.3mf` | `cad/v1/iteration_3/linkage/bottom/Second_linkage_servo_pulley.3mf` | 2026-01-20 | name tidied |
| `V1/IronSpider files/Version 3/Electronics/Arduino cluster.3mf` | `cad/v1/iteration_3/electronics/Arduino_cluster.3mf` | 2026-01-25 | name tidied |
| `V1/IronSpider files/Version 3/Electronics/Body18.3mf` | `cad/v1/iteration_3/electronics/Electronics_mount_plate_A.3mf` | 2026-01-25 | renamed from Body18 (62x77 mm plate with standoffs) |
| `V1/IronSpider files/Version 3/Electronics/Body44.3mf` | `cad/v1/iteration_3/electronics/Electronics_mount_plate_B.3mf` | 2026-01-25 | renamed from Body44 (64x77 mm plate with standoffs) |
| `V1/IronSpider files/Version 3/Electronics/Convertor Cluster.3mf` | `cad/v1/iteration_3/electronics/Convertor_Cluster.3mf` | 2026-01-25 | name tidied |
| `V1/IronSpider files/Version 3/Electronics/PSU Bracket v1.3mf` | `cad/v1/iteration_3/electronics/PSU_Bracket_v1.3mf` | 2026-01-25 | name tidied |
| `V1/IronSpider files/Version 3/Electronics/Stepper Motor driver box.3mf` | `cad/v1/iteration_3/electronics/Stepper_Motor_driver_box.3mf` | 2026-01-25 | name tidied |
| `V1/IronSpider files/GIC AND EPQ.3mf` | `cad/v1/print_plates/V1_base_casing_print_plate.3mf` | 2026-01-10 | renamed from 'GIC AND EPQ' (base casing, bevel gear and pins on one plate) |
| `Misc/Project Arachnae.3mf` | `cad/v1/print_plates/V1_base_casing_early.3mf` | 2025-11-23 | renamed from Misc/Project Arachnae.3mf (2025-11-23, first base casing) |
| `Misc/Elbow timing belt.stl` | `cad/v1/iteration_3/Elbow_timing_belt.stl` | 2026-01-30 |  |
| `Misc/Pulley Generator.stl` | `cad/v1/pulley_experiments/Pulley_generator_output_1.stl` | 2026-03-02 | renamed: unnamed generator outputs, numbered in file order |
| `Misc/Pulley Generator(1).stl` | `cad/v1/pulley_experiments/Pulley_generator_output_2.stl` | 2026-03-02 | renamed: unnamed generator outputs, numbered in file order |
| `Misc/Pulley Generator(2).stl` | `cad/v1/pulley_experiments/Pulley_generator_output_3.stl` | 2026-03-02 | renamed: unnamed generator outputs, numbered in file order |
| `Misc/Pulley Generator(3).stl` | `cad/v1/pulley_experiments/Pulley_generator_output_4.stl` | 2026-03-02 | renamed: unnamed generator outputs, numbered in file order |
| `Misc/Pulley Generator(4).stl` | `cad/v1/pulley_experiments/Pulley_generator_output_5.stl` | 2026-03-02 | renamed: unnamed generator outputs, numbered in file order |
| `Misc/Main.step` | `cad/v2/assemblies/Full_arm_v1.step` | 2026-05-07 | was Misc/Main.step (2026-05-07) |
| `CAD/Main.stp` | `cad/v2/assemblies/Full_arm_v2.stp` | 2026-10-01 | was CAD/Main.stp (2026-10-01): adds Shoulder GD v3, shoulder casing cover, 34T/38T pulleys |
| `Project arachnae/Shoulder.step` | `cad/v2/assemblies/Shoulder_v1.step` | 2026-05-16 | was Project arachnae/Shoulder.step (2026-05-16) |
| `Misc/Shoulder.step` | `cad/v2/assemblies/Shoulder_v2.step` | 2026-05-20 | was Misc/Shoulder.step (2026-05-20): adds the shoulder turntable |
| `CAD/Shoulder.step` | `cad/v2/assemblies/Shoulder_v3.step` | 2026-09-30 | was CAD/Shoulder.step (2026-09-30): elbow 38T and forearm 34T pulleys |
| `Project arachnae/Elbow/Elbow.step` | `cad/v2/assemblies/Elbow_v1.step` | 2026-05-17 | was Project arachnae/Elbow/Elbow.step (2026-05-17) |
| `CAD/Elbow.step` | `cad/v2/assemblies/Elbow_v2.step` | 2026-09-30 | was CAD/Elbow.step (2026-09-30): mirrored 25 mm bosses removed |
| `Misc/Cycloidal GB.step` | `cad/v2/assemblies/Base_cycloidal_gearbox_v1.step` | 2026-05-14 | was Misc/Cycloidal GB.step (2026-05-14) |
| `CAD/Base.step` | `cad/v2/assemblies/Base_cycloidal_gearbox_v2.step` | 2026-09-30 | was CAD/Base.step (2026-09-30): adds the servo mounting box |
| `Project arachnae/Forearm and gripper/Gripper/Hand Module.step` | `cad/v2/assemblies/Hand_module_ESP32CAM.step` | 2026-06-03 | was Hand Module.step (2026-06-03): hand with the original ESP32-CAM |
| `CAD/Gripper_.step` | `cad/v2/assemblies/Gripper_and_forearm_v2.step` | 2026-09-30 | was CAD/Gripper_.step (2026-09-30) |
| `Project arachnae/Body101.3mf` | `cad/v2/printed_parts/misc/Spacer_12mm_round.3mf` | 2026-05-21 | renamed from Body101 (8x12x12 mm rounded spacer; purpose to confirm) |
| `Project arachnae/Body22.3mf` | `cad/v2/printed_parts/upper_arm/Upper_arm_side_shell_200mm.3mf` | 2026-05-23 | renamed from Body22 (13x59x200 mm shell, used in Elbow.step) |
| `Project arachnae/Body31.3mf` | `cad/v2/printed_parts/upper_arm/Upper_arm_side_shell_169mm.3mf` | 2026-05-23 | renamed from Body31 (13x59x169 mm shell) |
| `Project arachnae/Cydlpidal base.3mf` | `cad/v2/printed_parts/base/Base_cycloidal_gearbox_print_v1.3mf` | 2026-05-15 | renamed from 'Cydlpidal base' (2026-05-15); v2 (2026-08-01, 227 MB) kept offline |
| `Project arachnae/Shoulder DRIVING HTD3M 40T Pulley.step` | `cad/v2/pulleys/Shoulder_driving_pulley_HTD3M_40T.step` | 2026-05-16 |  |
| `Project arachnae/Shoulder Servo HTD3M 16T Pulley.step` | `cad/v2/pulleys/Shoulder_servo_pulley_HTD3M_16T.step` | 2026-05-29 |  |
| `Project arachnae/V2 Elbow 40T TO 38T .step` | `cad/v2/pulleys/Elbow_pulley_HTD3M_38T.step` | 2026-08-01 | renamed: elbow pulley cut from 40T to 38T |
| `Project arachnae/V2 Forearm driver 34t from 36t.step` | `cad/v2/pulleys/Forearm_driver_pulley_HTD3M_34T.step` | 2026-07-31 | renamed: forearm driver cut from 36T to 34T |
| `Project arachnae/Elbow/Body31.3mf` | `cad/v2/printed_parts/elbow/Elbow_link_cover_119mm.3mf` | 2026-05-25 | renamed from Body31 (8x59x119 mm cover) |
| `Project arachnae/Elbow/Body33.3mf` | `cad/v2/printed_parts/elbow/Elbow_link_side_shell_125mm.3mf` | 2026-05-25 | renamed from Body33 (13x59x125 mm shell) |
| `Project arachnae/Elbow/Body34.3mf` | `cad/v2/printed_parts/elbow/Elbow_link_cover_125mm.3mf` | 2026-05-25 | renamed from Body34 (8x59x125 mm cover) |
| `Project arachnae/Base/Beariing Cage.3mf` | `cad/v2/printed_parts/base/Base_bearing_cage.3mf` | 2026-05-22 | typo fixed |
| `Project arachnae/Base/Cover.3mf` | `cad/v2/printed_parts/base/Base_cover.3mf` | 2026-05-22 |  |
| `Project arachnae/Base/Output.step` | `cad/v2/printed_parts/base/Base_output.step` | 2026-06-07 |  |
| `Project arachnae/Base/Servo Casing for mount.3mf` | `cad/v2/printed_parts/base/Base_servo_casing_v1.3mf` | 2026-05-24 | renamed; replaced by cad/v2/base_drive/servo_holder_v2 |
| `Project arachnae/Base/TPU Gasket.3mf` | `cad/v2/printed_parts/base/Base_TPU_gasket.3mf` | 2026-05-23 |  |
| `Project arachnae/Misc/Axle bracings/Axle brace 2 way.3mf` | `cad/v2/printed_parts/misc/Axle_brace_2_way.3mf` | 2026-06-07 |  |
| `Project arachnae/Misc/Axle bracings/Axle brace 3 way.3mf` | `cad/v2/printed_parts/misc/Axle_brace_3_way.3mf` | 2026-06-07 |  |
| `Project arachnae/Misc/Tensioner bushings/9mm dia.3mf` | `cad/v2/printed_parts/misc/Tensioner_bushing_9mm_short.3mf` | 2026-06-06 | renamed from '9mm dia' |
| `Project arachnae/Misc/Tensioner bushings/Long Bushing.3mf` | `cad/v2/printed_parts/misc/Tensioner_bushing_9mm_long.3mf` | 2026-06-06 |  |
| `Project arachnae/Battery/Bracket.3mf` | `cad/v2/printed_parts/battery/Battery_bracket.3mf` | 2026-05-25 |  |
| `Project arachnae/Battery/Top - Pattern v1.3mf` | `cad/v2/printed_parts/battery/Battery_box_lid_vented.3mf` | 2026-05-26 | renamed from 'Top - Pattern v1' |
| `Project arachnae/Forearm and gripper/Axle cover.3mf` | `cad/v2/printed_parts/forearm/Axle_end_cap_v1.3mf` | 2026-06-13 | same part as Body36; earlier copy (2026-06-13) |
| `Project arachnae/Forearm and gripper/Body36.3mf` | `cad/v2/printed_parts/forearm/Axle_end_cap_v2.3mf` | 2026-06-15 | renamed from Body36; later copy (2026-06-15) of the axle end cap |
| `Project arachnae/Forearm and gripper/Clamp.3mf` | `cad/v2/printed_parts/forearm/Forearm_axle_clamp.3mf` | 2026-06-15 |  |
| `Project arachnae/Forearm and gripper/First Half.3mf` | `cad/v2/printed_parts/forearm/Forearm_first_half.3mf` | 2026-05-27 |  |
| `Project arachnae/Forearm and gripper/First half cover.3mf` | `cad/v2/printed_parts/forearm/Forearm_first_half_cover.3mf` | 2026-05-27 |  |
| `Project arachnae/Forearm and gripper/Top half cover.3mf` | `cad/v2/printed_parts/forearm/Forearm_top_half_cover.3mf` | 2026-06-01 |  |
| `Project arachnae/Forearm and gripper/Top half.3mf` | `cad/v2/printed_parts/forearm/Forearm_top_half.3mf` | 2026-06-01 |  |
| `Project arachnae/Forearm and gripper/forearm axle cover.3mf` | `cad/v2/printed_parts/forearm/Forearm_axle_brace_3_way.3mf` | 2026-06-13 | renamed: it is a 3-way axle brace, not a cover |
| `Project arachnae/Forearm and gripper/Gripper/Component21.step` | `cad/v2/printed_parts/gripper/Camera_housing_lid_v2.step` | 2026-06-05 | renamed from Component21 (2026-06-05): adds a cut-out |
| `Project arachnae/Forearm and gripper/Gripper/Component22.step` | `cad/v2/printed_parts/gripper/Camera_housing_v2.step` | 2026-06-12 | renamed from Component22 (2026-06-12): slot pattern changed |
| `Project arachnae/Forearm and gripper/Gripper/Finger.3mf` | `cad/v2/printed_parts/gripper/Gripper_finger_v1.3mf` | 2026-06-04 |  |
| `Project arachnae/Forearm and gripper/Gripper/Gear 1.step` | `cad/v2/printed_parts/gripper/Gripper_jaw_gear_1.step` | 2026-06-05 | renamed: sector gear with jaw arm |
| `Project arachnae/Forearm and gripper/Gripper/Gear 2.step` | `cad/v2/printed_parts/gripper/Gripper_jaw_gear_2.step` | 2026-06-05 |  |
| `Project arachnae/Forearm and gripper/Gripper/Gripper servo box.3mf` | `cad/v2/printed_parts/gripper/Gripper_servo_box.3mf` | 2026-06-05 |  |
| `Project arachnae/Forearm and gripper/Gripper/Thingy.step` | `cad/v2/printed_parts/gripper/Gripper_servo_bracket.step` | 2026-06-05 | renamed from 'Thingy' |
| `Project arachnae/Forearm and gripper/Gripper/finger 2.3mf` | `cad/v2/printed_parts/gripper/Gripper_finger_v2.3mf` | 2026-06-04 | renamed from 'finger 2' |
| `Project arachnae/Forearm and gripper/Test/Body15.3mf` | `cad/v2/printed_parts/gripper/Test_ring_mount_plate.3mf` | 2026-05-28 | renamed from Body15 (43x48 mm plate with a clip ring; test part) |
| `Project arachnae/Forearm and gripper/Test/Component21.step` | `cad/v2/printed_parts/gripper/Camera_housing_lid_v1.step` | 2026-05-28 | renamed from Component21 (test print, 2026-05-28) |
| `Project arachnae/Forearm and gripper/Test/Component22.step` | `cad/v2/printed_parts/gripper/Camera_housing_v1.step` | 2026-05-28 | renamed from Component22 (test print, 2026-05-28) |
| `Misc/47T HTD3M.3mf` | `cad/v2/pulleys/HTD3M_47T_pulley.3mf` | 2026-05-20 |  |
| `Misc/49T HTD3M .3mf` | `cad/v2/pulleys/HTD3M_49T_pulley.3mf` | 2026-05-20 |  |
| `Misc/50T HTD3M.3mf` | `cad/v2/pulleys/HTD3M_50T_pulley.3mf` | 2026-05-20 |  |
| `Misc/57T HTD3M.3mf` | `cad/v2/pulleys/HTD3M_57T_pulley.3mf` | 2026-05-21 |  |
| `Misc/59T HTD3M.3mf` | `cad/v2/pulleys/HTD3M_59T_pulley.3mf` | 2026-05-20 |  |
| `Misc/Belt Generator.3mf` | `cad/v2/pulleys/HTD3M_belt_v1.3mf` | 2026-05-31 | renamed from 'Belt Generator' |
| `Misc/Belt Generator(2).3mf` | `cad/v2/pulleys/HTD3M_belt_v2.3mf` | 2026-06-02 | renamed from 'Belt Generator(2)' |
| `Misc/Elbow v21kmjknkn.3mf` | `cad/v2/pulleys/Elbow_HTD3M_pulley_v2.1.3mf` | 2026-06-02 | renamed from 'Elbow v21kmjknkn' |
| `CAD/servo_holder_v2/Servo_holder_v2.step` | `cad/v2/base_drive/servo_holder_v2/Servo_holder_v2.step` | 2026-10-02 |  |
| `CAD/servo_holder_v2/Servo_backplate_v2.step` | `cad/v2/base_drive/servo_holder_v2/Servo_backplate_v2.step` | 2026-10-02 |  |
| `CAD/servo_holder_v2/Servo_holder_v2_print.stl` | `cad/v2/base_drive/servo_holder_v2/Servo_holder_v2_print.stl` | 2026-10-02 |  |
| `CAD/servo_holder_v2/Servo_backplate_v2_print.stl` | `cad/v2/base_drive/servo_holder_v2/Servo_backplate_v2_print.stl` | 2026-10-02 |  |
| `CAD/servo_holder_v2/README.md` | `cad/v2/base_drive/servo_holder_v2/README.md` | 2026-10-02 |  |
| `CAD/servo_holder_v2/Servo_holder_v2_solid.step` | `cad/v2/base_drive/servo_holder_v2/Servo_holder_v2_solid.step` | 2026-10-02 |  |
| `CAD/servo_holder_v2/Servo_holder_v2_solid_print.stl` | `cad/v2/base_drive/servo_holder_v2/Servo_holder_v2_solid_print.stl` | 2026-10-02 |  |
| `CAD/servo_holder_v2/renders/1_fitted_top.png` | `cad/v2/base_drive/servo_holder_v2/renders/1_fitted_top.png` | 2026-10-02 |  |
| `CAD/servo_holder_v2/renders/2_fitted_below.png` | `cad/v2/base_drive/servo_holder_v2/renders/2_fitted_below.png` | 2026-10-02 |  |
| `CAD/servo_holder_v2/renders/3_holder_back_and_pockets.png` | `cad/v2/base_drive/servo_holder_v2/renders/3_holder_back_and_pockets.png` | 2026-10-02 |  |
| `CAD/servo_holder_v2/renders/4_holder_underside_pockets.png` | `cad/v2/base_drive/servo_holder_v2/renders/4_holder_underside_pockets.png` | 2026-10-02 |  |
| `CAD/servo_holder_v2/renders/5_topology_optimisation_slices.png` | `cad/v2/base_drive/servo_holder_v2/renders/5_topology_optimisation_slices.png` | 2026-10-02 |  |
| `CAD/servo_holder_v2/renders/solid_version/1_top_servo_side.png` | `cad/v2/base_drive/servo_holder_v2/renders/solid_version/1_top_servo_side.png` | 2026-10-02 |  |
| `CAD/servo_holder_v2/renders/solid_version/2_from_below.png` | `cad/v2/base_drive/servo_holder_v2/renders/solid_version/2_from_below.png` | 2026-10-02 |  |
| `CAD/servo_holder_v2/renders/solid_version/3_below_back_plate.png` | `cad/v2/base_drive/servo_holder_v2/renders/solid_version/3_below_back_plate.png` | 2026-10-02 |  |
| `CAD/base_worm_4start/Worm_4start.step` | `cad/v2/base_drive/1_worm_4start/Worm_4start.step` | 2026-09-30 |  |
| `CAD/base_worm_4start/Worm_4start.stl` | `cad/v2/base_drive/1_worm_4start/Worm_4start.stl` | 2026-09-30 |  |
| `CAD/base_worm_4start/Gear_20T_4start.step` | `cad/v2/base_drive/1_worm_4start/Gear_20T_4start.step` | 2026-09-30 |  |
| `CAD/base_worm_4start/Gear_20T_4start.stl` | `cad/v2/base_drive/1_worm_4start/Gear_20T_4start.stl` | 2026-09-30 |  |
| `CAD/base_worm_4start/README.md` | `cad/v2/base_drive/1_worm_4start/README.md` | 2026-10-01 |  |
| `CAD/base_worm_4start/Gear_20T_4start_stub.step` | `cad/v2/base_drive/1_worm_4start/Gear_20T_4start_stub.step` | 2026-10-01 |  |
| `CAD/base_worm_4start/Gear_support_bracket.step` | `cad/v2/base_drive/1_worm_4start/Gear_support_bracket.step` | 2026-10-01 |  |
| `CAD/base_worm_4start/Gear_20T_4start_stub.stl` | `cad/v2/base_drive/1_worm_4start/Gear_20T_4start_stub.stl` | 2026-10-01 |  |
| `CAD/base_worm_4start/Gear_support_bracket.stl` | `cad/v2/base_drive/1_worm_4start/Gear_support_bracket.stl` | 2026-10-01 |  |
| `CAD/base_worm_2start/Gear_10T_2start_stub.step` | `cad/v2/base_drive/2_worm_2start_and_face_gear/Gear_10T_2start_stub.step` | 2026-10-02 |  |
| `CAD/base_worm_2start/Gear_10T_2start_stub.stl` | `cad/v2/base_drive/2_worm_2start_and_face_gear/Gear_10T_2start_stub.stl` | 2026-10-02 |  |
| `CAD/base_worm_2start/Worm_2start.step` | `cad/v2/base_drive/2_worm_2start_and_face_gear/Worm_2start.step` | 2026-10-02 |  |
| `CAD/base_worm_2start/Worm_2start.stl` | `cad/v2/base_drive/2_worm_2start_and_face_gear/Worm_2start.stl` | 2026-10-02 |  |
| `CAD/base_worm_2start/Worm_2start_v2.step` | `cad/v2/base_drive/2_worm_2start_and_face_gear/Worm_2start_v2.step` | 2026-10-02 |  |
| `CAD/base_worm_2start/Worm_2start_v2_print.stl` | `cad/v2/base_drive/2_worm_2start_and_face_gear/Worm_2start_v2_print.stl` | 2026-10-02 |  |
| `CAD/base_worm_2start/Gear_10T_2start_throated.stl` | `cad/v2/base_drive/2_worm_2start_and_face_gear/Gear_10T_2start_throated.stl` | 2026-10-03 |  |
| `CAD/base_worm_2start/README.md` | `cad/v2/base_drive/2_worm_2start_and_face_gear/README.md` | 2026-10-05 |  |
| `CAD/base_worm_2start/Gear_10T_2start_throated_bore12.5.stl` | `cad/v2/base_drive/2_worm_2start_and_face_gear/Gear_10T_2start_throated_bore12.5.stl` | 2026-10-04 |  |
| `CAD/base_worm_2start/Face_gear_20T_2start.stl` | `cad/v2/base_drive/2_worm_2start_and_face_gear/Face_gear_20T_2start.stl` | 2026-10-04 |  |
| `CAD/base_worm_2start/Face_gear_20T_2start_print.stl` | `cad/v2/base_drive/2_worm_2start_and_face_gear/Face_gear_20T_2start_print.stl` | 2026-10-04 |  |
| `CAD/base_worm_2start/Face_gear_20T_2start_v2.stl` | `cad/v2/base_drive/2_worm_2start_and_face_gear/Face_gear_20T_2start_v2.stl` | 2026-10-05 |  |
| `CAD/base_worm_2start/Face_gear_20T_2start_v2_print.stl` | `cad/v2/base_drive/2_worm_2start_and_face_gear/Face_gear_20T_2start_v2_print.stl` | 2026-10-05 |  |
| `CAD/base_worm_2start/throated_gen/fc_export.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/throated_gen/fc_export.py` | 2026-10-03 |  |
| `CAD/base_worm_2start/throated_gen/common2d.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/throated_gen/common2d.py` | 2026-10-03 |  |
| `CAD/base_worm_2start/throated_gen/hob2d.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/throated_gen/hob2d.py` | 2026-10-03 |  |
| `CAD/base_worm_2start/throated_gen/verify2d.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/throated_gen/verify2d.py` | 2026-10-03 |  |
| `CAD/base_worm_2start/throated_gen/stl2d.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/throated_gen/stl2d.py` | 2026-10-03 |  |
| `CAD/base_worm_2start/throated_gen/patches.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/throated_gen/patches.py` | 2026-10-03 |  |
| `CAD/base_worm_2start/throated_gen/mesh_worm.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/throated_gen/mesh_worm.py` | 2026-10-03 |  |
| `CAD/base_worm_2start/throated_gen/fea_common.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/throated_gen/fea_common.py` | 2026-10-03 |  |
| `CAD/base_worm_2start/throated_gen/worm_fea.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/throated_gen/worm_fea.py` | 2026-10-03 |  |
| `CAD/base_worm_2start/throated_gen/gear_fea.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/throated_gen/gear_fea.py` | 2026-10-03 |  |
| `CAD/base_worm_2start/thrust_washers/Thrust_washer_0.5mm.stl` | `cad/v2/base_drive/2_worm_2start_and_face_gear/thrust_washers/Thrust_washer_0.5mm.stl` | 2026-10-03 |  |
| `CAD/base_worm_2start/thrust_washers/Thrust_washer_1.0mm.stl` | `cad/v2/base_drive/2_worm_2start_and_face_gear/thrust_washers/Thrust_washer_1.0mm.stl` | 2026-10-03 |  |
| `CAD/base_worm_2start/thrust_washers/Thrust_washer_1.6mm.stl` | `cad/v2/base_drive/2_worm_2start_and_face_gear/thrust_washers/Thrust_washer_1.6mm.stl` | 2026-10-03 |  |
| `CAD/base_worm_2start/thrust_washers/Thrust_washer_1.7mm.stl` | `cad/v2/base_drive/2_worm_2start_and_face_gear/thrust_washers/Thrust_washer_1.7mm.stl` | 2026-10-03 |  |
| `CAD/base_worm_2start/thrust_washers/Thrust_washer_1.8mm.stl` | `cad/v2/base_drive/2_worm_2start_and_face_gear/thrust_washers/Thrust_washer_1.8mm.stl` | 2026-10-03 |  |
| `CAD/base_worm_2start/thrust_washers/Thrust_washer_1.9mm.stl` | `cad/v2/base_drive/2_worm_2start_and_face_gear/thrust_washers/Thrust_washer_1.9mm.stl` | 2026-10-03 |  |
| `CAD/base_worm_2start/thrust_washers/Thrust_washer_2.0mm.stl` | `cad/v2/base_drive/2_worm_2start_and_face_gear/thrust_washers/Thrust_washer_2.0mm.stl` | 2026-10-03 |  |
| `CAD/base_worm_2start/gear_bracket_4arm/Gear_support_bracket_4arm.step` | `cad/v2/base_drive/2_worm_2start_and_face_gear/gear_bracket_4arm/Gear_support_bracket_4arm.step` | 2026-10-04 |  |
| `CAD/base_worm_2start/gear_bracket_4arm/Gear_support_bracket_4arm_print.stl` | `cad/v2/base_drive/2_worm_2start_and_face_gear/gear_bracket_4arm/Gear_support_bracket_4arm_print.stl` | 2026-10-04 |  |
| `CAD/base_worm_2start/gear_bracket_4arm/top_view.png` | `cad/v2/base_drive/2_worm_2start_and_face_gear/gear_bracket_4arm/top_view.png` | 2026-10-04 |  |
| `CAD/base_worm_2start/gear_bracket_4arm/bracket_fea.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/gear_bracket_4arm/bracket_fea.py` | 2026-10-04 |  |
| `CAD/base_worm_2start/gear_bracket_4arm/bracket_fea_slice.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/gear_bracket_4arm/bracket_fea_slice.py` | 2026-10-04 |  |
| `CAD/base_worm_2start/gear_bracket_4arm/README.md` | `cad/v2/base_drive/2_worm_2start_and_face_gear/gear_bracket_4arm/README.md` | 2026-10-04 |  |
| `CAD/base_worm_2start/face_gen/fc_export_face.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/face_gen/fc_export_face.py` | 2026-10-04 |  |
| `CAD/base_worm_2start/face_gen/common_face.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/face_gen/common_face.py` | 2026-10-04 |  |
| `CAD/base_worm_2start/face_gen/plot_face.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/face_gen/plot_face.py` | 2026-10-04 |  |
| `CAD/base_worm_2start/face_gen/verify_face.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/face_gen/verify_face.py` | 2026-10-04 |  |
| `CAD/base_worm_2start/face_gen/fea_face.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/face_gen/fea_face.py` | 2026-10-04 |  |
| `CAD/base_worm_2start/face_gen/hob_face.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/face_gen/hob_face.py` | 2026-10-05 |  |
| `CAD/base_worm_2start/face_gen/stl_face.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/face_gen/stl_face.py` | 2026-10-04 |  |
| `CAD/base_worm_2start/face_gen/params.py` | `cad/v2/base_drive/2_worm_2start_and_face_gear/face_gen/params.py` | 2026-10-05 |  |
| `CAD/base_worm_2start/face_gen/slices_20T.png` | `cad/v2/base_drive/2_worm_2start_and_face_gear/face_gen/slices_20T.png` | 2026-10-04 |  |
| `CAD/base_worm_2start/face_gen/README.md` | `cad/v2/base_drive/2_worm_2start_and_face_gear/face_gen/README.md` | 2026-10-05 |  |
| `CAD/base_cycloid_gearbox/make_double_cam.py` | `cad/v2/base_drive/cycloid_gearbox/make_double_cam.py` | 2026-10-04 |  |
| `CAD/base_cycloid_gearbox/Double_cam.step` | `cad/v2/base_drive/cycloid_gearbox/Double_cam.step` | 2026-10-04 |  |
| `CAD/base_cycloid_gearbox/Double_cam_print.stl` | `cad/v2/base_drive/cycloid_gearbox/Double_cam_print.stl` | 2026-10-04 |  |
| `CAD/base_cycloid_gearbox/README.md` | `cad/v2/base_drive/cycloid_gearbox/README.md` | 2026-10-04 |  |
| `CAD/base_cycloid_gearbox/export_cycloids.py` | `cad/v2/base_drive/cycloid_gearbox/export_cycloids.py` | 2026-10-04 |  |
| `CAD/base_cycloid_gearbox/Cycloid_1.step` | `cad/v2/base_drive/cycloid_gearbox/Cycloid_1.step` | 2026-10-04 |  |
| `CAD/base_cycloid_gearbox/Cycloid_1_print.stl` | `cad/v2/base_drive/cycloid_gearbox/Cycloid_1_print.stl` | 2026-10-04 |  |
| `CAD/base_cycloid_gearbox/Cycloid_2.step` | `cad/v2/base_drive/cycloid_gearbox/Cycloid_2.step` | 2026-10-04 |  |
| `CAD/base_cycloid_gearbox/Cycloid_2_print.stl` | `cad/v2/base_drive/cycloid_gearbox/Cycloid_2_print.stl` | 2026-10-04 |  |
| `CAD/base_cycloid_gearbox/renders/cycloid_exploded.png` | `cad/v2/base_drive/cycloid_gearbox/renders/cycloid_exploded.png` | 2026-10-04 |  |
| `CAD/base_cycloid_gearbox/renders/cycloid_section.png` | `cad/v2/base_drive/cycloid_gearbox/renders/cycloid_section.png` | 2026-10-04 |  |
| `CAD/pencil_module/pencil_gen.py` | `cad/v2/tool_modules/pencil_module/pencil_gen.py` | 2026-10-06 |  |
| `CAD/pencil_module/tool_receiver.step` | `cad/v2/tool_modules/pencil_module/tool_receiver.step` | 2026-10-06 |  |
| `CAD/pencil_module/tool_receiver.stl` | `cad/v2/tool_modules/pencil_module/tool_receiver.stl` | 2026-10-06 |  |
| `CAD/pencil_module/pencil_module.step` | `cad/v2/tool_modules/pencil_module/pencil_module.step` | 2026-10-06 |  |
| `CAD/pencil_module/pencil_module.stl` | `cad/v2/tool_modules/pencil_module/pencil_module.stl` | 2026-10-06 |  |
| `CAD/pencil_module/pencil_collet_ring.step` | `cad/v2/tool_modules/pencil_module/pencil_collet_ring.step` | 2026-10-06 |  |
| `CAD/pencil_module/pencil_collet_ring.stl` | `cad/v2/tool_modules/pencil_module/pencil_collet_ring.stl` | 2026-10-06 |  |
| `CAD/pencil_module/gripper_spigot.step` | `cad/v2/tool_modules/pencil_module/gripper_spigot.step` | 2026-10-06 |  |
| `CAD/pencil_module/gripper_spigot.stl` | `cad/v2/tool_modules/pencil_module/gripper_spigot.stl` | 2026-10-06 |  |
| `CAD/pencil_module/pencil_paint.stl` | `cad/v2/tool_modules/pencil_module/pencil_paint.stl` | 2026-10-06 |  |
| `CAD/pencil_module/pencil_wood.stl` | `cad/v2/tool_modules/pencil_module/pencil_wood.stl` | 2026-10-06 |  |
| `CAD/pencil_module/pencil_graphite.stl` | `cad/v2/tool_modules/pencil_module/pencil_graphite.stl` | 2026-10-06 |  |
| `CAD/pencil_module/README.md` | `cad/v2/tool_modules/pencil_module/README.md` | 2026-10-06 |  |
| `CAD/pencil_module/__pycache__/pencil_gen.cpython-314.pyc` | `cad/v2/tool_modules/pencil_module/pencil_gen.cpython-314.pyc` | 2026-10-06 |  |

## Not in the repo

| File | Why |
|---|---|
| `Misc/Cydlpidal base.3mf` (2026-08-01, 227 MB) | v2 of the base gearbox print file. It is a mesh export far too large for what it adds; the STEP of the same gearbox is in `cad/v2/assemblies/`. |
| Seeed XIAO ESP32-S3 Sense model | third-party CAD (Seeed Studio); get it from Seeed |
| RobotDigg flexible gripper (RC2SR / RC3SR) | third-party CAD I only used for reference |
| `Project arachnae CAD.zip` | an earlier rename had emptied it to 2 bytes; the full models are in this folder |
