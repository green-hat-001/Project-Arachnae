#pragma once
// ============================================================
//  PROJET ARACHNAE v2 — Configuration
//  Shoulder-blade mounted, right side, reach-around to front
//
//  Everything marked  [VERIFY]  is a physical fact the firmware
//  cannot know. Wrong values here = arm goes to the wrong place.
//  Ratios and joint signs can be corrected at runtime from the
//  teach UI (Joint calibration card) and are stored in flash, so
//  you do NOT need to reflash to fix them.
// ============================================================

// ── Servo IDs ────────────────────────────────────────────────
#define SERVO_BASE      1
#define SERVO_SHOULDER  2
#define SERVO_ELBOW     3
#define SERVO_FOREARM   4
#define SERVO_GRIPPER   5

// ── ST3215 Hardware ──────────────────────────────────────────
#define SERVO_STEPS_PER_REV  4096
#define SERVO_STEPS_CENTRE   2048

// ── Gear ratios (servo revs per output rev)  [VERIFY] ────────
// These are only first-boot defaults: once anything has been saved in the
// Joint calibration card, the values stored in NVS win.
//   base: 2-start worm -> 10T throated gear (5:1) -> cycloid. The cycloid is
//     10 lobes / 11 pins = 10:1 in CAD/Main.stp (the user believed 20:1), so
//     50 (or 100). Measure it with the /test spin turn counter.
//   shoulder 6.667 (16->40, 15->40), elbow 6.0 (16->40, 15->36): match the CAD.
//   forearm 2.25 (16->16 transfer, 16->36) from the CAD; it was 2.5 here.
#define RATIO_BASE      50.0f
#define RATIO_SHOULDER   6.667f
#define RATIO_ELBOW      6.0f
#define RATIO_FOREARM    2.25f
#define RATIO_GRIPPER    1.0f

// ── Joint direction  [VERIFY] ────────────────────────────────
// +1 = servo step count increasing moves the joint in the +angle
// direction of the IK model. Worn, with the arm hanging:
//   base "+"      swings the arm toward your RIGHT (outward);
//   shoulder "+"  swings link 1 away from your back;
//   elbow / forearm "+" bend the next link the same way as shoulder "+".
// Flip in the UI if a jog "+" moves the wrong way.
#define SIGN_BASE       1
#define SIGN_SHOULDER   1
#define SIGN_ELBOW      1
#define SIGN_FOREARM    1

// ── Backlash (servo steps of slack on direction reversal) ────
// First-boot defaults; runtime-editable in the Joint calibration card
// (NVS). The old 4096 (one servo rev) was the 1-start worm stage. The
// 2-start/throated mesh has ~1.2 deg of play at the gear = ~70 servo steps,
// which is ~0.1 deg at the base, so it starts at 0. [VERIFY] re-measure.
#define BACKLASH_BASE_STEPS         0
#define BACKLASH_SHOULDER_STEPS     0
#define BACKLASH_ELBOW_STEPS        0
#define BACKLASH_FOREARM_STEPS      0

// ── Wheel-mode (Mode 1) direction flag ───────────────────────
// STS-series: bit 15 of GOAL_SPEED = reverse.  Some firmware uses
// bit 10 (0x0400).  If a leapfrog move ever reports "wrong way",
// flip it from the teach UI (or use serial command "dirbit").
#define WHEEL_REVERSE_BIT   0x8000

// ── Link lengths (mm), measured in CAD/Main.stp ──────────────
// L1 shoulder axis -> elbow axis, L2 elbow axis -> wrist axis,
// L3 wrist axis -> grasp centre (the point the CV pipeline's "tip" frame
// is centred on; the camera lens is at ~403, the fingertips at ~503).
#define L1_MM   189.2f
#define L2_MM   400.0f
#define L3_MM   475.0f

// ── Mount: arm frame vs body frame  [VERIFY] ─────────────────
// Two frames, both with the origin where the shoulder axis meets the base
// axis (behind the RIGHT shoulder blade):
//   BODY frame: +X forward, +Y toward your left, +Z up. The body model,
//     "above"/"lift", tool pitch and the taught height envelope live here.
//   ARM frame (all FK/IK math, and "frame":"base" targets): +Z along the
//     base axis, +X where base angle 0 points, +Y = Z x X.
// Worn (photo, 2026-10-03), the base axis sticks straight out of the back,
// roughly horizontal. Base angle 0 is the arm plane hanging PLUMB (gravity
// straight down along the back), so base "+" swings the arm toward your right.
//   MOUNT_AXIS_TILT_DEG: base axis above horizontal (+ = points back AND up;
//     a forward lean tips it down). 90 = axis straight up = the old model.
//   MOUNT_AXIS_YAW_DEG:  + = axis turned outward, toward your right.
#define MOUNT_AXIS_TILT_DEG   0.0f
#define MOUNT_AXIS_YAW_DEG    0.0f

