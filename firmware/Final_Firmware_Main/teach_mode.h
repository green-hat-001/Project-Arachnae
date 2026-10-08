#pragma once
// ============================================================
//  PROJET ARACHNAE v2 — Teach Mode
//
//  Taught positions are stored as SERVO STEPS RELATIVE TO THE BOOT
//  REFERENCE POSE (the "effective" steps of servo_control.h), not as
//  angles. That means correcting a gear ratio or joint sign later does
//  NOT invalidate what you taught.
//
//  calibrated == all 7 positions saved.  (Old code set it after the
//  first save, which let the FSM drive to an unset "home".)
//
//  Everything here is NON-BLOCKING (preview / jog / home only start
//  a move; jointsTick() finishes it).
// ============================================================

#include <Preferences.h>
#include "servo_control.h"
#include "kinematics.h"

#define PREFS_NAMESPACE  "arachnae2"     // new namespace: old float/int layout is ignored

// ── Position slots ───────────────────────────────────────────
enum TaughtSlot : uint8_t {
  P_HOME = 0, P_FWD, P_HANDOFF, P_LEFT, P_RIGHT, P_HEIGHT, P_DEPTH, P_COUNT
};
static const char* const SLOT_NAMES[P_COUNT] = {
  "home", "fwd_reach", "handoff", "left_limit", "right_limit", "height_max", "depth_max"
};
#define SAVED_ALL  ((1u << P_COUNT) - 1u)

struct TaughtPose {
  int32_t eff[N_JOINTS];   // steps from boot reference (backlash removed)
  int16_t grip;            // gripper raw position, -1 = n/a
};

struct TaughtPositions {
  TaughtPose pose[P_COUNT];
  uint8_t    savedFlags;
  bool       calibrated;   // all 7 saved
};

static TaughtPositions g_taught;
static Preferences     g_prefs;

// boot-pose signature (encoder within-rev at boot vs at calibration)
static int16_t g_sigSaved[N_JOINTS] = {-1, -1, -1, -1};
static bool    g_sigHave = false;
static String  g_sigWarn = "";

struct LiveReadings {
  float base_deg, sh_deg, el_deg, fa_deg;
};

// ── helpers ──────────────────────────────────────────────────
static int slotFromName(const char* n) {
  for (int i = 0; i < P_COUNT; i++) if (!strcmp(n, SLOT_NAMES[i])) return i;
  return -1;
}

static JointAngles poseToAngles(const TaughtPose& p) {
  return { (float)radians(effToDeg(g_j[0], p.eff[0])), (float)radians(effToDeg(g_j[1], p.eff[1])),
           (float)radians(effToDeg(g_j[2], p.eff[2])), (float)radians(effToDeg(g_j[3], p.eff[3])) };
}

LiveReadings readAllPositions() {
  updateTrackers();
  LiveReadings r;
  r.base_deg = jDeg(g_j[0]); r.sh_deg = jDeg(g_j[1]);
  r.el_deg   = jDeg(g_j[2]); r.fa_deg = jDeg(g_j[3]);
  return r;
}

// ============================================================
//  Envelope: turn taught limits into IK limits
// ============================================================
void recomputeEnvelope() {
  g_env = RuntimeEnvelope();      // reset to config limits
  uint8_t f = g_taught.savedFlags;

  if ((f & (1u << P_LEFT)) && (f & (1u << P_RIGHT))) {
    float a = effToDeg(g_j[0], g_taught.pose[P_LEFT].eff[0]);
    float b = effToDeg(g_j[0], g_taught.pose[P_RIGHT].eff[0]);
    g_env.baseMinDeg = max((float)BASE_MIN_DEG, min(a, b));
    g_env.baseMaxDeg = min((float)BASE_MAX_DEG, max(a, b));
  }
  // Reach = straight-line distance from the origin; height = BODY-frame z of
  // the taught pose, with its own base angle (with the base axis pointing
  // back, swinging the base changes the height).
  if (f & (1u << P_FWD)) {
    JointAngles a = poseToAngles(g_taught.pose[P_FWD]);
    g_env.rMax = forwardKinematics(a.base, a.shoulder, a.elbow, a.forearm).norm() + ENVELOPE_MARGIN_MM;
    g_env.rValid = true;
  }
  if (f & (1u << P_HEIGHT)) {
    JointAngles a = poseToAngles(g_taught.pose[P_HEIGHT]);
    g_env.zMax = bodyZ(forwardKinematics(a.base, a.shoulder, a.elbow, a.forearm)) + ENVELOPE_MARGIN_MM;
    g_env.zMaxValid = true;
  }
  if (f & (1u << P_DEPTH)) {
    JointAngles a = poseToAngles(g_taught.pose[P_DEPTH]);
    g_env.zMin = bodyZ(forwardKinematics(a.base, a.shoulder, a.elbow, a.forearm)) - ENVELOPE_MARGIN_MM;
    g_env.zMinValid = true;
  }
  g_env.rzValid = g_env.rValid || g_env.zMaxValid || g_env.zMinValid;
}

