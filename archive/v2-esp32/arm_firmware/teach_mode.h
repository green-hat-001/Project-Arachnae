#pragma once
// ============================================================
//  PROJET ARACHNAE v2 — Teach Mode
//
//  BUG FIX vs original:
//   [FIX 4] teachStatusJSON used `!= 0` to detect whether a
//           position had been saved. This incorrectly reports
//           unsaved for any position whose step value is
//           legitimately 0 — e.g. HOME with base at 0°
//           (forward) genuinely produces home_base = 0 steps.
//           Fixed by adding a `savedFlags` bitmask that is set
//           explicitly in savePosition() and persisted to flash.
// ============================================================

#include <Preferences.h>
#include "servo_control.h"
#include "kinematics.h"

// ── Flash storage namespace ──────────────────────────────────
#define PREFS_NAMESPACE  "arachnae"

// ── Teach position keys ──────────────────────────────────────
#define KEY_HOME_BASE       "h_base"
#define KEY_HOME_SH         "h_sh"
#define KEY_HOME_EL         "h_el"
#define KEY_HOME_FA         "h_fa"
#define KEY_HOME_GRIP       "h_grip"

#define KEY_FWD_BASE        "f_base"
#define KEY_FWD_SH          "f_sh"
#define KEY_FWD_EL          "f_el"
#define KEY_FWD_FA          "f_fa"

#define KEY_HANDOFF_BASE    "d_base"
#define KEY_HANDOFF_SH      "d_sh"
#define KEY_HANDOFF_EL      "d_el"
#define KEY_HANDOFF_FA      "d_fa"
#define KEY_HANDOFF_GRIP    "d_grip"

#define KEY_LEFT_BASE       "l_base"
#define KEY_RIGHT_BASE      "r_base"
#define KEY_HEIGHT_MAX_SH   "zmax_sh"
#define KEY_HEIGHT_MAX_EL   "zmax_el"
#define KEY_HEIGHT_MAX_FA   "zmax_fa"
#define KEY_DEPTH_MAX_SH    "zmin_sh"
#define KEY_DEPTH_MAX_EL    "zmin_el"
#define KEY_DEPTH_MAX_FA    "zmin_fa"

#define KEY_CALIBRATED      "calibrated"
#define KEY_SAVED_FLAGS     "sv_flags"   // [FIX 4] new key

// ── [FIX 4] Saved-position bitmask definitions ───────────────
#define SAVED_HOME       (1u << 0)
#define SAVED_FWD        (1u << 1)
#define SAVED_HANDOFF    (1u << 2)
#define SAVED_LEFT       (1u << 3)
#define SAVED_RIGHT      (1u << 4)
#define SAVED_HEIGHT_MAX (1u << 5)
#define SAVED_DEPTH_MAX  (1u << 6)

// ── Taught position store (RAM copy, loaded from flash) ──────
struct TaughtPositions {
  int32_t home_base, home_sh, home_el, home_fa;
  int16_t home_grip;

  int32_t fwd_base, fwd_sh, fwd_el, fwd_fa;

  int32_t handoff_base, handoff_sh, handoff_el, handoff_fa;
  int16_t handoff_grip;

  int32_t left_base;
  int32_t right_base;

  int32_t height_max_sh, height_max_el, height_max_fa;
  int32_t depth_max_sh,  depth_max_el,  depth_max_fa;

  bool    calibrated;
  uint8_t savedFlags;  // [FIX 4] explicit bitmask, replaces != 0 heuristic
};

static TaughtPositions g_taught;
static Preferences     g_prefs;
static bool            g_teachActive = false;

struct LiveReadings {
  int32_t base, sh, el, fa;
  int16_t grip;
  float   base_deg, sh_deg, el_deg, fa_deg;
};

// ============================================================
//  Read all present positions from servos
// ============================================================
LiveReadings readAllPositions() {
  LiveReadings r;
  r.base = g_bus.readDWord(SERVO_BASE,     REG_PRESENT_POS);
  r.sh   = g_bus.readDWord(SERVO_SHOULDER, REG_PRESENT_POS);
  r.el   = g_bus.readDWord(SERVO_ELBOW,    REG_PRESENT_POS);
  r.fa   = g_bus.readDWord(SERVO_FOREARM,  REG_PRESENT_POS);
  r.grip = g_bus.readWord (SERVO_GRIPPER,  REG_PRESENT_POS);

  r.base_deg = degrees(stepsToAngle(r.base, RATIO_BASE));
  r.sh_deg   = degrees(stepsToAngle(r.sh,   RATIO_SHOULDER));
  r.el_deg   = degrees(stepsToAngle(r.el,   RATIO_ELBOW));
  r.fa_deg   = degrees(stepsToAngle(r.fa,   RATIO_FOREARM));
  return r;
}