// ── Body geometry, BODY frame  [VERIFY — measure YOUR body] ──
// Estimated from the worn photo: the origin sits ~110 mm behind the
// back surface, the spine ~110 mm to its left. The torso is an elliptic
// cylinder down to the ground; your own right arm hangs beside it.
#define MOUNT_HEIGHT_ABOVE_GROUND_MM  1350.0f
#define BODY_TORSO_X_MM         220.0f   // torso centre (spine line)
#define BODY_TORSO_Y_MM         110.0f
#define BODY_TORSO_HALF_DEPTH_MM 110.0f  // front-back
#define BODY_TORSO_HALF_WIDTH_MM 190.0f  // side-side
#define BODY_TORSO_TOP_Z_MM     150.0f
#define BODY_TORSO_BOTTOM_Z_MM  (-MOUNT_HEIGHT_ABOVE_GROUND_MM)
#define BODY_RARM_X_MM          220.0f   // your right arm, hanging
#define BODY_RARM_Y_MM         -130.0f
#define BODY_RARM_RADIUS_MM      60.0f
#define BODY_RARM_TOP_Z_MM      120.0f
#define BODY_RARM_BOTTOM_Z_MM  -600.0f
#define BODY_HEAD_X_MM          230.0f
#define BODY_HEAD_Y_MM          110.0f
#define BODY_HEAD_Z_MM          330.0f
#define BODY_HEAD_RADIUS_MM     120.0f
#define BODY_NECK_X_MM          230.0f
#define BODY_NECK_Y_MM          110.0f
#define BODY_NECK_Z_MM          200.0f
#define BODY_NECK_RADIUS_MM      70.0f
#define BODY_LEFT_SHOULDER_X_MM  220.0f
#define BODY_LEFT_SHOULDER_Y_MM  300.0f
#define BODY_LEFT_SHOULDER_Z_MM  120.0f
#define BODY_LEFT_SHOULDER_RADIUS_MM 110.0f
#define BODY_RIGHT_SHOULDER_X_MM 220.0f
#define BODY_RIGHT_SHOULDER_Y_MM -80.0f
#define BODY_RIGHT_SHOULDER_Z_MM 120.0f
#define BODY_RIGHT_SHOULDER_RADIUS_MM 110.0f
#define COLLISION_MARGIN_MM     25.0f
#define COLLISION_SEGMENTS       8
// Link 1 starts behind the back now (it used to start inside the torso
// cylinder), so every link is checked against the torso.
#define TORSO_CHECK_START_SEG    0

// ── Delivery / hand-off pose ─────────────────────────────────
// The grab FSM delivers to the TAUGHT "handoff" pose (no fixed
// coordinates any more), keeping the gripper closed on the way, holds
// it out for HANDOFF_HOLD_MS so the person can take hold, then opens and
// waits HANDOFF_RELEASE_MS before going home. Hold the object by then:
// the jaws open whether or not anyone has.
#define HANDOFF_HOLD_MS      3000
#define HANDOFF_RELEASE_MS   1500

// ── HARD joint limits (output shaft degrees) ─────────────────
// Nothing (IK, jog, taught positions) may command outside these,
// and a tracked joint beyond them (+ JOINT_LIMIT_SLACK_DEG) while
// powered triggers an emergency stop.
// Arm-frame angles (see Mount above): base 0 = hanging plumb, + = toward
// your right; shoulder 0 = link 1 along the back plane, +90 = straight back
// along the base axis. [VERIFY] against the real cable/hard-stop range.
#define BASE_MIN_DEG      -30.0f
#define BASE_MAX_DEG      130.0f
#define SHOULDER_MIN_DEG  -100.0f
#define SHOULDER_MAX_DEG  200.0f
#define ELBOW_MIN_DEG    -160.0f
#define ELBOW_MAX_DEG     160.0f
#define FOREARM_MIN_DEG  -160.0f
#define FOREARM_MAX_DEG   160.0f
#define GRIPPER_MIN_DEG     0.0f
#define GRIPPER_MAX_DEG    90.0f
#define JOINT_LIMIT_SLACK_DEG  8.0f

// ── Home / BOOT REFERENCE pose ───────────────────────────────
// The angles the zero reference pose is assigned: by "Set this position
// as 0", and at every boot (the boot pose is taken to be this pose).
// Taught positions are stored relative to it.
// Worn, this is the arm hanging behind you, close to the photo pose:
//   base 0      the arm hangs straight down, not swung left or right;
//   shoulder 30 link 1 tilted 30 deg back from vertical;
//   elbow -30   the forearm link plumb (vertical) again;
//   forearm 0   the wrist straight, in line with the forearm link.
// Phone inclinometer: link 1 reads 30 deg from vertical, the links below
// it read vertical (add MOUNT_AXIS_TILT_DEG if that is not 0).
#define HOME_BASE_DEG        0.0f
#define HOME_SHOULDER_DEG   30.0f
#define HOME_ELBOW_DEG     -30.0f
#define HOME_FOREARM_DEG     0.0f