// ============================================================
//  Boot reference confirmation ("arming")
// ============================================================
static void computeSigWarn() {
  g_sigWarn = "";
  if (!g_sigHave) return;
  for (int i = 1; i < N_JOINTS; i++) {          // skip base: backlash makes its raw value arbitrary
    if (g_j[i].bootRaw < 0 || g_sigSaved[i] < 0) continue;
    int d = abs((int)g_j[i].bootRaw - (int)g_sigSaved[i]);
    if (d > SERVO_STEPS_PER_REV / 2) d = SERVO_STEPS_PER_REV - d;
    if (d > BOOT_SIG_TOL_STEPS) {
      if (g_sigWarn.length()) g_sigWarn += ", ";
      g_sigWarn += String(g_j[i].name) + " differs by " + String(d) + " steps";
    }
  }
  if (g_sigWarn.length())
    g_sigWarn = "Boot pose differs from the one used at calibration: " + g_sigWarn;
}

// Returns "" on success, else an error string.
String armNow(bool force) {
  if (g_faultActive) return "fault active";
  for (auto& j : g_j) if (!j.ok) return String(j.name) + " servo not responding";
  if (g_sigWarn.length() && !force) return "SIGNATURE:" + g_sigWarn;
  g_armed = true;
  Serial.println(F("[Arm] Boot reference confirmed — motion enabled."));
  return "";
}

// ============================================================
//  Teach mode enable / disable
// ============================================================
void teachModeStart() {
  g_autoMode = false;
  abortAllMotion();
  g_teachActive = true;
  // Verified: a dropped torque-off would leave a joint live while the user
  // believes the arm is free and reaches in to pose it by hand.
  for (uint8_t id : ARM_IDS) {
    if (!writeByteVerified(id, REG_TORQUE_ENABLE, 0) && id != SERVO_GRIPPER)
      wsLog("[Teach] WARNING: ID" + String(id) + " did NOT release - still powered");
  }
  for (auto& j : g_j) j.powered = false;
  Serial.println(F("[Teach] Torque OFF — arm is free to move"));
}

// Re-energise holding EXACTLY where the arm is now (never snaps back).
void teachModeEnd() {
  g_teachActive = false;
  if (g_faultActive) return;
  updateTrackers();
  for (auto& j : g_j) if (j.ok) jointSetMode(j, 0);
  int32_t gp = gripperPos();
  busFor(SERVO_GRIPPER).writeByte(SERVO_GRIPPER, REG_TORQUE_ENABLE, 0);
  busFor(SERVO_GRIPPER).writeWord(SERVO_GRIPPER, REG_TORQUE_LIMIT, GRIP_TORQUE);
  busFor(SERVO_GRIPPER).writeWord(SERVO_GRIPPER, REG_GOAL_SPEED, safeSpeed(DEFAULT_SPEED));
  if (gp >= 0) busFor(SERVO_GRIPPER).writeWord(SERVO_GRIPPER, REG_GOAL_POSITION, (uint16_t)gp);
  busFor(SERVO_GRIPPER).writeByte(SERVO_GRIPPER, REG_TORQUE_ENABLE, 1);
  Serial.println(F("[Teach] Torque ON — holding current pose"));
}