// ============================================================
//  Teach mode enable / disable
// ============================================================
void teachModeStart() {
  g_teachActive = true;
  uint8_t ids[] = {SERVO_BASE, SERVO_SHOULDER, SERVO_ELBOW,
                   SERVO_FOREARM, SERVO_GRIPPER};
  for (uint8_t id : ids) { g_bus.writeByte(id, REG_TORQUE_ENABLE, 0); delay(8); }
  Serial.println(F("[Teach] Torque OFF — arm is free to move"));
}

void teachModeEnd() {
  g_teachActive = false;
  uint8_t ids[] = {SERVO_BASE, SERVO_SHOULDER, SERVO_ELBOW,
                   SERVO_FOREARM, SERVO_GRIPPER};
  for (uint8_t id : ids) { g_bus.writeByte(id, REG_TORQUE_ENABLE, 1); delay(8); }
  Serial.println(F("[Teach] Torque ON — arm locked"));
}

// ============================================================
//  Save a position to flash
//  [FIX 4] Sets the corresponding savedFlags bit so status
//          reporting is no longer fooled by zero-step positions.
// ============================================================
bool savePosition(const char* positionName) {
  LiveReadings r = readAllPositions();
  g_prefs.begin(PREFS_NAMESPACE, false);

  String name = String(positionName);

  if (name == "home") {
    g_prefs.putInt(KEY_HOME_BASE, r.base);
    g_prefs.putInt(KEY_HOME_SH,   r.sh);
    g_prefs.putInt(KEY_HOME_EL,   r.el);
    g_prefs.putInt(KEY_HOME_FA,   r.fa);
    g_prefs.putShort(KEY_HOME_GRIP, r.grip);
    g_taught.home_base = r.base; g_taught.home_sh = r.sh;
    g_taught.home_el   = r.el;   g_taught.home_fa = r.fa;
    g_taught.home_grip = r.grip;
    g_taught.savedFlags |= SAVED_HOME;                     // [FIX 4]

  } else if (name == "fwd_reach") {
    g_prefs.putInt(KEY_FWD_BASE, r.base);
    g_prefs.putInt(KEY_FWD_SH,  r.sh);
    g_prefs.putInt(KEY_FWD_EL,  r.el);
    g_prefs.putInt(KEY_FWD_FA,  r.fa);
    g_taught.fwd_base = r.base; g_taught.fwd_sh = r.sh;
    g_taught.fwd_el   = r.el;   g_taught.fwd_fa = r.fa;
    g_taught.savedFlags |= SAVED_FWD;                      // [FIX 4]

  } else if (name == "handoff") {
    g_prefs.putInt(KEY_HANDOFF_BASE, r.base);
    g_prefs.putInt(KEY_HANDOFF_SH,   r.sh);
    g_prefs.putInt(KEY_HANDOFF_EL,   r.el);
    g_prefs.putInt(KEY_HANDOFF_FA,   r.fa);
    g_prefs.putShort(KEY_HANDOFF_GRIP, r.grip);
    g_taught.handoff_base = r.base; g_taught.handoff_sh = r.sh;
    g_taught.handoff_el   = r.el;   g_taught.handoff_fa = r.fa;
    g_taught.handoff_grip = r.grip;
    g_taught.savedFlags |= SAVED_HANDOFF;                  // [FIX 4]

  } else if (name == "left_limit") {
    g_prefs.putInt(KEY_LEFT_BASE, r.base);
    g_taught.left_base = r.base;
    g_taught.savedFlags |= SAVED_LEFT;                     // [FIX 4]

  } else if (name == "right_limit") {
    g_prefs.putInt(KEY_RIGHT_BASE, r.base);
    g_taught.right_base = r.base;
    g_taught.savedFlags |= SAVED_RIGHT;                    // [FIX 4]

  } else if (name == "height_max") {
    g_prefs.putInt(KEY_HEIGHT_MAX_SH, r.sh);
    g_prefs.putInt(KEY_HEIGHT_MAX_EL, r.el);
    g_prefs.putInt(KEY_HEIGHT_MAX_FA, r.fa);
    g_taught.height_max_sh = r.sh;
    g_taught.height_max_el = r.el;
    g_taught.height_max_fa = r.fa;
    g_taught.savedFlags |= SAVED_HEIGHT_MAX;               // [FIX 4]

  } else if (name == "depth_max") {
    g_prefs.putInt(KEY_DEPTH_MAX_SH, r.sh);
    g_prefs.putInt(KEY_DEPTH_MAX_EL, r.el);
    g_prefs.putInt(KEY_DEPTH_MAX_FA, r.fa);
    g_taught.depth_max_sh = r.sh;
    g_taught.depth_max_el = r.el;
    g_taught.depth_max_fa = r.fa;
    g_taught.savedFlags |= SAVED_DEPTH_MAX;                // [FIX 4]

  } else {
    g_prefs.end();
    return false;
  }

  g_prefs.putBool (KEY_CALIBRATED,  true);
  g_prefs.putUChar(KEY_SAVED_FLAGS, g_taught.savedFlags);  // [FIX 4]
  g_taught.calibrated = true;
  g_prefs.end();

  Serial.printf("[Teach] Saved '%s': base=%.1f° sh=%.1f° el=%.1f° fa=%.1f°\n",
    positionName, r.base_deg, r.sh_deg, r.el_deg, r.fa_deg);
  return true;
}