// ── Tool pitch strategy ───────────────────────────────────────
// Tool elevation in the BODY frame: 0 = gripper level, -90 = pointing
// straight down. The arm-frame wrist angle that gives it depends on the
// base angle (solveWithPitches); when the arm plane can't reach it, the
// closest elevation it can reach is used.
#define TOOL_PITCH_LOW_DEG   -90.0f
#define TOOL_PITCH_MID_DEG   -60.0f
#define TOOL_PITCH_HIGH_DEG  -30.0f

// ── Speeds ────────────────────────────────────────────────────
// ST3215 steps/sec at safety scale = 1.0 (the lead joint of a move
// gets this speed; the others are slowed so all four arrive together).
// The real speed sent is  raw * g_safetyScale.  At the default 10%
// scale, HOME_SPEED 2000 -> 200 steps/s (~17 deg/s at the servo).
#define SPEED_STEPS_MAX  3400     // ST3215 ceiling used for wheel-mode runs
#define DEFAULT_SPEED    2000
#define APPROACH_SPEED   1500
#define DESCEND_SPEED     800
#define LIFT_SPEED       1400
#define HOME_SPEED       2000
#define HANDOFF_SPEED    1200
#define JOG_SPEED        1000
#define MIN_JOINT_SPEED     6     // slowest a joint is ever commanded (steps/s)

// ── Torque ────────────────────────────────────────────────────
// REG_TORQUE_LIMIT, 0-1000 = 0-100 % of the servo's output. It caps the
// PWM duty, so it limits both stall force and top speed.
#define DEFAULT_TORQUE   700
#define GRIP_TORQUE      350
// Base: a jam at 700 puts ~960 N on the printed worm/gear (FEA SF 0.17).
// 350 halves that and still lifts the arm straight out sideways at a 10:1
// cycloid with margin (~20 % duty needed). It also caps the base's wheel-mode
// speed at roughly 35 % of the servo's no-load maximum. First-boot default;
// runtime-editable in the Joint calibration card. [VERIFY]
#define BASE_TORQUE      350
// Base stall cut-off: a base move that makes no encoder progress for this
// long holds where it is (no fault, the worm self-locks). Other joints: 1 s.
#define BASE_NO_MOTION_MS  500
// Base torque cap step-down. While the base moves its load is read every joint
// tick. Load at BASE_CAP_LOAD_PCT % of its torque limit for BASE_CAP_HOLD_MS:
// above BASE_CAP_SLOW_SPEED the move drops to that speed and carries on; at or
// below it, the move stops and holds (no fault, the worm self-locks). Each start,
// step-down and live spin-speed change gets BASE_CAP_GRACE_MS to accelerate first.
// In wheel mode the load register tracks motor effort, which is mostly speed
// (a 30 % spin read ~310 of a 350 limit), so a fast spin will step down on its
// own. The slow speed is servo steps/s, NOT safety-scaled (20 % of the wheel
// ceiling), so a normal scaled move is already below it and just stops. [VERIFY]
#define BASE_CAP_LOAD_PCT    95
#define BASE_CAP_HOLD_MS     200
#define BASE_CAP_GRACE_MS    300
#define BASE_CAP_SLOW_SPEED  680

// ── Grab sequence clearances (straight up, BODY frame) ────────
#define APPROACH_CLEARANCE_MM   80.0f
#define LIFT_HEIGHT_MM         100.0f

// ── Taught-envelope margins (applied to the FK of taught poses) ─
#define ENVELOPE_MARGIN_MM      15.0f

// ── CV ────────────────────────────────────────────────────────
#define CV_CONFIDENCE_THRESHOLD  0.60f

// ── Camera intrinsics (VGA 640x480 — matches STREAM_FRAMESIZE) ─
#define CAM_FX  614.4f
#define CAM_FY  625.6f
#define CAM_CX  320.0f
#define CAM_CY  240.0f
#define CAM_W   640
#define CAM_H   480

// ── Pins (XIAO ESP32S3: D6 = GPIO43 TX, D7 = GPIO44 RX) ──────
#define PIN_SERVO_RX    44
#define PIN_SERVO_TX    43
#define PIN_SERVO_DIR    -1
#define SERVO_BAUD  1000000