// ============================================================
//  Save / load
// ============================================================
static void persistPose(int slot) {
  char k[8]; snprintf(k, sizeof(k), "p%d", slot);
  g_prefs.putBytes(k, &g_taught.pose[slot], sizeof(TaughtPose));
}

// Returns "" on success, else error text.
String savePosition(const char* positionName) {
  int slot = slotFromName(positionName);
  if (slot < 0)   return "unknown position";
  if (!g_armed)   return "confirm the boot pose first (Arm button)";
  updateTrackers();
  for (auto& j : g_j) if (!j.ok) return String(j.name) + " not tracked";

  TaughtPose& p = g_taught.pose[slot];
  for (int i = 0; i < N_JOINTS; i++) p.eff[i] = jEff(g_j[i]);
  int32_t gp = gripperPos();
  p.grip = (gp >= 0) ? (int16_t)gp : -1;
  g_taught.savedFlags |= (1u << slot);
  g_taught.calibrated = (g_taught.savedFlags & SAVED_ALL) == SAVED_ALL;

  g_prefs.begin(PREFS_NAMESPACE, false);
  persistPose(slot);
  g_prefs.putUChar("flags", g_taught.savedFlags);
  int16_t sig[N_JOINTS];
  for (int i = 0; i < N_JOINTS; i++) sig[i] = g_j[i].bootRaw;
  g_prefs.putBytes("bootsig", sig, sizeof(sig));
  g_prefs.end();
  memcpy(g_sigSaved, sig, sizeof(sig));
  g_sigHave = true;
  g_sigWarn = "";

  recomputeEnvelope();
  Serial.printf("[Teach] Saved '%s': base=%.1f sh=%.1f el=%.1f fa=%.1f deg\n", positionName,
                effToDeg(g_j[0], p.eff[0]), effToDeg(g_j[1], p.eff[1]),
                effToDeg(g_j[2], p.eff[2]), effToDeg(g_j[3], p.eff[3]));
  return "";
}

void loadTaughtPositions() {
  memset(&g_taught, 0, sizeof(g_taught));
  g_prefs.begin(PREFS_NAMESPACE, true);
  g_taught.savedFlags = g_prefs.getUChar("flags", 0) & SAVED_ALL;
  for (int s = 0; s < P_COUNT; s++) {
    if (!(g_taught.savedFlags & (1u << s))) continue;
    char k[8]; snprintf(k, sizeof(k), "p%d", s);
    if (g_prefs.getBytes(k, &g_taught.pose[s], sizeof(TaughtPose)) != sizeof(TaughtPose))
      g_taught.savedFlags &= ~(1u << s);          // corrupt slot -> treat as unset
  }
  int16_t sig[N_JOINTS];
  if (g_prefs.getBytes("bootsig", sig, sizeof(sig)) == sizeof(sig)) {
    memcpy(g_sigSaved, sig, sizeof(sig));
    g_sigHave = true;
  }
  g_prefs.end();

  g_taught.calibrated = (g_taught.savedFlags & SAVED_ALL) == SAVED_ALL;
  computeSigWarn();
  recomputeEnvelope();
  Serial.printf("[Teach] %d/7 positions loaded (flags=0x%02X)%s\n",
                __builtin_popcount(g_taught.savedFlags), g_taught.savedFlags,
                g_taught.calibrated ? " — CALIBRATED" : "");
  if (g_sigWarn.length()) Serial.println("[Teach] WARNING: " + g_sigWarn);
}

// ============================================================
//  Move to a taught position (non-blocking)
// ============================================================
bool moveToTaught(const char* name, uint16_t speed = DEFAULT_SPEED) {
  int slot = slotFromName(name);
  if (slot < 0 || !(g_taught.savedFlags & (1u << slot))) {
    Serial.printf("[Teach] '%s' not taught yet\n", name);
    return false;
  }
  if (!motionAllowed()) { Serial.println(F("[Teach] Move refused — not armed / fault")); return false; }
  const TaughtPose& p = g_taught.pose[slot];
  JointAngles a = poseToAngles(p);
  if (!armMoveToAngles(a, speed)) return false;
  if ((slot == P_HOME || slot == P_HANDOFF) && p.grip >= 0)
    setGripperSteps(p.grip, speed);
  return true;
}

