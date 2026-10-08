# Project Arachnae

**A low-cost robotic arm you wear on your back, with a camera in its hand.**

I started Project Arachnae as a passion project for a friend of mine who doesn't have an arm. As it grew, it became a
goal: make an assistive arm that people on a tight budget, or in developing countries, could actually build, adapt and
repair, and that does more than the expensive prosthetics that offer little real functionality.

Arachnae is a 1 m, 2 kg, 4-joint arm with a gripper. It mounts on a back plate behind the right shoulder blade and
reaches around the body. The camera in the gripper feeds an object detector, and the arm's own firmware plans moves
that avoid the wearer's body.

![Arachnae worn, September 2026](docs/images/worn_2026-09-30.jpg)

<!-- VIDEO LINKS: replace the # with the YouTube links once uploaded -->
**Videos:** [prototype film](#) · [vision proof](#) · [first grab, 2026-09-29](#)

## Status (October 2026)

| | |
|---|---|
| **Works on the real arm** | jogging all four joints to commanded angles; the gripper; picking up and releasing an object; teaching poses; E-STOP and recovery; Wi-Fi control from a phone; over-the-air updates; live object detection from the hand camera |
| **Works in simulation** | the full autonomous grab (scan → approach → grip → lift → hand off → home), checked by 940 automated tests |
| **Not done yet** | **the base joint**: its printed gearbox still breaks under the full arm's weight, after seven redesigns ([the whole story](docs/BASE_GEARBOX.md)). Without it the arm can't lift itself, so autonomous grabbing hasn't run on the real arm yet. |

## How it works

| | |
|---|---|
| **Mechanics** | Shoulder, elbow and wrist driven by ST3215 servos through HTD3M timing belts (6.7:1, 6:1, 2.25:1). The base, which lifts the whole arm, uses a self-locking worm stage into a 10:1 cycloidal reducer. Everything structural is printed in PETG. The gripper twist-locks on and off. |
| **Electronics** | One Seeed XIAO ESP32-S3 Sense runs everything: five serial bus servos, the camera, Wi-Fi and the control pages. The servos connect to the board through just two resistors per bus. A 3S LiPo powers it, and a physical switch cuts the 12 V rail. |
| **Firmware** | Multi-turn motion on single-turn servos, inverse kinematics with a model of the wearer's body, seven taught poses, a grab state machine, and a safety layer that never trusts a servo write it hasn't read back. |
| **Vision** | YOLOv8s on a laptop reads the hand camera, estimates the object's 3D position (calibrated to within about 3 cm), and sends it to the arm. |
| **Testing** | The exact firmware compiles on a PC against a simulated servo bus, so every bug I find on the arm becomes a test first. |

Read more: [system overview](docs/ARCHITECTURE.md) · [hardware](docs/HARDWARE.md) · [firmware](docs/FIRMWARE.md) ·
[safety](docs/SAFETY.md) · [testing](docs/TESTING.md) · [vision](docs/VISION.md) ·
[base gearbox](docs/BASE_GEARBOX.md) · [operating the arm](docs/OPERATING.md)

## The story

- [**TIMELINE.md**](TIMELINE.md): day by day, from the first base design in November 2025 to now.
- [**Design decisions**](docs/DESIGN_DECISIONS.md): what I chose and why.
- [**Lessons learned**](docs/LESSONS_LEARNED.md): the failures that taught me the most.

## Repository

| Folder | What |
|---|---|
| [`firmware/`](firmware) | the arm's firmware (PlatformIO, C++), the servo ID tool and the simulator |
| [`vision/`](vision) | the laptop vision pipeline (Python, ONNX Runtime) |
| [`cad/`](cad) | all CAD for V1 and V2: STEP assemblies, print files, and the base gearbox iterations with their generator and FEA scripts ([index](cad/README.md), [rename log](cad/RENAMES.md)) |
| [`docs/`](docs) | how everything works, and why |
| [`archive/`](archive) | the V1 Arduino code and the first V2 code |

CAD files and models are stored with Git LFS (about 0.9 GB). To clone just the code:

```bash
GIT_LFS_SKIP_SMUDGE=1 git clone https://github.com/green-hat-001/Project-Arachnae.git
```

The full V1 model can also be viewed in a browser on [Autodesk](https://a360.co/4daP7HM).

## Quick start

```bash
cd firmware
cp Final_Firmware_Main/secrets.h.example Final_Firmware_Main/secrets.h   # Wi-Fi + OTA passwords
tools/host_sim/run_tests.sh      # 940 checks, no hardware needed
pio run -e main_fw -t upload     # flash the XIAO ESP32-S3 over USB-C
```

Then power the arm, join its `ARACHNAE` network and open `http://192.168.4.1/`. Before moving it, read
[SAFETY.md](docs/SAFETY.md) and [OPERATING.md](docs/OPERATING.md).

## History

**V1** (Nov 2025 to Apr 2026): an Arduino Uno R4 driving a NEMA 23 stepper base, DS3218 servos, and a five-finger hand
pulled by MG996R servos and rubber-band tendons. Its code is in [`archive/v1-arduino/`](archive/v1-arduino) and its CAD
in [`cad/v1/`](cad/v1). **V2** (May 2026 onward) moved to serial bus servos with position feedback, a single ESP32 with
a camera, and everything above.

## Contact

For more detail, email me at gal1le02620@gmail.com.