// ── Gripper bus ───────────────────────────────────────────────
// One resistor junction cannot drive five ST3215s at 1 Mbaud. Measured
// 2026-09-27: the four joints alone are clean (even with the TX wire
// being shaken), the gripper alone is clean, and all five on one junction
// fail (base/elbow never answer, the rest mostly garbled). So the gripper
// gets its OWN junction on a second UART (Serial2), wired exactly like
// the first: TX through 300R to the node, RX through 10k to the node,
// gripper data wire on the node. No pull-up: TX idles high through the 300R.
// Set GRIPPER_OWN_BUS 0 to put the gripper back on the main bus (e.g. once
// a proper driver board replaces the resistor junction).
#define GRIPPER_OWN_BUS   1
#define PIN_GRIP_RX       5   // D4 = GPIO5  [VERIFY against the actual wiring]
#define PIN_GRIP_TX       6   // D5 = GPIO6  [VERIFY against the actual wiring]

// ── Timing ────────────────────────────────────────────────────
// Upper bound for a blocking wait. Each joint also has its own
// distance/speed-based deadline, so this only matters as a backstop.
#define MOTION_TIMEOUT_MS   240000
#define POLL_INTERVAL_MS       15
#define JOINT_POLL_MS          25   // how often trackers read the encoders
#define WRAP_THRESH          3000   // steps; tracker wrap detection
#define POS_TOL_STEPS           8   // "arrived" tolerance in servo steps

// Mode 0 is SINGLE-TURN on the ST3215: raw 0 and raw 4095 are opposite ends
// of its range, not neighbours. A servo sitting within a few steps of that
// wrap that dips across it (hunting, gear/belt unwind when torque drops for
// a mode switch, gravity) suddenly sees its goal ~4000 steps away and drives
// the long way round — first wear test: base and elbow "rotate some, go back,
// rotate some".
//  WHEEL_HANDOFF_STEPS: a wheel leg keeps spinning until it is this far past
//    the wrap it entered through (or has reached the target) before Mode 0
//    takes over — not the instant it crosses.
//  WRAP_GUARD_STEPS: no Mode-0 goal is ever placed closer than this to the
//    wrap; a target that lands there is nudged inward (<= 0.5 deg output).
//  MAX_LEAPFROG_REPLANS: if a joint still ends up on the wrong side of the
//    wrap, it goes back to wheel mode at most this many times per move before
//    the move is aborted (holding, no fault).
#define WHEEL_HANDOFF_STEPS   256
#define WRAP_GUARD_STEPS       12
#define MAX_LEAPFROG_REPLANS    3

// ── Boot-pose signature check ────────────────────────────────
// Raw encoder reading at boot is compared with the one saved at
// calibration. A big difference => arm was NOT in the same pose.
#define BOOT_SIG_TOL_STEPS    220

// ── Wi-Fi ─────────────────────────────────────────────────────
// 0 = AUTO (try hotspot, then fall back to own access point)
// 1 = AP only  (ESP32 makes its own network — recommended when worn)
// 2 = STA only
#define WIFI_BOOT_MODE        0
#define WIFI_STA_TIMEOUT_MS   12000
// Regulatory country for the radio. Out of the box the ESP32 only scans
// channels 1-11 actively, so a phone hotspot that picks channel 12 or 13
// (my phone's did, 2026-10-02) is never joined and the arm
// falls back to its own AP. "AE" (UAE) allows 1-13. 802.11d is off so an AP's
// beacon can't narrow it back down. [VERIFY] set your own country code.
#define WIFI_COUNTRY          "AE"
// Credentials live in secrets.h (gitignored).

// ============================================================
//  SAFETY CONFIGURATION
// ============================================================

// Master speed/acceleration multiplier (0.0 – 1.0).
//   0.10 → very cautious (default)   0.25 → tuning runs
//   0.50 → normal testing            1.00 → only when confident
#define SAFETY_SCALE_DEFAULT   0.10f

// Acceleration at scale=1.0 (steps/ms² register units). 0 = hw max.
// REG_GOAL_ACCEL (0x29) may not exist on every ST3215 firmware.
#define SAFETY_ACCEL_MAX       40

#define SAFETY_TEMP_MAX_C      65
// Consecutive over-limit readings (per servo) before an E-STOP. Each servo is
// read every 5 x SAFETY_MONITOR_MS = 750 ms, so 3 strikes ~= 2 s. A single
// reading used to trip it: first on-bench run, the elbow E-STOPped at "67C"
// with a cold housing. A real over-temp persists; a one-off reading doesn't.
#define SAFETY_TEMP_STRIKES    3
#define SAFETY_LOAD_MAX        820
#define SAFETY_LOAD_STRIKES    3
#define SAFETY_VOLTAGE_MIN_DV  65    // 6.5 V
#define SAFETY_VOLTAGE_MAX_DV  140   // 14.0 V
#define SAFETY_MONITOR_MS      150

// Wheel-mode legs: if no encoder read succeeds for this long,
// the joint is stopped and an E-STOP is raised.
#define WHEEL_COMMS_TIMEOUT_MS 300