// ============================================================
//  Runtime joint calibration (ratio / sign / wheel bit)
// ============================================================
// backlash (servo steps) and torque (REG_TORQUE_LIMIT) < 0 = leave unchanged.
String setJointCalibration(int idx, float ratio, int sign, int backlash = -1, int torque = -1) {
  if (idx < 0 || idx >= N_JOINTS) return "bad joint";
  if (!allJointsIdle()) return "arm is moving";
  if (ratio < 0.5f || ratio > 500.0f) return "ratio out of range";
  if (sign != 1 && sign != -1) return "sign must be +1 or -1";
  if (backlash > 8192) return "backlash out of range (0-8192 steps)";
  if (torque >= 0 && (torque < 100 || torque > 1000)) return "torque limit out of range (100-1000)";
  JointTracker& j = g_j[idx];
  j.ratio = ratio;
  j.sign  = (int8_t)sign;
  if (backlash >= 0 && backlash != j.bl) {
    j.bl = backlash;
    j.blGoal = j.blOff;   // no half-taken-up slack from the old value left to apply
  }
  if (torque >= 0 && torque != j.tq) {
    j.tq = (uint16_t)torque;
    if (j.powered) g_bus.writeWord(j.id, REG_TORQUE_LIMIT, j.tq);   // takes effect now, not at the next energise
  }
  saveJointConfig();
  recomputeEnvelope();
  return "";
}

// ============================================================
//  JSON status for the web UI
// ============================================================
static String jsonEsc(const String& s) {
  String o; o.reserve(s.length() + 4);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 32) o += ' ';
    else o += c;
  }
  return o;
}

String demoJSON();   // demo sequence section below

String teachStatusJSON() {
  LiveReadings r = readAllPositions();
  String s; s.reserve(2200);
  char b[160];
  snprintf(b, sizeof(b), "{\"teach_active\":%s,\"calibrated\":%s,\"armed\":%s,\"fault\":%s,",
           g_teachActive ? "true" : "false", g_taught.calibrated ? "true" : "false",
           g_armed ? "true" : "false", g_faultActive ? "true" : "false");
  s += b;
  s += "\"fault_reason\":\"" + jsonEsc(g_faultReason) + "\",";
  s += "\"move_msg\":\"" + jsonEsc(g_moveMsg) + "\",";
  s += "\"sig_warn\":\"" + jsonEsc(g_sigWarn) + "\",";
  snprintf(b, sizeof(b), "\"scale\":%.2f,\"revbit\":%u,\"busy\":%s,\"grip_open\":%d,\"grip_close\":%d,", g_safetyScale,
           (unsigned)g_wheelRevBit, allJointsIdle() ? "false" : "true", (int)g_gripOpen, (int)g_gripClose);
  s += b;
  snprintf(b, sizeof(b), "\"live\":{\"base_deg\":%.1f,\"sh_deg\":%.1f,\"el_deg\":%.1f,\"fa_deg\":%.1f},",
           r.base_deg, r.sh_deg, r.el_deg, r.fa_deg);
  s += b;
  s += "\"saved\":{";
  for (int i = 0; i < P_COUNT; i++) {
    s += "\""; s += SLOT_NAMES[i]; s += "\":";
    s += (g_taught.savedFlags & (1u << i)) ? "true" : "false";
    if (i < P_COUNT - 1) s += ",";
  }
  s += "},\"joints\":[";
  for (int i = 0; i < N_JOINTS; i++) {
    snprintf(b, sizeof(b), "{\"name\":\"%s\",\"ratio\":%.3f,\"sign\":%d,\"backlash\":%ld,\"torque\":%u,\"ok\":%s,\"powered\":%s}%s",
             g_j[i].name, g_j[i].ratio, g_j[i].sign, (long)g_j[i].bl, (unsigned)g_j[i].tq, g_j[i].ok ? "true" : "false",
             g_j[i].powered ? "true" : "false", i < N_JOINTS - 1 ? "," : "");
    s += b;
  }
  s += "],\"demo\":" + demoJSON() + "}";
  return s;
}