// ============================================================
//  Load all taught positions from flash into RAM
// ============================================================
void loadTaughtPositions() {
  g_prefs.begin(PREFS_NAMESPACE, true);
  g_taught.calibrated  = g_prefs.getBool (KEY_CALIBRATED, false);
  g_taught.savedFlags  = g_prefs.getUChar(KEY_SAVED_FLAGS, 0);  // [FIX 4]

  if (!g_taught.calibrated) {
    g_prefs.end();
    Serial.println(F("[Teach] No calibration data — using firmware defaults"));
    return;
  }

  g_taught.home_base = g_prefs.getInt(KEY_HOME_BASE, 0);
  g_taught.home_sh   = g_prefs.getInt(KEY_HOME_SH,   angleToSteps(radians(HOME_SHOULDER_DEG), RATIO_SHOULDER));
  g_taught.home_el   = g_prefs.getInt(KEY_HOME_EL,   angleToSteps(radians(HOME_ELBOW_DEG),    RATIO_ELBOW));
  g_taught.home_fa   = g_prefs.getInt(KEY_HOME_FA,   angleToSteps(radians(HOME_FOREARM_DEG),  RATIO_FOREARM));
  g_taught.home_grip = g_prefs.getShort(KEY_HOME_GRIP, SERVO_STEPS_CENTRE);

  g_taught.fwd_base = g_prefs.getInt(KEY_FWD_BASE, 0);
  g_taught.fwd_sh   = g_prefs.getInt(KEY_FWD_SH,   0);
  g_taught.fwd_el   = g_prefs.getInt(KEY_FWD_EL,   0);
  g_taught.fwd_fa   = g_prefs.getInt(KEY_FWD_FA,   0);

  g_taught.handoff_base = g_prefs.getInt(KEY_HANDOFF_BASE, 0);
  g_taught.handoff_sh   = g_prefs.getInt(KEY_HANDOFF_SH,   0);
  g_taught.handoff_el   = g_prefs.getInt(KEY_HANDOFF_EL,   0);
  g_taught.handoff_fa   = g_prefs.getInt(KEY_HANDOFF_FA,   0);
  g_taught.handoff_grip = g_prefs.getShort(KEY_HANDOFF_GRIP, SERVO_STEPS_CENTRE);

  g_taught.left_base  = g_prefs.getInt(KEY_LEFT_BASE,  angleToSteps(radians(BASE_MAX_DEG), RATIO_BASE));
  g_taught.right_base = g_prefs.getInt(KEY_RIGHT_BASE, angleToSteps(radians(BASE_MIN_DEG), RATIO_BASE));

  g_taught.height_max_sh = g_prefs.getInt(KEY_HEIGHT_MAX_SH, 0);
  g_taught.height_max_el = g_prefs.getInt(KEY_HEIGHT_MAX_EL, 0);
  g_taught.height_max_fa = g_prefs.getInt(KEY_HEIGHT_MAX_FA, 0);
  g_taught.depth_max_sh  = g_prefs.getInt(KEY_DEPTH_MAX_SH,  0);
  g_taught.depth_max_el  = g_prefs.getInt(KEY_DEPTH_MAX_EL,  0);
  g_taught.depth_max_fa  = g_prefs.getInt(KEY_DEPTH_MAX_FA,  0);

  g_prefs.end();
  Serial.printf("[Teach] Calibration loaded — %d/7 positions set (flags=0x%02X)\n",
    __builtin_popcount(g_taught.savedFlags), g_taught.savedFlags);
}

