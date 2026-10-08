# Base Subsystem Changelog

## 2026-09-10
* **Backlash / Deadband Compensation Added**: Implemented a 1-revolution (4096 step) backlash compensation in `base_mode0_multiturn.py`. Testing revealed that due to mechanical play in the 5mm steel rod and helical gear, exactly 1 servo revolution is consumed as slack when switching directions before the cycloidal disks engage.
* **Mechanical Adjustments Noted**: Identified that the pen nib securing the helical gear protrudes by 1mm, wearing the PETG worm gear. Mechanical redesign (taller threads, bevel gears) was audited. Calculated cantilever load (~7.8 N·m) vs max cycloidal torque (~63 N·m); decided against raising the base profile with bevel gears because the existing load rating has an 8x safety margin.
* **Firmware/Mode 3 Audit Finding**: Evaluated Mode 3 (32-bit position) versus Mode 0 (16-bit position). Confirmed ST3215 Mode 3 is fundamentally flawed without explicit flash writes and power cycles. Defaulting to Mode 0 with software tracking for continuous rotation validation.
* **Teach Mode Web UI Bug Fix**: Resolved the bug where servo angles were blank on the smartphone interface. The error was caused by invalid `NaN` stringification reaching the `JSON.parse` engine when Mode 3 32-bit registers were spuriously corrupted. Ported to strict Mode 0 bounding which inherently eradicates outputting `NaN`.
* **Repository Architecture Update**: Fully reorganized the messy `arachnae_package` root directory into proper domains:
  - `Final_Firmware_Main/`: Main ESP32 kinematics and Web UI (renamed `main.ino` -> `Final_Firmware_Main.ino` to satisfy Arduino compiler rules).
  - `Final_Firmware_Camera/`: ESP32-CAM stream scripts (renamed `esp32cam_stream.ino`).
  - `Final_Tools/`: Python orchestration tools like `arachnae_setup.py` and `cv_pipeline.py`.
  - `Old_Iterations/`: Legacy tests and archived patches.