// ============================================================
//  Demo sequence — a grab scripted from jogged poses, played back in order.
//
//  The fallback for a demo without CV (or without the base): jog to each
//  pose, pick what the gripper does once there (keep / open / close until
//  contact) and how long to pause, Add step. Play drives through the steps
//  at the safety-scaled DEFAULT_SPEED. Non-blocking: demoTick() from loop().
//
//  Steps are stored like taught poses (steps from the zero reference, NVS
//  namespace "arachnae_d"), so ratio/sign edits keep them and a re-zero
//  wipes them. Stop, E-STOP, a fault, teach or auto mode end playback; the
//  arm holds where it is.
// ============================================================
#define DEMO_PREFS_NAMESPACE "arachnae_d"
#define DEMO_MAX_STEPS       16
#define DEMO_CONTACT_LOAD    200    // gripper load that counts as "closed on the object"

enum DemoGrip : int8_t { DEMO_GRIP_KEEP = 0, DEMO_GRIP_OPEN = 1, DEMO_GRIP_CLOSE = 2 };
static const char* const DEMO_GRIP_NAMES[] = { "keep", "open", "close" };

struct DemoStep { int32_t eff[N_JOINTS]; int8_t grip; uint16_t pauseMs; };
struct DemoSeq  { DemoStep step[DEMO_MAX_STEPS]; uint8_t n; };
static DemoSeq g_demo;

enum class DemoPhase : uint8_t { IDLE, MOVE, GRIP, PAUSE };
static const char* const DEMO_PHASE_NAMES[] = { "idle", "move", "grip", "pause" };
struct DemoRun { DemoPhase phase = DemoPhase::IDLE; uint8_t idx = 0; uint32_t t0 = 0; String msg = ""; };
static DemoRun g_demoRun;

inline bool demoRunning() { return g_demoRun.phase != DemoPhase::IDLE; }

static void persistDemo() {
  g_prefs.begin(DEMO_PREFS_NAMESPACE, false);
  g_prefs.clear();
  g_prefs.putUChar("n", g_demo.n);
  for (int i = 0; i < g_demo.n; i++) {
    char k[8]; snprintf(k, sizeof(k), "s%d", i);
    g_prefs.putBytes(k, &g_demo.step[i], sizeof(DemoStep));
  }
  g_prefs.end();
}

void loadDemo() {
  memset(&g_demo, 0, sizeof(g_demo));
  g_prefs.begin(DEMO_PREFS_NAMESPACE, true);
  uint8_t n = min((int)g_prefs.getUChar("n", 0), DEMO_MAX_STEPS);
  for (int i = 0; i < n; i++) {
    char k[8]; snprintf(k, sizeof(k), "s%d", i);
    if (g_prefs.getBytes(k, &g_demo.step[i], sizeof(DemoStep)) != sizeof(DemoStep)) break;   // keep the good prefix
    g_demo.n = i + 1;
  }
  g_prefs.end();
}

void clearDemo() {
  memset(&g_demo, 0, sizeof(g_demo));
  persistDemo();
}

// Returns "" on success, else why not.
String demoAddStep(int grip, int pauseMs) {
  if (demoRunning())                 return "the demo is playing";
  if (!g_armed)                      return "arm first (Arm button)";
  if (g_demo.n >= DEMO_MAX_STEPS)    return "demo is full (" + String(DEMO_MAX_STEPS) + " steps)";
  if (grip < DEMO_GRIP_KEEP || grip > DEMO_GRIP_CLOSE) return "grip must be 0 keep, 1 open or 2 close";
  if (pauseMs < 0 || pauseMs > 10000) return "pause must be 0-10000 ms";
  if (grip != DEMO_GRIP_KEEP && !gripperCalibrated()) return "set the gripper's open / closed positions first";
  updateTrackers();
  for (auto& j : g_j) if (!j.ok) return String(j.name) + " not tracked";
  DemoStep& s = g_demo.step[g_demo.n];
  for (int i = 0; i < N_JOINTS; i++) s.eff[i] = jEff(g_j[i]);
  s.grip = (int8_t)grip;
  s.pauseMs = (uint16_t)pauseMs;
  g_demo.n++;
  persistDemo();
  return "";
}