// ============================================================
//  Move to a taught position (torque must be ON)
// ============================================================
void moveToTaught(const char* name, uint16_t speed = DEFAULT_SPEED) {
  if (!g_taught.calibrated) {
    Serial.println(F("[Teach] No calibration — run teach first"));
    return;
  }
  String n = String(name);

  auto go4 = [&](int32_t b, int32_t s, int32_t e, int32_t f) {
    g_bus.writeWord(SERVO_BASE,     REG_GOAL_SPEED, speed);
    g_bus.writeWord(SERVO_SHOULDER, REG_GOAL_SPEED, speed);
    g_bus.writeWord(SERVO_ELBOW,    REG_GOAL_SPEED, speed);
    g_bus.writeWord(SERVO_FOREARM,  REG_GOAL_SPEED, speed);
    g_bus.writeDWord(SERVO_BASE,     REG_GOAL_POSITION, b);
    g_bus.writeDWord(SERVO_SHOULDER, REG_GOAL_POSITION, s);
    g_bus.writeDWord(SERVO_ELBOW,    REG_GOAL_POSITION, e);
    g_bus.writeDWord(SERVO_FOREARM,  REG_GOAL_POSITION, f);
  };

  if (n == "home") {
    go4(g_taught.home_base, g_taught.home_sh,
        g_taught.home_el,   g_taught.home_fa);
    g_bus.writeWord(SERVO_GRIPPER, REG_GOAL_SPEED,    speed);
    g_bus.writeWord(SERVO_GRIPPER, REG_GOAL_POSITION, (uint16_t)g_taught.home_grip);
  } else if (n == "handoff") {
    go4(g_taught.handoff_base, g_taught.handoff_sh,
        g_taught.handoff_el,   g_taught.handoff_fa);
    g_bus.writeWord(SERVO_GRIPPER, REG_GOAL_SPEED,    speed);
    g_bus.writeWord(SERVO_GRIPPER, REG_GOAL_POSITION, (uint16_t)g_taught.handoff_grip);
  } else if (n == "fwd_reach") {
    go4(g_taught.fwd_base, g_taught.fwd_sh,
        g_taught.fwd_el,   g_taught.fwd_fa);
  } else if (n == "height_max") {
    go4(g_taught.home_base, g_taught.height_max_sh,
        g_taught.height_max_el, g_taught.height_max_fa);
  } else if (n == "depth_max") {
    go4(g_taught.home_base, g_taught.depth_max_sh,
        g_taught.depth_max_el, g_taught.depth_max_fa);
  }
}

// ============================================================
//  Build JSON status string for web UI
//  [FIX 4] Uses savedFlags bitmask — not the value != 0 heuristic
// ============================================================
String teachStatusJSON() {
  LiveReadings r = readAllPositions();
  char buf[512];
  snprintf(buf, sizeof(buf),
    "{"
    "\"teach_active\":%s,"
    "\"calibrated\":%s,"
    "\"live\":{"
      "\"base_deg\":%.1f,"
      "\"sh_deg\":%.1f,"
      "\"el_deg\":%.1f,"
      "\"fa_deg\":%.1f"
    "},"
    "\"saved\":{"
      "\"home\":%s,"
      "\"fwd_reach\":%s,"
      "\"handoff\":%s,"
      "\"left_limit\":%s,"
      "\"right_limit\":%s,"
      "\"height_max\":%s,"
      "\"depth_max\":%s"
    "}"
    "}",
    g_teachActive ? "true" : "false",
    g_taught.calibrated ? "true" : "false",
    r.base_deg, r.sh_deg, r.el_deg, r.fa_deg,
    // [FIX 4] bitmask checks replace the fragile != 0 comparisons
    (g_taught.savedFlags & SAVED_HOME)       ? "true" : "false",
    (g_taught.savedFlags & SAVED_FWD)        ? "true" : "false",
    (g_taught.savedFlags & SAVED_HANDOFF)    ? "true" : "false",
    (g_taught.savedFlags & SAVED_LEFT)       ? "true" : "false",
    (g_taught.savedFlags & SAVED_RIGHT)      ? "true" : "false",
    (g_taught.savedFlags & SAVED_HEIGHT_MAX) ? "true" : "false",
    (g_taught.savedFlags & SAVED_DEPTH_MAX)  ? "true" : "false"
  );
  return String(buf);
}

// ============================================================
//  Clear all taught data (factory reset)
// ============================================================
void clearAllTaught() {
  g_prefs.begin(PREFS_NAMESPACE, false);
  g_prefs.clear();
  g_prefs.end();
  memset(&g_taught, 0, sizeof(g_taught));
  g_taught.calibrated = false;
  g_taught.savedFlags = 0;
  Serial.println(F("[Teach] All calibration data cleared"));
}
