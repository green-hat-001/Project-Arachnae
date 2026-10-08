#pragma once
// ============================================================
//  PROJET ARACHNAE v2 — Configuration
//  Shoulder-blade mounted, right side, reach-around to front
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

// ── Gear ratios ──────────────────────────────────────────────
#define RATIO_BASE      10.0f
#define RATIO_SHOULDER   6.0f
#define RATIO_ELBOW      5.0f
#define RATIO_FOREARM    2.0f
#define RATIO_GRIPPER    1.0f

// ── Link lengths (mm) ────────────────────────────────────────
#define L1_MM   150.0f
#define L2_MM   400.0f
#define L3_MM   350.0f
#define TOOL_MM  65.0f

// ── Body geometry ────────────────────────────────────────────
#define MOUNT_HEIGHT_ABOVE_GROUND_MM  1155.0f
#define BODY_SPINE_Y_MM          180.0f
#define BODY_SPINE_X_MM         -60.0f
#define BODY_TORSO_RADIUS_MM    155.0f
#define BODY_TORSO_TOP_Z_MM     445.0f
#define BODY_TORSO_BOTTOM_Z_MM -1155.0f
#define BODY_HEAD_X_MM          -30.0f
#define BODY_HEAD_Y_MM          180.0f
#define BODY_HEAD_Z_MM          445.0f
#define BODY_HEAD_RADIUS_MM     130.0f
#define BODY_LEFT_SHOULDER_X_MM  20.0f
#define BODY_LEFT_SHOULDER_Y_MM 380.0f
#define BODY_LEFT_SHOULDER_Z_MM 265.0f
#define BODY_LEFT_SHOULDER_RADIUS_MM 140.0f
#define BODY_NECK_X_MM           0.0f
#define BODY_NECK_Y_MM          60.0f
#define BODY_NECK_Z_MM         385.0f
#define BODY_NECK_RADIUS_MM     80.0f
#define COLLISION_MARGIN_MM     25.0f
#define COLLISION_SEGMENTS       8
#define TORSO_CHECK_START_SEG    1

// ── Delivery / hand-off pose ─────────────────────────────────
#define HANDOFF_X_MM   320.0f
#define HANDOFF_Y_MM  -120.0f
#define HANDOFF_Z_MM   -55.0f
#define HANDOFF_TOOL_PITCH_DEG  -60.0f

// ── Joint limits (output shaft degrees) ──────────────────────
#define BASE_MIN_DEG      -30.0f
#define BASE_MAX_DEG      130.0f
#define SHOULDER_MIN_DEG  -110.0f
#define SHOULDER_MAX_DEG  270.0f
#define ELBOW_MIN_DEG    -160.0f
#define ELBOW_MAX_DEG     160.0f
#define FOREARM_MIN_DEG  -160.0f
#define FOREARM_MAX_DEG   160.0f
#define GRIPPER_MIN_DEG     0.0f
#define GRIPPER_MAX_DEG    90.0f

// ── Home position ─────────────────────────────────────────────
#define HOME_BASE_DEG        0.0f
#define HOME_SHOULDER_DEG  -60.0f
#define HOME_ELBOW_DEG     120.0f
#define HOME_FOREARM_DEG    30.0f
#define HOME_GRIPPER_DEG    45.0f

// ── Tool pitch strategy ───────────────────────────────────────
#define TOOL_PITCH_LOW_DEG   -90.0f
#define TOOL_PITCH_MID_DEG   -60.0f
#define TOOL_PITCH_HIGH_DEG  -30.0f

// ── Speeds (ST3215 steps/sec, BEFORE safety scaling) ─────────
// At the default SAFETY_SCALE_DEFAULT of 0.10 (10%), all of
// these are multiplied by 0.10 before being sent to the servos.
// Example: DEFAULT_SPEED 700 → 70 steps/sec at 10% scale.
// Increase SAFETY_SCALE_DEFAULT gradually as you gain confidence.
#define DEFAULT_SPEED    700
#define APPROACH_SPEED   500
#define DESCEND_SPEED    250
#define LIFT_SPEED       450
#define HOME_SPEED       900
#define HANDOFF_SPEED    400

// ── Torque ────────────────────────────────────────────────────
#define DEFAULT_TORQUE   700
#define GRIP_TORQUE      350

// ── Grab sequence clearances ──────────────────────────────────
#define APPROACH_CLEARANCE_MM   80.0f
#define LIFT_HEIGHT_MM         100.0f

// ── CV ────────────────────────────────────────────────────────
#define CV_CONFIDENCE_THRESHOLD  0.60f

// ── Camera intrinsics ─────────────────────────────────────────
#define CAM_FX  768.0f
#define CAM_FY  782.0f
#define CAM_CX  400.0f
#define CAM_CY  300.0f
#define CAM_W   800
#define CAM_H   600

// ── Pins ──────────────────────────────────────────────────────
#define PIN_SERVO_RX    D7
#define PIN_SERVO_TX    D6
#define PIN_SERVO_DIR    -1
#define SERVO_BAUD  1000000

// ── Timing ────────────────────────────────────────────────────
#define MOTION_TIMEOUT_MS   8000
#define POLL_INTERVAL_MS      20

// ============================================================
//  SAFETY CONFIGURATION
//  Adjust these to tune the safety system for your setup.
// ============================================================

// Master speed/acceleration multiplier (0.0 – 1.0).
// ALL speed and acceleration values are scaled by this before
// being sent to servos. Raise gradually — never jump to 1.0.
//   0.10 → 10% speed (default, very cautious)
//   0.25 → 25% speed (good for initial tuning runs)
//   0.50 → 50% speed (normal testing)
//   1.00 → 100% speed (full speed, only when confident)
#define SAFETY_SCALE_DEFAULT   0.10f

// Maximum acceleration at scale=1.0, in steps/ms².
// Scaled the same as speed: at 10% → safeAccel() = 4.
// Lower = gentler ramp-up. 0 = hardware maximum (no limiting).
// Note: REG_GOAL_ACCEL (0x29) may not be present on all ST3215
// firmware versions. If servos misbehave after init, set this
// to 0 to disable acceleration control.
#define SAFETY_ACCEL_MAX       40

// Temperature limit (°C). Emergency stop if any servo exceeds this.
// ST3215 max rated: ~80°C. 65°C gives a 15° safety margin.
#define SAFETY_TEMP_MAX_C      65

// Load/torque limit (0–1000, raw register value, magnitude only).
// Emergency stop after SAFETY_LOAD_STRIKES consecutive polls above this.
// 820/1000 = 82% of max torque. Protects against mechanical binding.
// Lower this if you want to stop on lighter contact.
#define SAFETY_LOAD_MAX        820

// How many consecutive over-limit load readings before faulting.
// Prevents false trips from brief contact spikes (e.g. grip detection).
// At SAFETY_MONITOR_MS=150ms: 3 strikes = ~1.1s sustained overload.
#define SAFETY_LOAD_STRIKES    3

// Voltage limits (×0.1V).  60 = 6.0V,  140 = 14.0V.
// Sized for a 12V supply or 3S LiPo (11.1V–12.6V nominal).
// Tune down SAFETY_VOLTAGE_MIN_DV if running from a bench supply
// that shows higher droop under load.
#define SAFETY_VOLTAGE_MIN_DV  65    // 6.5V — gross undervolt / disconnected
#define SAFETY_VOLTAGE_MAX_DV  140   // 14.0V — overvoltage

// How often (ms) to sample one servo for health data.
// 5 servos × 150ms = each servo checked every 750ms.
#define SAFETY_MONITOR_MS      150