String demoRemoveLast() {
  if (demoRunning()) return "the demo is playing";
  if (g_demo.n == 0) return "no steps";
  g_demo.n--;
  persistDemo();
  return "";
}

void demoStop(const String& why) {
  if (!demoRunning()) return;
  g_demoRun.phase = DemoPhase::IDLE;
  if (!g_faultActive) abortAllMotion();   // hold where it is
  g_demoRun.msg = why;
  wsLog("[Demo] " + why);
}

static void demoStartStep() {
  const DemoStep& s = g_demo.step[g_demoRun.idx];
  JointAngles a = { (float)radians(effToDeg(g_j[0], s.eff[0])), (float)radians(effToDeg(g_j[1], s.eff[1])),
                    (float)radians(effToDeg(g_j[2], s.eff[2])), (float)radians(effToDeg(g_j[3], s.eff[3])) };
  if (!armMoveToAngles(a, DEFAULT_SPEED)) { demoStop("step " + String(g_demoRun.idx + 1) + ": move refused"); return; }
  g_demoRun.phase = DemoPhase::MOVE;
  wsLog("[Demo] step " + String(g_demoRun.idx + 1) + "/" + String(g_demo.n));
}

String demoPlay() {
  if (demoRunning())        return "already playing";
  if (!motionAllowed())     return "not armed, fault active, or teach mode on";
  if (g_autoMode)           return "auto mode is on";
  if (g_baseSpin.active)    return "base spin test is running";
  if (!allJointsIdle())     return "arm is still moving";
  if (g_demo.n == 0)        return "no steps recorded";
  for (int i = 0; i < g_demo.n; i++)
    if (g_demo.step[i].grip != DEMO_GRIP_KEEP && !gripperCalibrated()) return "set the gripper's open / closed positions first";
  g_demoRun.idx = 0;
  g_demoRun.msg = "";
  demoStartStep();
  return demoRunning() ? "" : g_demoRun.msg;
}

void demoTick() {
  if (!demoRunning()) return;
  if (g_faultActive)              { demoStop("stopped: fault - " + g_faultReason); return; }
  if (g_teachActive || g_autoMode) { demoStop("stopped: teach or auto mode"); return; }
  const DemoStep& s = g_demo.step[g_demoRun.idx];
  uint32_t now = millis();

  switch (g_demoRun.phase) {
    case DemoPhase::MOVE:
      if (!allJointsIdle()) return;
      if (!g_lastMoveOk) { demoStop("step " + String(g_demoRun.idx + 1) + ": " + g_moveMsg); return; }
      g_demoRun.t0 = now;
      if (s.grip == DEMO_GRIP_OPEN) {
        gripperOpen();
        g_demoRun.phase = DemoPhase::GRIP;
      } else if (s.grip == DEMO_GRIP_CLOSE) {
        busFor(SERVO_GRIPPER).writeWord(SERVO_GRIPPER, REG_TORQUE_LIMIT, GRIP_TORQUE);
        setGripperSteps(g_gripClose, 300);
        g_demoRun.phase = DemoPhase::GRIP;
      } else {
        g_demoRun.phase = DemoPhase::PAUSE;
      }
      return;

    case DemoPhase::GRIP:
      if (s.grip == DEMO_GRIP_CLOSE) {
        // Same contact logic as gripperCloseToContact(), without blocking.
        int16_t load = gripperLoad();
        if (load > DEMO_CONTACT_LOAD) {
          int32_t pos = gripperPos();
          if (pos >= 0) setGripperSteps((int16_t)pos, 300);   // hold at contact, don't keep squeezing toward closed
          wsLog("[Demo] gripper contact at load " + String(load));
        } else if (now - g_demoRun.t0 < 3000) {
          return;
        } else {
          wsLog(F("[Demo] gripper closed with no contact"));
        }
      } else if (now - g_demoRun.t0 < 800) {
        return;   // let the jaws open before moving on
      }
      g_demoRun.t0 = now;
      g_demoRun.phase = DemoPhase::PAUSE;
      return;

    case DemoPhase::PAUSE:
      if (now - g_demoRun.t0 < s.pauseMs) return;
      if (++g_demoRun.idx >= g_demo.n) {
        g_demoRun.phase = DemoPhase::IDLE;
        g_demoRun.msg = "done";
        wsLog(F("[Demo] done"));
        return;
      }
      demoStartStep();
      return;

    default:
      return;
  }
}

String demoJSON() {
  String s; s.reserve(160 + g_demo.n * 48);
  char b[120];
  snprintf(b, sizeof(b), "{\"n\":%d,\"max\":%d,\"running\":%s,\"step\":%d,\"phase\":\"%s\",\"msg\":\"",
           g_demo.n, DEMO_MAX_STEPS, demoRunning() ? "true" : "false", (int)g_demoRun.idx,
           DEMO_PHASE_NAMES[(int)g_demoRun.phase]);
  s += b; s += jsonEsc(g_demoRun.msg); s += "\",\"steps\":[";
  for (int i = 0; i < g_demo.n; i++) {
    const DemoStep& d = g_demo.step[i];
    snprintf(b, sizeof(b), "%s[%d,%u,%.1f,%.1f,%.1f,%.1f]", i ? "," : "", (int)d.grip, (unsigned)d.pauseMs,
             effToDeg(g_j[0], d.eff[0]), effToDeg(g_j[1], d.eff[1]), effToDeg(g_j[2], d.eff[2]), effToDeg(g_j[3], d.eff[3]));
    s += b;
  }
  s += "]}";
  return s;
}

// ============================================================
//  Factory reset of TAUGHT data (joint ratios/signs are kept)
// ============================================================
// ============================================================
//  setZeroHere — redefine the CURRENT pose as the home/boot reference.
//
//  The original bring-up flow established the zero by powering the arm up
//  while it sat in a hand-placed pose. That is not usable here: the arm must
//  never be backdriven by hand (it risks the servos' internal boards), so the
//  reference has to be settable in software from whatever pose the arm has
//  been jogged into.
//
//  After this, every joint reads exactly its HOME_*_DEG angle and all step
//  counters are measured from here. Any previously taught pose was stored as
//  steps from the OLD reference and is meaningless afterwards, so this
//  refuses to run while poses are saved unless `force` is set — in which case
//  it wipes them.
// ============================================================
String setZeroHere(bool force) {
  if (g_faultActive) return "fault active";
  updateTrackers();
  for (auto& j : g_j) if (!j.ok) return String(j.name) + " servo not responding";

  if (demoRunning()) return "the demo is playing";
  if ((g_taught.savedFlags || g_demo.n) && !force)
    return "SAVED:re-zeroing invalidates the taught positions and demo steps already stored";

  if (g_taught.savedFlags) {
    g_prefs.begin(PREFS_NAMESPACE, false);
    g_prefs.clear();
    g_prefs.end();
    memset(&g_taught, 0, sizeof(g_taught));
  }
  if (g_demo.n) clearDemo();

  int16_t sig[N_JOINTS];
  for (int i = 0; i < N_JOINTS; i++) {
    JointTracker& j = g_j[i];
    j.zeroAbs = j.absSteps;   // current pose becomes eff = 0, i.e. HOME_*_DEG
    j.blOff   = 0;
    j.blGoal  = 0;
    j.dir     = 1;            // treat the slack as taken up on the "+" side
    j.mv      = JMove::HOLD;
    j.bootRaw = j.lastRaw;    // signature now matches this physical pose
    sig[i]    = j.bootRaw;
  }

  // Persist the signature so the next boot in this same pose arms cleanly.
  g_prefs.begin(PREFS_NAMESPACE, false);
  g_prefs.putBytes("bootsig", sig, sizeof(sig));
  g_prefs.end();
  memcpy(g_sigSaved, sig, sizeof(sig));
  g_sigHave = true;
  g_sigWarn = "";

  recomputeEnvelope();
  wsLog(F("[Teach] Current pose set as ZERO — all joints now read their HOME angles"));
  return "";
}

void clearAllTaught() {
  g_prefs.begin(PREFS_NAMESPACE, false);
  g_prefs.clear();
  g_prefs.end();
  memset(&g_taught, 0, sizeof(g_taught));
  g_sigHave = false; g_sigWarn = "";
  recomputeEnvelope();
  Serial.println(F("[Teach] All taught positions cleared"));
}

