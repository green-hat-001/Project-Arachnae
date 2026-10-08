#pragma once
// ============================================================
//  PROJET ARACHNAE v2 — Servo Control
//  ST3215 x 5 via Waveshare Serial Bus Servo Driver
//  SCSerial half-duplex UART, 1 Mbaud. Joints on Serial1 (PIN_SERVO_RX/TX);
//  gripper on its own junction on Serial2 (PIN_GRIP_RX/TX) when
//  GRIPPER_OWN_BUS is set — one resistor junction can't drive all five.
//
//  ARCHITECTURE (see docs/FIRMWARE.md "Servo bus" for the full spec)
//  ─────────────────────────────────────────────────────────
//  There is NO usable Mode 3 on this servo. Every arm joint
//  (base/shoulder/elbow/forearm) runs Mode 0 (single-revolution
//  position control) most of the time, with a SOFTWARE revolution
//  tracker (updateTrackers()) turning the raw 0–4095 encoder into
//  a continuous multi-turn "absSteps" count. A move that needs to
//  travel more than about half a revolution first "leapfrogs"
//  through Mode 1 (continuous wheel rotation) to close most of the
//  distance, then drops back to Mode 0 for a precise final approach
//  (JMove::WHEEL -> JMove::SETTLE -> JMove::HOLD).
//
//  Everything here is NON-BLOCKING: armMoveToAngles()/jogJoint()
//  only compute a target and kick off the state machine; jointsTick()
//  (pumped from tickServers() and loop(), see docs/FIRMWARE.md) reads
//  encoders every JOINT_POLL_MS and advances each joint's state.
//  waitForMotionComplete() only pumps ticks — it never itself talks
//  to a servo beyond what jointsTick() does.
//
//  BACKLASH (per joint, JointTracker::bl, runtime-editable): reversing
//  a joint's direction of travel consumes bl servo steps of gear slack
//  before the output shaft actually starts moving (the old 1-start base
//  worm: a whole servo rev). blOff tracks how many raw steps
//  have been "wasted" on slack so far and is subtracted back out
//  when converting absSteps -> effective/output-shaft steps (jEff/
//  jDeg), so a taught position and the live angle readout both stay
//  correct across any number of direction reversals.
//
//  BUG FIXES carried over from the field-tested build:
//   [FIX 1] readDWord: response is 10 bytes not 9.
//   [FIX 2] readWord: response is 8 bytes not 7.
//   [FIX 3] gripperCloseToContact uses readWord/writeWord — the
//           gripper is Mode 0 (16-bit), not a DWORD register.
//   [FIX 4] REG_PRESENT_LOAD is 0x3C, not 0x3E (that's temperature).
// ============================================================

#include <Arduino.h>
#include <HardwareSerial.h>
#include <Preferences.h>
#include "config.h"
#include "kinematics.h"

// ── Register map ─────────────────────────────────────────────
#define REG_OPERATING_MODE  0x21
#define REG_TORQUE_LIMIT    0x22
#define REG_TORQUE_ENABLE   0x28
#define REG_GOAL_ACCEL      0x29   // acceleration (0=hw max, 1-254 steps/ms²)
#define REG_GOAL_POSITION   0x2A
#define REG_GOAL_SPEED      0x2E
#define REG_PRESENT_POS     0x38
#define REG_PRESENT_SPEED   0x3A
#define REG_PRESENT_LOAD     0x3C   // load magnitude, bits[9:0]
#define REG_PRESENT_VOLTAGE  0x3E   // byte, x0.1 V
#define REG_PRESENT_TEMP     0x3F   // byte, deg C

// ── GrabFSM and ArmState (used by the main sketch's FSM) ──────
enum class GrabFSM : uint8_t {
  IDLE, SCANNING, APPROACH, DESCEND, GRIP, LIFT, HANDOFF, HOME
};

struct ArmState {
  JointAngles current;
  bool torqueEnabled = false;
  GrabFSM fsm = GrabFSM::IDLE;
};

// ============================================================
//  SAFETY GLOBALS — extern-visible throughout the sketch.
// ============================================================
float  g_safetyScale = SAFETY_SCALE_DEFAULT;
bool   g_faultActive  = false;
String g_faultReason  = "";
bool   g_armed        = false;   // no powered motion until the user confirms boot pose
bool   g_teachActive  = false;   // torque released for hand-teaching

// Result of the most recently *finished* commanded move (armMoveToAngles /
// jogJoint / moveToTaught). A wrong-direction or stalled leapfrog leg sets
// this false and fills g_moveMsg WITHOUT latching a fault — the joint just
// holds where it is and stays armed.
String g_moveMsg    = "";
bool   g_lastMoveOk  = true;

// STS-series: bit 15 of GOAL_SPEED = reverse. Some firmware uses bit 10
// (0x0400) instead. Runtime-editable from the teach UI, stored in NVS.
uint16_t g_wheelRevBit = WHEEL_REVERSE_BIT;

// Gripper open / closed positions, in raw servo steps (0-4095). Set by
// jogging the gripper there and pressing "Set open"/"Set closed" in /teach;
// stored in NVS with the joint calibration. -1 = not set. Until both are set,
// open/close commands refuse — the old code drove to a fixed 2048..3072
// window that had never been measured against the real jaws.
int16_t g_gripOpen  = -1;
int16_t g_gripClose = -1;
inline bool gripperCalibrated() { return g_gripOpen >= 0 && g_gripClose >= 0 && g_gripOpen != g_gripClose; }

// Base spin test (/test page): the base run as a free motor, on until stopped.
// See baseSpinStart().
struct BaseSpin {
  bool     active   = false;   // a spin is running (or still stopping)
  bool     stopping = false;   // stop was pressed, the joint is handing off to hold
  int8_t   dir      = 0;       // +1 = the base's + jog direction, -1 = its - direction
  uint8_t  pct      = 0;       // 10-100 % of the servo's wheel-mode ceiling
  uint16_t speed    = 0;       // servo steps/s actually commanded
  int32_t  startAbs = 0;       // absSteps when the spin started (servo turns counter)
  int32_t  startEff = 0;       // jEff() when the spin started (re-reference on the way out)
  int32_t  steps    = 0;       // servo steps turned by the current / last spin, signed
};
BaseSpin g_baseSpin;

// Base torque cap step-down (see BASE_CAP_* in config.h and baseCapTick()).
// Re-armed by every new base move and every spin start / speed change.
struct BaseCap {
  bool     over       = false;   // load is at the cap, since overSince
  uint32_t overSince  = 0;
  uint32_t graceUntil = 0;       // no judging before this (accelerating)
  uint16_t load       = 0;       // last load read while moving
};
BaseCap g_baseCap;
static void baseCapRearm() {
  g_baseCap.over = false;
  g_baseCap.graceUntil = millis() + BASE_CAP_GRACE_MS;
}

// ── safeSpeed / safeAccel ────────────────────────────────────
inline uint16_t safeSpeed(uint16_t raw) {
  return (uint16_t)max(1, (int)(raw * g_safetyScale));
}
inline uint8_t safeAccel() {
  return (uint8_t)max(1, (int)(SAFETY_ACCEL_MAX * g_safetyScale));
}

// ============================================================
//  SCSerial Bus Driver
//  Packet format:
//    Write:  FF FF  ID  LEN  0x03  REG  DATA...  CHKSUM
//    Read:   FF FF  ID  4    0x02  REG  N        CHKSUM
//    Resp:   FF FF  ID  LEN  ERR   DATA...        CHKSUM
//      Response total bytes = 6 + N  (1-byte=7, 2-byte=8, 4-byte=10)
// ============================================================
class SCSerialBus {
public:
  HardwareSerial* _serial;
  int8_t _dirPin;

  void begin(HardwareSerial* s, int8_t dirPin = -1) {
    _serial = s;
    _dirPin = dirPin;
    if (_dirPin >= 0) { pinMode(_dirPin, OUTPUT); digitalWrite(_dirPin, LOW); }
  }

  void writeDWord(uint8_t id, uint8_t reg, int32_t val) {
    uint8_t b0=val&0xFF, b1=(val>>8)&0xFF, b2=(val>>16)&0xFF, b3=(val>>24)&0xFF;
    uint8_t pkt[11];
    pkt[0]=0xFF; pkt[1]=0xFF; pkt[2]=id;
    pkt[3]=7; pkt[4]=0x03; pkt[5]=reg;
    pkt[6]=b0; pkt[7]=b1; pkt[8]=b2; pkt[9]=b3;
    pkt[10]=~(id+7+0x03+reg+b0+b1+b2+b3)&0xFF;
    _send(pkt, 11);
    _afterWrite();
  }

  void writeWord(uint8_t id, uint8_t reg, uint16_t val) {
    uint8_t lo=val&0xFF, hi=(val>>8)&0xFF;
    uint8_t pkt[9];
    pkt[0]=0xFF; pkt[1]=0xFF; pkt[2]=id;
    pkt[3]=5; pkt[4]=0x03; pkt[5]=reg;
    pkt[6]=lo; pkt[7]=hi;
    pkt[8]=~(id+5+0x03+reg+lo+hi)&0xFF;
    _send(pkt, 9);
    _afterWrite();
  }

  void writeByte(uint8_t id, uint8_t reg, uint8_t val) {
    uint8_t pkt[8];
    pkt[0]=0xFF; pkt[1]=0xFF; pkt[2]=id;
    pkt[3]=4; pkt[4]=0x03; pkt[5]=reg; pkt[6]=val;
    pkt[7]=~(id+4+0x03+reg+val)&0xFF;
    _send(pkt, 8);
    _afterWrite();
  }

  // [FIX 1] DWord response: 10 bytes
  int32_t readDWord(uint8_t id, uint8_t reg) {
    uint8_t resp[10];
    if (!_read(id, reg, 4, resp)) return INT32_MIN;
    return (int32_t)(resp[5]|(resp[6]<<8)|(resp[7]<<16)|(resp[8]<<24));
  }

  // [FIX 2] Word response: 8 bytes
  int16_t readWord(uint8_t id, uint8_t reg) {
    uint8_t resp[8];
    if (!_read(id, reg, 2, resp)) return -1;
    return (int16_t)(resp[5]|(resp[6]<<8));
  }

  int16_t readByte(uint8_t id, uint8_t reg) {
    uint8_t resp[7];
    if (!_read(id, reg, 1, resp)) return -1;
    return (int16_t)resp[5];
  }

private:
  // Some wiring of this half-duplex bus loops transmitted bytes back into
  // our own RX path ("echo"); some doesn't. Either way, a fire-and-forget
  // write() still gets a status reply from the servo that nothing ever
  // consumes. _send() drains any such leftovers before every transaction,
  // and _read() explicitly recognizes + skips a leading echo of the
  // request it just sent, so both wiring styles parse correctly.
  bool _read(uint8_t id, uint8_t reg, uint8_t nb, uint8_t* out) {
    uint8_t req[8];
    req[0]=0xFF; req[1]=0xFF; req[2]=id; req[3]=4;
    req[4]=0x02; req[5]=reg; req[6]=nb;
    req[7]=~(id+4+0x02+reg+nb)&0xFF;
    _send(req, 8);

    uint8_t buf[24]; int n = 0;
    int wantTotal = 6 + nb;
    uint32_t t0 = millis();
    while (millis() - t0 < 8) {
      while (_serial->available() > 0 && n < (int)sizeof(buf)) buf[n++] = (uint8_t)_serial->read();
      int off = (n >= 8 && memcmp(buf, req, 8) == 0) ? 8 : 0;
      if (n - off >= wantTotal && buf[off] == 0xFF && buf[off+1] == 0xFF) {
        // A header alone proves nothing on a marginal bus: a corrupted or
        // foreign frame was accepted as data (first full-bus test read the
        // gripper's torque limit as -14316, 94 times). A bad position read
        // would be folded into the revolution tracker as real motion, so
        // the frame must be from this servo, the right length, and checksum-clean.
        const uint8_t* f = buf + off;
        uint8_t sum = 0;
        for (int i = 2; i < wantTotal - 1; i++) sum += f[i];
        if (f[2] != id || f[3] != nb + 2 || (uint8_t)~sum != f[wantTotal - 1]) return false;
        memcpy(out, f, wantTotal);
        return true;
      }
      if (_serial->available() == 0) delayMicroseconds(50);
    }
    return false;
  }

  // Called after every WRITE (never from the read path, which consumes the
  // reply itself).
  //
  // A write is fire-and-forget from our side, but the servo still answers it
  // with a status packet, and on this resistor-spliced bus our own echo comes
  // back too. Nothing consumed either, and _send() only drains BEFORE it
  // transmits — so that reply was still arriving when the next packet went
  // out, collided with it on the shared wire, and destroyed it. The symptom
  // was that the second of any two back-to-back writes silently vanished:
  // GOAL_SPEED landed and the GOAL_POSITION right behind it did not, so the
  // servo sat holding its old goal with torque on, drawing no load, making no
  // sound. Waiting out the reply and discarding it before returning keeps the
  // line clear for the next packet.
  void _afterWrite() {
    delayMicroseconds(500);                              // ~50 byte-times at 1 Mbaud
    while (_serial->available() > 0) _serial->read();
  }

  void _send(uint8_t* d, size_t len) {
    while (_serial->available() > 0) _serial->read();   // drop any unconsumed reply from a prior write
    if (_dirPin>=0) digitalWrite(_dirPin, HIGH);
    _serial->write(d, len);
    _serial->flush();
    if (_dirPin>=0) { delayMicroseconds(20); digitalWrite(_dirPin, LOW); }
  }
};

static SCSerialBus g_bus;
// Second junction, gripper only (see GRIPPER_OWN_BUS in config.h). Every
// access to a servo by ID goes through busFor() so the gripper's packets
// never touch the joints' wire and vice versa.
static SCSerialBus g_gripBus;
static inline SCSerialBus& busFor(uint8_t id) {
#if GRIPPER_OWN_BUS
  if (id == SERVO_GRIPPER) return g_gripBus;
#endif
  (void)id;
  return g_bus;
}

// ============================================================
//  Verified byte write.
//
//  ST3215 writes are fire-and-forget: the servo sends no ACK, so a packet
//  mangled by a collision on this half-duplex resistor-hack bus is dropped
//  in total silence. Measured on the real arm (/teach/diag): after
//  servoInit(), REG_TORQUE_LIMIT (word) read back 700 on all four joints
//  while REG_TORQUE_ENABLE (byte) had landed on ID 1 only — leaving the
//  firmware convinced all four were powered while three were limp. Their
//  gearboxes are non-backdrivable, so they still felt rigid by hand.
//
//  Every write that changes whether or when the arm can move (torque
//  enable, operating mode) therefore goes through here: write, read back,
//  retry. Position/speed goals are NOT verified this way — they are
//  re-issued every tick anyway, and 0x2A does not read back reliably.
// ============================================================
static bool writeByteVerified(uint8_t id, uint8_t reg, uint8_t val, uint8_t tries = 4) {
  for (uint8_t t = 0; t < tries; t++) {
    busFor(id).writeByte(id, reg, val);
    delay(6);
    int16_t rb = busFor(id).readByte(id, reg);
    if (rb == (int16_t)val) return true;
    delay(4);
  }
  return false;
}

// ============================================================
//  Joint tracking
// ============================================================
enum class JMove : uint8_t { HOLD, WHEEL, SETTLE };

struct JointTracker {
  JointTracker(const char* n, uint8_t i, float r, int8_t s, float rd, int32_t b, uint16_t t)
    : name(n), id(i), ratio(r), sign(s), refDeg(rd), bl(b), tq(t) {}

  const char* name;
  uint8_t  id;
  float    ratio;
  int8_t   sign;
  float    refDeg;        // output-shaft angle at boot pose (HOME_*_DEG)
  int32_t  bl;            // backlash, servo steps of slack on a reversal (runtime, NVS)
  uint16_t tq;            // REG_TORQUE_LIMIT written on every energise (runtime, NVS)
  int32_t  zeroAbs = 0;    // absSteps value at boot (the reference)
  int32_t  blOff   = 0;    // backlash slack "eaten" so far (base only)
  int32_t  blGoal  = 0;    // where blOff ends up once the slack of the last reversal is taken up
  int32_t  absSteps = 0;   // software-tracked multi-turn absolute steps
  int16_t  lastRaw  = -1;  // last raw (0-4095) encoder reading
  int16_t  bootRaw  = -1;  // raw reading captured once at boot
  bool     ok       = false;
  bool     powered  = false;
  JMove    mv = JMove::HOLD;

  // motion planning / watchdog state
  int32_t  targetAbs = 0;
  int8_t   dir        = 1;   // last commanded direction (for backlash + wrong-way detection)
  uint16_t cmdSpeed    = 0;
  int32_t  lastDelta   = 0;
  int32_t  wrongWayAccum = 0;
  uint32_t lastProgressMs = 0;
  uint32_t lastGoodReadMs = 0;
  uint16_t missedReads    = 0;   // consecutive FAILED read attempts — see WHEEL_MISSED_READS_MAX
  uint8_t  replans        = 0;   // wrap recoveries this move — see MAX_LEAPFROG_REPLANS
};

#define N_JOINTS 4   // base, shoulder, elbow, forearm — gripper is tracked separately
// Consumed by teach_mode.h's teachModeStart() to cut torque for a hand-teach
// session — includes the gripper too, so it can also be posed/moved by hand.
static const uint8_t ARM_IDS[N_JOINTS + 1] = { SERVO_BASE, SERVO_SHOULDER, SERVO_ELBOW, SERVO_FOREARM, SERVO_GRIPPER };

static JointTracker g_j[N_JOINTS] = {
  { "Base",     SERVO_BASE,     RATIO_BASE,     SIGN_BASE,     HOME_BASE_DEG,     BACKLASH_BASE_STEPS,     BASE_TORQUE    },
  { "Shoulder", SERVO_SHOULDER, RATIO_SHOULDER, SIGN_SHOULDER, HOME_SHOULDER_DEG, BACKLASH_SHOULDER_STEPS, DEFAULT_TORQUE },
  { "Elbow",    SERVO_ELBOW,    RATIO_ELBOW,    SIGN_ELBOW,    HOME_ELBOW_DEG,    BACKLASH_ELBOW_STEPS,    DEFAULT_TORQUE },
  { "Forearm",  SERVO_FOREARM,  RATIO_FOREARM,  SIGN_FOREARM,  HOME_FOREARM_DEG,  BACKLASH_FOREARM_STEPS,  DEFAULT_TORQUE },
};

static constexpr float JOINT_MIN_DEG[N_JOINTS] = { BASE_MIN_DEG, SHOULDER_MIN_DEG, ELBOW_MIN_DEG, FOREARM_MIN_DEG };
static constexpr float JOINT_MAX_DEG[N_JOINTS] = { BASE_MAX_DEG, SHOULDER_MAX_DEG, ELBOW_MAX_DEG, FOREARM_MAX_DEG };

// Non-fault abort thresholds for a leapfrog leg gone wrong.
#define WRONG_WAY_STEPS   200
#define NO_MOTION_MS      1000
// The base stalls into printed gear teeth, so it gives up sooner.
static inline uint32_t noMotionMs(int idx) { return idx == 0 ? BASE_NO_MOTION_MS : NO_MOTION_MS; }

// Comms-loss watchdog threshold, expressed as consecutive FAILED read
// attempts rather than elapsed wall-clock time. A blocking safety operation
// for a DIFFERENT joint (jointSetMode()'s verified writes, an E-STOP's
// per-servo retries) can legitimately run past WHEEL_COMMS_TIMEOUT_MS
// without this joint's tracker ever attempting a read in the meantime —
// that must not look like lost comms. Same time budget as before
// (WHEEL_COMMS_TIMEOUT_MS at JOINT_POLL_MS cadence), just counted in
// reads that were actually tried and failed.
static constexpr int WHEEL_MISSED_READS_MAX = WHEEL_COMMS_TIMEOUT_MS / JOINT_POLL_MS;

// ── Angle <-> steps ───────────────────────────────────────────
// eff = steps from the boot reference, backlash removed — this is what
// gets stored in a taught position (see teach_mode.h) and is independent
// of ratio/sign, so recalibrating either does not invalidate what was taught.
inline int32_t jEff(const JointTracker& j) { return (j.absSteps - j.zeroAbs) - j.blOff; }

inline float effToDeg(const JointTracker& j, int32_t eff) {
  return j.refDeg + j.sign * (float)eff * 360.0f / (j.ratio * (float)SERVO_STEPS_PER_REV);
}
inline float jDeg(const JointTracker& j) { return effToDeg(j, jEff(j)); }
inline float jointAngleRad(int i) { return radians(jDeg(g_j[i])); }
inline JointAngles currentAngles() {
  return { jointAngleRad(0), jointAngleRad(1), jointAngleRad(2), jointAngleRad(3) };
}

inline bool allJointsIdle() {
  for (int i = 0; i < N_JOINTS; i++) if (g_j[i].mv != JMove::HOLD) return false;
  return true;
}
inline bool motionAllowed() { return g_armed && !g_faultActive && !g_teachActive; }

// ============================================================
//  Runtime joint calibration — NVS namespace "arachnae_j"
// ============================================================
#define JOINT_PREFS_NAMESPACE "arachnae_j"

void saveJointConfig() {
  Preferences p;
  p.begin(JOINT_PREFS_NAMESPACE, false);
  for (int i = 0; i < N_JOINTS; i++) {
    char kr[8]; snprintf(kr, sizeof(kr), "r%d", i);
    char ks[8]; snprintf(ks, sizeof(ks), "s%d", i);
    char kb[8]; snprintf(kb, sizeof(kb), "b%d", i);
    char kt[8]; snprintf(kt, sizeof(kt), "t%d", i);
    p.putFloat(kr, g_j[i].ratio);
    p.putChar(ks, g_j[i].sign);
    p.putInt(kb, g_j[i].bl);
    p.putUShort(kt, g_j[i].tq);
  }
  p.putUShort("revbit", g_wheelRevBit);
  p.putShort("gOpen",  g_gripOpen);
  p.putShort("gClose", g_gripClose);
  p.end();
}

static void loadJointConfig() {
  Preferences p;
  p.begin(JOINT_PREFS_NAMESPACE, true);
  for (int i = 0; i < N_JOINTS; i++) {
    char kr[8]; snprintf(kr, sizeof(kr), "r%d", i);
    char ks[8]; snprintf(ks, sizeof(ks), "s%d", i);
    char kb[8]; snprintf(kb, sizeof(kb), "b%d", i);
    char kt[8]; snprintf(kt, sizeof(kt), "t%d", i);
    g_j[i].ratio = p.getFloat(kr, g_j[i].ratio);
    g_j[i].sign  = p.getChar(ks, g_j[i].sign);
    g_j[i].bl    = p.getInt(kb, g_j[i].bl);
    g_j[i].tq    = p.getUShort(kt, g_j[i].tq);
  }
  g_wheelRevBit = p.getUShort("revbit", WHEEL_REVERSE_BIT);
  g_gripOpen  = p.getShort("gOpen",  -1);
  g_gripClose = p.getShort("gClose", -1);
  p.end();
}

// ============================================================
//  jointSetMode — the ONLY place the mode register is written.
//  Mode register writes only happen with torque off; re-energising
//  always sets goal = the servo's own current reading first, so
//  torque-on never causes a snap toward a stale goal.
// ============================================================
void jointSetMode(JointTracker& j, uint8_t mode) {
  // Torque must be OFF for the mode register write to be accepted.
  writeByteVerified(j.id, REG_TORQUE_ENABLE, 0);
  j.powered = false;
  if (!writeByteVerified(j.id, REG_OPERATING_MODE, mode)) {
    wsLog(String("[Servo] ") + j.name + " mode write FAILED (wanted " + String(mode) + ")");
  }

  if (mode == 0) {
    // Torque is about to come back on, and the servo drives to whatever sits in
    // GOAL_POSITION the moment it does. If the present-position read fails, that
    // register still holds a PREVIOUS move's target and the joint lunges there
    // at DEFAULT_SPEED. Retry once, and refuse to energise if we still cannot
    // read where the joint actually is — a limp joint is recoverable, a
    // full-speed lunge on a loaded arm is not.
    int16_t raw = g_bus.readWord(j.id, REG_PRESENT_POS);
    if (raw < 0) raw = g_bus.readWord(j.id, REG_PRESENT_POS);
    if (raw < 0 && j.lastRaw >= 0) {
      // Fall back to the tracker's last good reading — at most JOINT_POLL_MS
      // old, and already a raw 0-4095 value. Holding there is wrong by at most
      // one tick of motion; energising on a stale goal is wrong by an entire
      // previous move, and dropping torque instead would let a loaded joint
      // fall, which is the backdriving we must never cause.
      raw = j.lastRaw;
      wsLog(String("[Servo] ") + j.name + " position unreadable on re-energise - "
            "holding last known encoder reading");
    }
    if (raw < 0) {
      wsLog(String("[Servo] ") + j.name + " position unknown - leaving torque OFF "
            "rather than energising against a stale goal");
      j.powered = false;
      return;
    }
    g_bus.writeWord(j.id, REG_GOAL_SPEED, safeSpeed(DEFAULT_SPEED));
    g_bus.writeWord(j.id, REG_GOAL_POSITION, (uint16_t)raw);
  } else {
    g_bus.writeWord(j.id, REG_GOAL_SPEED, 0);
  }
  // Restore the torque limit on EVERY re-energise. The ST3215 clamps this
  // register down when its overload protection trips, and it stays clamped
  // until something rewrites it — so a servo that has been stalled will
  // afterwards hold position but produce no usable torque to move with.
  // servo_jog (which works on this arm) rewrites it on every energise; so did
  // the pre-rewrite firmware. Omitting it here meant that once any joint
  // tripped overload, no amount of jogging or fault-resetting could recover it.
  g_bus.writeWord(j.id, REG_TORQUE_LIMIT, j.tq);
  delay(4);
  if (!writeByteVerified(j.id, REG_TORQUE_ENABLE, 1)) {
    wsLog(String("[Servo] ") + j.name + " TORQUE ENABLE FAILED - joint is limp");
    j.powered = false;
    return;
  }
  j.powered = true;
}

// ============================================================
//  updateTrackers — reads all 4 arm encoders and folds the raw
//  0-4095 reading into each joint's continuous absSteps count.
//  Runs unconditionally (even unpowered / faulted) so trackers
//  keep following the arm — required for hand-teaching and for
//  the boot-pose check after an E-STOP.
// ============================================================
static bool trackJoint(JointTracker& j, uint32_t now) {
  int16_t raw = g_bus.readWord(j.id, REG_PRESENT_POS);
  if (raw < 0) { j.lastDelta = 0; j.missedReads++; return false; }
  if (j.lastRaw < 0) { j.lastRaw = raw; j.lastGoodReadMs = now; j.missedReads = 0; j.ok = true; return true; }
  int32_t delta = (int32_t)raw - (int32_t)j.lastRaw;
  if (delta > WRAP_THRESH) delta -= SERVO_STEPS_PER_REV;
  else if (delta < -WRAP_THRESH) delta += SERVO_STEPS_PER_REV;
  j.absSteps += delta;
  // A reversal's slack is taken up as the servo actually turns through it, and
  // the output shaft stands still meanwhile: until blOff reaches blGoal it
  // follows the servo (either way — spring-back during a mode switch moves it
  // back) within the one-backlash band between the two sides, so jDeg() holds
  // steady instead of jumping a whole backlash (18 deg on the base) the moment
  // the reversal is commanded. That jump used to make the hard-limit watchdog
  // E-STOP any base reversal within ~18 deg of a limit.
  if (j.blOff != j.blGoal) {
    int32_t bl = j.bl;
    int32_t lo = (j.blGoal > j.blOff) ? j.blGoal - bl : j.blGoal;
    j.blOff = constrain(j.blOff + delta, lo, lo + bl);
  }
  j.lastDelta = delta;
  j.lastRaw = raw;
  j.lastGoodReadMs = now;
  j.missedReads = 0;
  j.ok = true;
  return true;
}

void updateTrackers() {
  uint32_t now = millis();
  for (int i = 0; i < N_JOINTS; i++) trackJoint(g_j[i], now);
}

// ============================================================
//  Move planning
//
//  The simulated (and real) Mode-0 target is "base + goalReg", where
//  base = floor(currentPos/4096)*4096 is recomputed from the servo's
//  OWN present position on every physics step — not fixed at command
//  time. So a goalReg written as "targetAbs - base-at-command-time"
//  only converges correctly if the servo never leaves that revolution
//  while approaching. A direct Mode-0 move is therefore only safe when
//  the current position and the target already share a revolution
//  ("bucket"); anything that would cross a revolution boundary must
//  leapfrog through Mode 1 (WHEEL) until it lands in the target's
//  bucket, then drop to Mode 0 (SETTLE) for the final approach.
// ============================================================
static inline double bucketOf(int32_t abs_) { return floor((double)abs_ / 4096.0); }
static inline bool sameBucket(int32_t a, int32_t b) { return bucketOf(a) == bucketOf(b); }

// Confirms the servo is really in `mode` before we rely on it, instead of
// trusting the firmware's own idea of it. One extra byte read per move start.
// A servo left in the wrong mode (e.g. wheel mode from a bench sketch, or a
// mode write that was silently rejected) accepts our writes and ignores them.
static void assertMode(JointTracker& j, uint8_t mode) {
  int16_t rb = g_bus.readByte(j.id, REG_OPERATING_MODE);
  if (rb < 0 || rb == (int16_t)mode) return;
  // Confirm with a second read before cycling torque — on this half-duplex
  // resistor-hack bus a single garbled reply must not be enough to make the
  // arm go briefly limp mid-jog.
  rb = g_bus.readByte(j.id, REG_OPERATING_MODE);
  if (rb >= 0 && rb != (int16_t)mode) {
    wsLog(String("[Servo] ") + j.name + " was in mode " + String(rb) +
          ", forcing mode " + String(mode));
    jointSetMode(j, mode);
  }
}

// Position of abs_ within its revolution (0-4095), i.e. what the raw encoder reads there.
static inline int32_t withinRev(int32_t abs_) { return abs_ - (int32_t)(bucketOf(abs_) * 4096.0); }

// How far abs_ sits inside its revolution, measured from the wrap a move
// travelling in `dir` entered that revolution through.
static inline int32_t stepsPastEntry(int32_t abs_, int8_t dir) {
  int32_t w = withinRev(abs_);
  return (dir > 0) ? w : (SERVO_STEPS_PER_REV - 1 - w);
}

// Nudges a target out of the WRAP_GUARD_STEPS band either side of the wrap,
// where a Mode-0 servo hunting around its goal would dip across it.
static int32_t guardWrap(int32_t targetAbs) {
  int32_t w = withinRev(targetAbs);
  if (w < WRAP_GUARD_STEPS) return targetAbs + (WRAP_GUARD_STEPS - w);
  if (w > SERVO_STEPS_PER_REV - 1 - WRAP_GUARD_STEPS) return targetAbs - (w - (SERVO_STEPS_PER_REV - 1 - WRAP_GUARD_STEPS));
  return targetAbs;
}

// Ends a move early WITHOUT a fault: the joint holds where it is, stays armed,
// and the reason goes to g_moveMsg for the UI.
static void abortLeg(JointTracker& j, const String& msg) {
  jointSetMode(j, 0);
  j.mv = JMove::HOLD;
  g_lastMoveOk = false;
  g_moveMsg = msg;
  wsLog(g_moveMsg);
}

static void startWheel(JointTracker& j, int32_t targetAbs, uint16_t speed, int8_t dir);

// The joint is (or has slipped back) on the wrong side of a wrap for a Mode-0
// approach. Go round again in wheel mode, a bounded number of times.
static void replanAcrossWrap(JointTracker& j) {
  if (++j.replans > MAX_LEAPFROG_REPLANS) {
    abortLeg(j, String(j.name) + " kept slipping back across the encoder wrap - aborted");
    return;
  }
  wsLog(String("[Servo] ") + j.name + " fell back across the encoder wrap - re-running the wheel leg");
  if (j.mv == JMove::SETTLE) j.mv = JMove::HOLD;   // servo is in Mode 0: have startWheel switch cleanly
  startWheel(j, j.targetAbs, j.cmdSpeed, (j.targetAbs > j.absSteps) ? 1 : -1);
}

static void startSettle(JointTracker& j, int32_t targetAbs, uint16_t speed) {
  j.targetAbs = targetAbs;
  if (j.mv == JMove::WHEEL) {
    jointSetMode(j, 0);
    if (!j.powered) { abortLeg(j, String(j.name) + " did not re-energise after its wheel leg - aborted"); return; }
    // Torque was off for the mode write and the drivetrain may have unwound
    // or sagged meanwhile. Re-read NOW: the goal below is computed against
    // the revolution the servo is really in, not where the last tick saw it.
    int32_t before = j.absSteps;
    trackJoint(j, millis());
    int32_t unwound = j.absSteps - before;
    if (labs(unwound) > POS_TOL_STEPS)
      wsLog(String("[Servo] ") + j.name + " moved " + String(unwound) + " steps during the wheel->position switch");
    if (!sameBucket(j.absSteps, targetAbs)) { j.mv = JMove::HOLD; replanAcrossWrap(j); return; }
  } else {
    assertMode(j, 0);
  }
  j.mv = JMove::SETTLE;
  // Fresh watchdog baseline: a settle may legitimately back up a little
  // after a wheel leg that overshot, which is not a wrong-way fault.
  j.wrongWayAccum = 0;
  j.lastProgressMs = millis();
  double base = bucketOf(j.absSteps) * 4096.0;
  double goalReg = (double)targetAbs - base;   // in [0,4095] given the sameBucket precondition
  g_bus.writeWord(j.id, REG_GOAL_SPEED, max((uint16_t)MIN_JOINT_SPEED, speed));
  g_bus.writeWord(j.id, REG_GOAL_POSITION, (uint16_t)llround(goalReg));
}

// A wheel leg hands over to Mode 0 once it is in the target's revolution AND
// far enough past the wrap it came through that unwind or hunting can't take
// it back across. A target closer to that wrap than the margin is handled by
// running up to the target itself. After a slip-back the margin is always
// applied in full and doubles per retry (overshoot, then Mode 0 backs up), so
// it cannot ping-pong on the same wrap.
static bool wheelLegDone(const JointTracker& j) {
  if (!sameBucket(j.absSteps, j.targetAbs)) return false;
  int32_t need = min((int32_t)WHEEL_HANDOFF_STEPS << j.replans, (int32_t)SERVO_STEPS_PER_REV / 2);
  if (j.replans == 0) need = min(need, stepsPastEntry(j.targetAbs, j.dir));
  return stepsPastEntry(j.absSteps, j.dir) >= need;
}

// The direction flag shares REG_GOAL_SPEED's 16-bit word with the magnitude,
// so the magnitude must stay strictly BELOW the flag or a fast forward command
// sets the reverse bit by itself: SPEED_STEPS_MAX (3400) already has bit 10
// set, which is the reverse bit on servos that use 0x0400.
static int wheelMagMax() {
  int magMax = (int)SPEED_STEPS_MAX;
  if ((int)g_wheelRevBit - 1 < magMax) magMax = (int)g_wheelRevBit - 1;
  if (magMax < (int)MIN_JOINT_SPEED)   magMax = (int)MIN_JOINT_SPEED;
  return magMax;
}

static void startWheel(JointTracker& j, int32_t targetAbs, uint16_t speed, int8_t dir) {
  if (j.mv != JMove::WHEEL) {
    jointSetMode(j, 1);
    // Fold in whatever the drivetrain unwound while torque was off, so the
    // wrong-way watchdog doesn't count it against the spin that follows.
    trackJoint(j, millis());
  } else {
    assertMode(j, 1);
  }
  j.mv = JMove::WHEEL;
  j.targetAbs = targetAbs;
  j.dir = dir;
  j.wrongWayAccum = 0;
  j.lastProgressMs = millis();
  j.lastGoodReadMs = millis();
  uint16_t mag = (uint16_t)constrain((int)speed, (int)MIN_JOINT_SPEED, wheelMagMax());
  g_bus.writeWord(j.id, REG_GOAL_SPEED, (dir < 0) ? (uint16_t)(mag | g_wheelRevBit) : mag);
}

static void planMove(JointTracker& j, int32_t targetAbs, uint16_t speed) {
  j.cmdSpeed = speed;
  if (&j == &g_j[0]) baseCapRearm();
  targetAbs = guardWrap(targetAbs);
  int32_t dist = targetAbs - j.absSteps;
  if (dist == 0) { j.mv = JMove::HOLD; return; }
  // Fresh progress-watchdog baseline for this move, whether it starts
  // straight in SETTLE or leapfrogs through WHEEL first (jointsTick's
  // no-progress check below covers both phases).
  j.wrongWayAccum = 0;
  j.missedReads = 0;
  j.replans = 0;
  j.lastProgressMs = millis();
  if (sameBucket(j.absSteps, targetAbs)) startSettle(j, targetAbs, speed);
  else                                   startWheel(j, targetAbs, speed, dist > 0 ? 1 : -1);
}

// Converts a desired output-shaft angle to an absolute step target, adding
// the backlash slack on a direction reversal. The slack is committed to
// blGoal here; trackJoint() moves blOff there as the servo takes it up.
static int32_t computeTargetAbs(int idx, float deg) {
  JointTracker& j = g_j[idx];
  int32_t eff = (int32_t)lround((deg - j.refDeg) * j.sign * j.ratio * (float)SERVO_STEPS_PER_REV / 360.0f);
  int32_t provisional = j.zeroAbs + j.blGoal + eff;
  int8_t newDir = (provisional > j.absSteps) ? 1 : (provisional < j.absSteps ? -1 : 0);
  if (newDir != 0 && newDir != j.dir) j.blGoal += (newDir > 0 ? j.bl : -j.bl);
  if (newDir != 0) j.dir = newDir;
  return j.zeroAbs + j.blGoal + eff;
}

// ============================================================
//  Public motion API
// ============================================================
bool armMoveToAngles(const JointAngles& a, uint16_t speed) {
  if (!motionAllowed()) return false;
  float deg[N_JOINTS] = { degrees(a.base), degrees(a.shoulder), degrees(a.elbow), degrees(a.forearm) };
  int32_t targetAbs[N_JOINTS];
  double  dist[N_JOINTS];
  for (int i = 0; i < N_JOINTS; i++) {
    deg[i] = constrain(deg[i], JOINT_MIN_DEG[i], JOINT_MAX_DEG[i]);
    targetAbs[i] = computeTargetAbs(i, deg[i]);
    dist[i] = fabs((double)targetAbs[i] - (double)g_j[i].absSteps);
  }
  int lead = 0;
  for (int i = 1; i < N_JOINTS; i++) if (dist[i] > dist[lead]) lead = i;
  uint16_t leadSpeed = safeSpeed(speed);

  g_moveMsg = ""; g_lastMoveOk = true;
  for (int i = 0; i < N_JOINTS; i++) {
    uint16_t sp = (dist[lead] < 1.0) ? leadSpeed
                : (uint16_t)max((double)MIN_JOINT_SPEED, leadSpeed * (dist[i] / dist[lead]));
    planMove(g_j[i], targetAbs[i], sp);
  }
  return true;
}

static int jointIndexFromName(const char* name) {
  if (!strcmp(name, "base"))     return 0;
  if (!strcmp(name, "shoulder")) return 1;
  if (!strcmp(name, "elbow"))    return 2;
  if (!strcmp(name, "forearm"))  return 3;
  return -1;
}

void setGripperSteps(int16_t steps, uint16_t speed);   // fwd decls (gripper section below)
int32_t gripperPos();

// Gripper travel window in raw steps: between the taught open/closed
// positions once set, otherwise anywhere that keeps Mode 0 off the wrap.
static void gripperLimits(int32_t& lo, int32_t& hi) {
  if (gripperCalibrated()) { lo = min(g_gripOpen, g_gripClose); hi = max(g_gripOpen, g_gripClose); }
  else                     { lo = WRAP_GUARD_STEPS; hi = SERVO_STEPS_PER_REV - 1 - WRAP_GUARD_STEPS; }
}

bool jogJoint(const char* name, float deltaDeg) {
  if (!motionAllowed() || !allJointsIdle()) return false;

  if (!strcmp(name, "gripper")) {
    int32_t gp = gripperPos();
    if (gp < 0) return false;
    // RELATIVE to where the gripper really is. This used to clamp an absolute
    // angle into 0..90 deg around raw 2048, so from any start outside that
    // window the first 1-degree press jumped the jaws thousands of steps
    // into their hard stop.
    int32_t lo, hi;
    gripperLimits(lo, hi);
    int32_t tgt = gp + lroundf(deltaDeg * SERVO_STEPS_PER_REV / 360.0f);
    if (gp < lo || gp > hi) {
      // Already outside the window (e.g. calibrated after being moved): only
      // allow jogs that head back toward it, never further out.
      if ((gp < lo && tgt < gp) || (gp > hi && tgt > gp)) return false;
    } else {
      tgt = constrain(tgt, lo, hi);
    }
    setGripperSteps((int16_t)tgt, JOG_SPEED);
    return true;
  }

  int idx = jointIndexFromName(name);
  if (idx < 0) return false;
  float newDeg = constrain(jDeg(g_j[idx]) + deltaDeg, JOINT_MIN_DEG[idx], JOINT_MAX_DEG[idx]);
  int32_t targetAbs = computeTargetAbs(idx, newDeg);

  g_moveMsg = ""; g_lastMoveOk = true;
  planMove(g_j[idx], targetAbs, safeSpeed(JOG_SPEED));
  return true;
}

void abortAllMotion() {
  for (int i = 0; i < N_JOINTS; i++) {
    if (g_j[i].mv != JMove::HOLD) { jointSetMode(g_j[i], 0); g_j[i].mv = JMove::HOLD; }
  }
}

// ============================================================
//  Base spin test — the base driven as a free motor, on until stopped.
//
//  No target and no angle limit: the base sits in wheel mode at the chosen
//  speed and direction until Stop, an E-STOP, or a watchdog ends it. While it
//  runs, jointsTick() skips the base's hard-limit check and its wheel->position
//  handoff. What still applies: E-STOP, encoder loss, the wrong-way / no-motion
//  (stall) watchdog, and the temp/load/voltage monitor.
//
//  The speed may be changed while running; the direction may not (stop first).
//  Stopping re-targets the wheel leg WHEEL_HANDOFF_STEPS ahead of where the
//  joint is, so the wheel->position handoff happens clear of the encoder wrap.
//
//  A free spin carries the base's tracked angle arbitrarily far past its limits,
//  where the hard-limit watchdog would E-STOP the whole arm the moment the spin
//  ended (and a base jog would clamp to a limit = a long run back). So when the
//  spin ends the base is re-referenced to read the angle it read at the start.
//  The tracked base angle means nothing after a free spin until the base is
//  jogged to its real zero and "Set this position as 0" is pressed.
//
//  The speed is a percentage of the wheel-mode ceiling and deliberately NOT
//  multiplied by g_safetyScale: at the default 10 % scale the whole 10-100 %
//  range would collapse to a crawl, and the speed is chosen by the user on
//  every start. Acceleration is still g_safetyScale's.
// ============================================================
static uint16_t baseSpinSpeed(int pct) {
  return (uint16_t)min((int)SPEED_STEPS_MAX * pct / 100, wheelMagMax());
}

// True while the base is free-running (not yet handing off to hold).
inline bool baseFreeSpinning() { return g_baseSpin.active && !g_baseSpin.stopping; }

// Returns "" on success, otherwise why the spin was refused.
String baseSpinStart(int dir, int pct) {
  if (!motionAllowed())               return "not armed, fault active, or teach mode on";
  if (dir != 1 && dir != -1)          return "dir must be 1 or -1";
  if (pct < 10 || pct > 100)          return "speed must be 10-100 %";
  JointTracker& j = g_j[0];
  uint16_t speed = baseSpinSpeed(pct);

  if (g_baseSpin.active) {
    // Live speed change. Reversing a loaded worm at speed is left to a stop first.
    if (g_baseSpin.stopping || j.mv != JMove::WHEEL) return "base is stopping";
    if (dir != g_baseSpin.dir)                       return "stop before changing direction";
    j.cmdSpeed = speed;
    startWheel(j, j.targetAbs, speed, (int8_t)dir);
    baseCapRearm();
    g_baseSpin.pct = (uint8_t)pct; g_baseSpin.speed = speed;
    char buf[64];
    snprintf(buf, sizeof(buf), "[Test] Base spin speed -> %d%% (%u steps/s)", pct, (unsigned)speed);
    wsLog(buf);
    return "";
  }
  if (!allJointsIdle())               return "arm is still moving";

  g_moveMsg = ""; g_lastMoveOk = true;
  j.cmdSpeed = speed;
  j.missedReads = 0;
  j.replans = 0;
  g_baseSpin.startAbs = j.absSteps;
  g_baseSpin.startEff = jEff(j);
  g_baseSpin.steps = 0;
  startWheel(j, j.absSteps, speed, (int8_t)dir);   // target unused until Stop
  baseCapRearm();
  if (!j.powered) { jointSetMode(j, 0); j.mv = JMove::HOLD; return "base did not energise in wheel mode"; }

  g_baseSpin.active = true; g_baseSpin.stopping = false;
  g_baseSpin.dir = (int8_t)dir; g_baseSpin.pct = (uint8_t)pct; g_baseSpin.speed = speed;
  char buf[96];
  snprintf(buf, sizeof(buf), "[Test] Base free spin %s at %d%% (%u steps/s)",
           dir > 0 ? "+" : "-", pct, (unsigned)speed);
  wsLog(buf);
  return "";
}

void baseSpinStop() {
  JointTracker& j = g_j[0];
  if (!g_baseSpin.active || g_baseSpin.stopping || j.mv == JMove::HOLD) return;
  g_baseSpin.stopping = true;
  if (j.mv == JMove::WHEEL) {
    // Don't drop to Mode 0 on the spot: this may be a few steps past the wrap,
    // where the torque-off mode switch lets the drivetrain unwind back across
    // it. Give the wheel leg a target just ahead instead and let wheelLegDone()
    // hand off once it is clear.
    j.targetAbs = guardWrap(j.absSteps + (int32_t)j.dir * WHEEL_HANDOFF_STEPS);
  } else {
    // SETTLE is already in its target's revolution and heading into it, so
    // holding right here cannot cross a wrap.
    startSettle(j, guardWrap(j.absSteps), j.cmdSpeed);
  }
}

// Counts the spin's servo turns and notices it has ended (stopped, aborted or
// E-STOPped), re-referencing the base so it reads what it read at the start.
static void baseSpinTick() {
  if (!g_baseSpin.active) return;
  JointTracker& j = g_j[0];
  g_baseSpin.steps = j.absSteps - g_baseSpin.startAbs;
  if (j.mv != JMove::HOLD) return;
  g_baseSpin.active = false;
  j.blGoal = j.blOff;                          // no half-taken-up slack left to move the reading later
  j.zeroAbs += jEff(j) - g_baseSpin.startEff;  // jEff() == startEff again
  char buf[128];
  snprintf(buf, sizeof(buf), "[Test] Base spin %s after %.2f servo turns; base angle reset to %.1f deg",
           g_faultActive ? "cut by E-STOP" : (!g_lastMoveOk ? "aborted" : "stopped"),
           (float)g_baseSpin.steps / SERVO_STEPS_PER_REV, jDeg(j));
  wsLog(buf);
}

// Base torque cap: the base has sat at its torque limit for BASE_CAP_HOLD_MS.
// Fast -> drop to BASE_CAP_SLOW_SPEED and keep going; already slow -> stop and
// hold (no fault: the worm self-locks, and an E-STOP would drop the whole arm).
// Called from jointsTick() for a moving base with a good position read this
// tick. Returns true if it re-commanded or ended the move.
static bool baseCapTick(JointTracker& j, uint32_t now) {
  if ((int32_t)(now - g_baseCap.graceUntil) < 0) { g_baseCap.over = false; return false; }
  int16_t raw = g_bus.readWord(j.id, REG_PRESENT_LOAD);
  if (raw < 0) return false;   // no reading: judge nothing, keep the timer
  uint16_t load = (uint16_t)raw & 0x03FF;
  g_baseCap.load = load;
  if ((uint32_t)load * 100 < (uint32_t)j.tq * BASE_CAP_LOAD_PCT) { g_baseCap.over = false; return false; }
  if (!g_baseCap.over) { g_baseCap.over = true; g_baseCap.overSince = now; return false; }
  if (now - g_baseCap.overSince < BASE_CAP_HOLD_MS) return false;

  g_baseCap.over = false;
  char buf[128];
  if (j.cmdSpeed > BASE_CAP_SLOW_SPEED) {
    snprintf(buf, sizeof(buf), "[Servo] Base at torque cap (load %u of %u) at %u steps/s - slowing to %u",
             (unsigned)load, (unsigned)j.tq, (unsigned)j.cmdSpeed, (unsigned)BASE_CAP_SLOW_SPEED);
    wsLog(buf);
    j.cmdSpeed = BASE_CAP_SLOW_SPEED;
    if (j.mv == JMove::WHEEL) startWheel(j, j.targetAbs, j.cmdSpeed, j.dir);
    else                      startSettle(j, j.targetAbs, j.cmdSpeed);
    g_baseCap.graceUntil = now + BASE_CAP_GRACE_MS;
    if (g_baseSpin.active) {
      g_baseSpin.speed = j.cmdSpeed;
      g_baseSpin.pct = (uint8_t)((uint32_t)j.cmdSpeed * 100 / SPEED_STEPS_MAX);
    }
    return true;
  }
  snprintf(buf, sizeof(buf), "Base at torque cap (load %u of %u) at %u steps/s - stopped",
           (unsigned)load, (unsigned)j.tq, (unsigned)j.cmdSpeed);
  abortLeg(j, buf);
  return true;
}

void emergencyStop(const String& reason);   // fwd decl — jointsTick's watchdogs call it

// ============================================================
//  jointsTick — advances the HOLD/WHEEL/SETTLE state machine and
//  runs the per-joint watchdogs. Pumped every JOINT_POLL_MS from
//  tickServers() and loop(). Always calls updateTrackers() first,
//  even mid-fault, so the live position never goes stale.
// ============================================================
void jointsTick() {
  static uint32_t lastMs = 0;
  uint32_t now = millis();
  if (now - lastMs < JOINT_POLL_MS) return;
  lastMs = now;

  updateTrackers();
  baseSpinTick();
  if (g_faultActive) return;   // tracking stays live; no new commands while faulted

  for (int i = 0; i < N_JOINTS; i++) {
    JointTracker& j = g_j[i];

    // Hard joint limit — checked every tick, even while HOLDing, so an
    // external force (slipped belt, someone pushing the arm) still trips it.
    // Not for the base during a spin test, which has no angle limit by design.
    if (j.powered && !(i == 0 && g_baseSpin.active)) {
      float d = jDeg(j);
      if (d < JOINT_MIN_DEG[i] - JOINT_LIMIT_SLACK_DEG || d > JOINT_MAX_DEG[i] + JOINT_LIMIT_SLACK_DEG) {
        emergencyStop(String(j.name) + " beyond hard limit: " + String(d, 1) + " deg");
        return;
      }
    }

    if (j.mv == JMove::HOLD) continue;

    if (j.missedReads > WHEEL_MISSED_READS_MAX) {
      emergencyStop(String(j.name) + " Lost encoder feedback during motion");
      return;
    }
    // No reading this tick: nothing below can be judged, and a comms dropout
    // must end in the E-STOP above, not be mistaken for a stall (the base's
    // shorter stall timeout used to fire first and just hold).
    if (j.missedReads > 0) continue;

    // A Mode-0 SETTLE is only valid while the joint is in the target's
    // revolution. If it has slipped back across the wrap, its goal register
    // now points the long way round and it is about to drive backwards.
    // Catch that on the first tick, before it gets anywhere.
    if (j.mv == JMove::SETTLE && !sameBucket(j.absSteps, j.targetAbs)) {
      replanAcrossWrap(j);
      continue;
    }

    // No-progress watchdog — applies to BOTH phases of a move, not just the
    // WHEEL leapfrog leg. A Mode-0 SETTLE that never actually arrives (not
    // enough torque, mechanical resistance, anything) must not be able to
    // sit there forever: with no timeout here, allJointsIdle() would never
    // become true again, silently locking out every future command with no
    // fault raised — the servo would just keep pushing toward an unreached
    // goal indefinitely.
    // WHEEL: progress is along the commanded spin. SETTLE: progress is toward
    // the target, which after an overshooting wheel leg may mean backing up.
    int8_t fwd = j.dir;
    if (j.mv == JMove::SETTLE) fwd = (j.targetAbs >= j.absSteps - j.lastDelta) ? 1 : -1;
    int32_t progress = fwd * j.lastDelta;
    if (progress > 0) { j.wrongWayAccum = 0; j.lastProgressMs = now; }
    else               { j.wrongWayAccum += -progress; }

    if (j.wrongWayAccum > WRONG_WAY_STEPS || now - j.lastProgressMs > noMotionMs(i)) {
      const bool wrongWay = (j.wrongWayAccum > WRONG_WAY_STEPS);
      const bool inWheel   = (j.mv == JMove::WHEEL);
      const char* why = wrongWay ? "WRONG WAY" : "NO MOTION";
      abortLeg(j, String(j.name) + " " + why + " - aborted");
      // A wheel-mode leg that runs backwards almost always means the reverse
      // bit written into REG_GOAL_SPEED is not the one these servos use: the
      // servo then reads "reverse at N" as "forward at N + revbit". Say so,
      // because the symptom (a fast lurch the wrong way) looks nothing like
      // a configuration error.
      if (inWheel && wrongWay)
        wsLog(String("[Servo] ") + j.name + " reversed in WHEEL mode - wheel reverse bit " +
              String((unsigned)g_wheelRevBit) + " is likely wrong for these servos; "
              "toggle it via POST /teach/cfg {\"revbit\":1}");
      continue;
    }

    if (i == 0 && baseCapTick(j, now)) continue;

    if (j.mv == JMove::WHEEL) {
      if (i == 0 && baseFreeSpinning()) continue;   // runs until Stop
      if (wheelLegDone(j)) startSettle(j, j.targetAbs, j.cmdSpeed);
    } else if (j.mv == JMove::SETTLE) {
      if (labs(j.targetAbs - j.absSteps) <= POS_TOL_STEPS) j.mv = JMove::HOLD;
    }
  }
}

// waitForMotionComplete never itself touches the bus beyond what jointsTick
// does — it pumps tickFn() (tickServers, so WiFi/WS/HTTP keep running) and
// jointsTick() every POLL_INTERVAL_MS until every joint reports HOLD.
bool waitForMotionComplete(uint32_t timeout_ms, int16_t tol = 8, void (*tickFn)() = nullptr) {
  (void)tol;   // arrival tolerance is POS_TOL_STEPS, enforced inside jointsTick
  uint32_t t0 = millis();
  while (millis() - t0 < timeout_ms) {
    if (g_faultActive) return false;
    jointsTick();
    if (allJointsIdle()) return true;
    if (tickFn) tickFn();
    delay(POLL_INTERVAL_MS);
  }
  wsLog(F("[Servo] Motion timeout"));
  return false;
}

// ============================================================
//  SAFETY — Emergency Stop / Fault clear
// ============================================================
void emergencyStop(const String& reason) {
  if (g_faultActive) return;
  g_faultActive = true;
  g_faultReason = reason;

  g_bus.writeByte(0xFE, REG_TORQUE_ENABLE, 0);   // broadcast first
#if GRIPPER_OWN_BUS
  g_gripBus.writeByte(0xFE, REG_TORQUE_ENABLE, 0);
#endif
  uint8_t ids[] = { SERVO_BASE, SERVO_SHOULDER, SERVO_ELBOW, SERVO_FOREARM, SERVO_GRIPPER };
  // Broadcast above is the fast path; these are verified so a single dropped
  // packet can never leave a joint powered after an E-STOP.
  for (uint8_t id : ids) {
    if (!writeByteVerified(id, REG_TORQUE_ENABLE, 0) && id != SERVO_GRIPPER)
      wsLog("[E-STOP] WARNING: ID" + String(id) + " did NOT drop torque");
  }
  for (int i = 0; i < N_JOINTS; i++) { g_j[i].powered = false; g_j[i].mv = JMove::HOLD; }

  wsLog(F("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
  wsLog(F("  EMERGENCY STOP"));
  wsLog("  Reason: " + g_faultReason);
  wsLog(F("  All torque cut. Send fault_reset to recover."));
  wsLog(F("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"));
}

int32_t gripperPos() {
  int16_t raw = busFor(SERVO_GRIPPER).readWord(SERVO_GRIPPER, REG_PRESENT_POS);
  return (raw >= 0) ? (int32_t)raw : -1;
}

// clearFault HOLDS in place — never moves. Re-energising always reads the
// servo's own current position and uses that as the goal, so there is
// never a stale-goal snap on re-enable.
void clearFault() {
  g_faultActive = false;
  g_faultReason = "";
  for (int i = 0; i < N_JOINTS; i++) jointSetMode(g_j[i], 0);

  int32_t gp = gripperPos();
  busFor(SERVO_GRIPPER).writeByte(SERVO_GRIPPER, REG_TORQUE_ENABLE, 0);
  busFor(SERVO_GRIPPER).writeWord(SERVO_GRIPPER, REG_TORQUE_LIMIT, GRIP_TORQUE);
  busFor(SERVO_GRIPPER).writeWord(SERVO_GRIPPER, REG_GOAL_SPEED, safeSpeed(DEFAULT_SPEED));
  if (gp >= 0) busFor(SERVO_GRIPPER).writeWord(SERVO_GRIPPER, REG_GOAL_POSITION, (uint16_t)gp);
  busFor(SERVO_GRIPPER).writeByte(SERVO_GRIPPER, REG_TORQUE_ENABLE, 1);

  wsLog(F("[Safety] Fault cleared — torque re-enabled, holding current pose."));
}

// ============================================================
//  SAFETY — Monitor (call from tickServers). One servo per call,
//  round-robin, so it never stalls the bus.
// ============================================================
void runSafetyMonitor() {
  if (g_faultActive) return;

  static uint8_t  idx    = 0;
  static uint32_t lastMs = 0;
  if (millis() - lastMs < SAFETY_MONITOR_MS) return;
  lastMs = millis();

  static const uint8_t ids[] = { SERVO_BASE, SERVO_SHOULDER, SERVO_ELBOW, SERVO_FOREARM, SERVO_GRIPPER };
  uint8_t id = ids[idx];

  static uint8_t tempStrikes[5] = { 0, 0, 0, 0, 0 };
  int16_t temp = busFor(id).readByte(id, REG_PRESENT_TEMP);
  if (temp >= 0 && temp > SAFETY_TEMP_MAX_C) {
    if (++tempStrikes[idx] >= SAFETY_TEMP_STRIKES) {
      emergencyStop("Servo ID" + String(id) + " temp " + String(temp) + "C > limit " + String(SAFETY_TEMP_MAX_C) +
                    "C for " + String(SAFETY_TEMP_STRIKES) + " polls");
      return;
    }
    // Logged with the load, so a blip can be told apart from a joint that is
    // genuinely straining (the sensor sits by the driver and heats long
    // before the housing does).
    int16_t l = busFor(id).readWord(id, REG_PRESENT_LOAD);
    char buf[96];
    snprintf(buf, sizeof(buf), "[Safety] warn: Servo ID%d temp=%dC load=%d (strike %d/%d)",
             id, temp, (l >= 0) ? (l & 0x03FF) : -1, tempStrikes[idx], SAFETY_TEMP_STRIKES);
    wsLog(buf);
  } else if (temp >= 0) {
    tempStrikes[idx] = 0;
  }

  static uint8_t loadStrikes[5] = { 0, 0, 0, 0, 0 };
  int16_t loadRaw = busFor(id).readWord(id, REG_PRESENT_LOAD);
  if (loadRaw >= 0) {
    uint16_t loadMag = (uint16_t)loadRaw & 0x03FF;
    if (loadMag > SAFETY_LOAD_MAX) {
      loadStrikes[idx]++;
      if (loadStrikes[idx] >= SAFETY_LOAD_STRIKES) {
        emergencyStop("Servo ID" + String(id) + " load " + String(loadMag) + " > limit " + String(SAFETY_LOAD_MAX) +
                      " for " + String(SAFETY_LOAD_STRIKES) + " polls");
        return;
      }
      char buf[80];
      snprintf(buf, sizeof(buf), "[Safety] warn: Servo ID%d load=%d (strike %d/%d)", id, loadMag, loadStrikes[idx], SAFETY_LOAD_STRIKES);
      wsLog(buf);
    } else {
      loadStrikes[idx] = 0;
    }
  }

  if (idx == 0) {
    int16_t volt = busFor(id).readByte(id, REG_PRESENT_VOLTAGE);
    if (volt >= 0) {
      if (volt < SAFETY_VOLTAGE_MIN_DV) {
        emergencyStop("Supply voltage " + String(volt * 0.1f, 1) + "V below minimum " + String(SAFETY_VOLTAGE_MIN_DV * 0.1f, 1) + "V");
        return;
      }
      if (volt > SAFETY_VOLTAGE_MAX_DV) {
        emergencyStop("Supply voltage " + String(volt * 0.1f, 1) + "V above maximum " + String(SAFETY_VOLTAGE_MAX_DV * 0.1f, 1) + "V");
        return;
      }
    }
  }

  idx = (idx + 1) % 5;
}

// ============================================================
//  applyAccel — apply acceleration register to all arm joints.
// ============================================================
void applyAccel() {
  uint8_t accel = safeAccel();
  uint8_t ids[] = { SERVO_BASE, SERVO_SHOULDER, SERVO_ELBOW, SERVO_FOREARM };
  for (uint8_t id : ids) { busFor(id).writeByte(id, REG_GOAL_ACCEL, accel); delay(2); }
  Serial.printf("[Servo] Scale=%.0f%%  speed x%.2f  accel=%d steps/ms^2\n", g_safetyScale * 100.0f, g_safetyScale, accel);
}

// ============================================================
//  servoInit — boot sequence. Forces every arm joint back to
//  Mode 0 (in case the last power-down left one in wheel mode),
//  captures the boot reference (zeroAbs/bootRaw/refDeg), and
//  re-energises everything holding exactly its current position —
//  the arm must never move during boot.
// ============================================================
void servoInit() {
  Serial1.begin(SERVO_BAUD, SERIAL_8N1, PIN_SERVO_RX, PIN_SERVO_TX);
  g_bus.begin(&Serial1, PIN_SERVO_DIR);
#if GRIPPER_OWN_BUS
  Serial2.begin(SERVO_BAUD, SERIAL_8N1, PIN_GRIP_RX, PIN_GRIP_TX);
  g_gripBus.begin(&Serial2, -1);
#endif
  delay(150);

  loadJointConfig();

  uint8_t allIds[] = { SERVO_BASE, SERVO_SHOULDER, SERVO_ELBOW, SERVO_FOREARM, SERVO_GRIPPER };
  for (uint8_t id : allIds) { busFor(id).writeByte(id, REG_TORQUE_ENABLE, 0); delay(8); }

  for (int i = 0; i < N_JOINTS; i++) {
    JointTracker& j = g_j[i];
    g_bus.writeByte(j.id, REG_OPERATING_MODE, 0);   // force out of any stale wheel mode
    delay(8);
    int16_t raw = g_bus.readWord(j.id, REG_PRESENT_POS);
    j.ok      = (raw >= 0);
    j.lastRaw = (raw >= 0) ? raw : 0;
    j.bootRaw = j.lastRaw;
    j.absSteps = j.lastRaw;
    j.zeroAbs  = j.lastRaw;
    j.blOff    = 0;
    j.blGoal   = 0;
    j.dir      = 1;   // assume the arm was left backlash-settled on the "+" side
    j.mv       = JMove::HOLD;
    j.lastGoodReadMs = millis();
    g_bus.writeWord(j.id, REG_GOAL_SPEED, safeSpeed(DEFAULT_SPEED));
    if (raw >= 0) g_bus.writeWord(j.id, REG_GOAL_POSITION, (uint16_t)raw);
  }

  busFor(SERVO_GRIPPER).writeByte(SERVO_GRIPPER, REG_OPERATING_MODE, 0);
  delay(8);
  int16_t gRaw = busFor(SERVO_GRIPPER).readWord(SERVO_GRIPPER, REG_PRESENT_POS);
  busFor(SERVO_GRIPPER).writeWord(SERVO_GRIPPER, REG_TORQUE_LIMIT, GRIP_TORQUE);
  busFor(SERVO_GRIPPER).writeWord(SERVO_GRIPPER, REG_GOAL_SPEED, safeSpeed(DEFAULT_SPEED));
  if (gRaw >= 0) busFor(SERVO_GRIPPER).writeWord(SERVO_GRIPPER, REG_GOAL_POSITION, (uint16_t)gRaw);

  for (int k = 0; k <= N_JOINTS; k++) {
    uint8_t id = allIds[k];
    // The gripper keeps its own, lower limit — this loop used to overwrite
    // the GRIP_TORQUE written above with the joints' DEFAULT_TORQUE, so the
    // gripper squeezed at twice the intended force (diag read 700).
    busFor(id).writeWord(id, REG_TORQUE_LIMIT, id == SERVO_GRIPPER ? GRIP_TORQUE : g_j[k].tq);
    delay(8);
    // Gripper may legitimately be absent during bring-up; don't fail on it.
    if (!writeByteVerified(id, REG_TORQUE_ENABLE, 1) && id != SERVO_GRIPPER) {
      wsLog("[Servo] ID" + String(id) + " TORQUE ENABLE FAILED at boot - joint is limp");
    }
    delay(8);
  }
  // Trust the servo's own register, not the fact that we sent a write.
  for (int i = 0; i < N_JOINTS; i++) {
    g_j[i].powered = (g_bus.readByte(g_j[i].id, REG_TORQUE_ENABLE) == 1);
  }

  applyAccel();

  Serial.println(F("[Servo] Init OK — Mode 0 + trackers armed, holding boot pose"));
  Serial.println(F("[Servo] *** ARM MUST BE AT MECHANICAL HOME BEFORE USE ***"));
  Serial.printf ("[Servo] Safety scale: %.0f%% (send {\"type\":\"safety\",\"scale\":x} to change)\n", g_safetyScale * 100.0f);
}

// ============================================================
//  Gripper — single-revolution Mode 0 joint, no leapfrog tracking.
// ============================================================
void setGripperSteps(int16_t steps, uint16_t speed) {
  if (g_faultActive) return;
  busFor(SERVO_GRIPPER).writeWord(SERVO_GRIPPER, REG_GOAL_SPEED, safeSpeed(speed));
  busFor(SERVO_GRIPPER).writeWord(SERVO_GRIPPER, REG_GOAL_POSITION,
                                  (uint16_t)constrain((int)steps, WRAP_GUARD_STEPS, SERVO_STEPS_PER_REV - 1 - WRAP_GUARD_STEPS));
}

void gripperOpen() {
  if (g_faultActive) return;
  if (!gripperCalibrated()) { wsLog(F("[Gripper] open/closed positions not set - jog it and use Set open / Set closed")); return; }
  busFor(SERVO_GRIPPER).writeWord(SERVO_GRIPPER, REG_TORQUE_LIMIT, GRIP_TORQUE);
  setGripperSteps(g_gripOpen, DEFAULT_SPEED);
}

void gripperClose() {
  if (g_faultActive) return;
  if (!gripperCalibrated()) { wsLog(F("[Gripper] open/closed positions not set - jog it and use Set open / Set closed")); return; }
  busFor(SERVO_GRIPPER).writeWord(SERVO_GRIPPER, REG_TORQUE_LIMIT, GRIP_TORQUE);
  setGripperSteps(g_gripClose, DEFAULT_SPEED / 2);
}

int16_t gripperLoad() {
  int16_t raw = busFor(SERVO_GRIPPER).readWord(SERVO_GRIPPER, REG_PRESENT_LOAD);
  if (raw < 0) return 0;
  return (int16_t)(raw & 0x03FF);
}

// [FIX 3] Gripper is Mode 0 (16-bit): readWord/writeWord, not the DWord API.
// tickFn: called every poll (pass tickServers so WiFi/WS/HTTP/safety-monitor
// keep running for the up-to-3s this can block — the gripper is the one
// place in the FSM where the arm is deliberately under load, exactly when
// the safety monitor matters most).
void gripperCloseToContact(int16_t contactLoad = 200, void (*tickFn)() = nullptr) {
  if (g_faultActive) return;
  if (!gripperCalibrated()) { wsLog(F("[Gripper] open/closed positions not set - not closing")); return; }
  busFor(SERVO_GRIPPER).writeWord(SERVO_GRIPPER, REG_TORQUE_LIMIT, GRIP_TORQUE);
  setGripperSteps(g_gripClose, 300);

  uint32_t t0 = millis();
  while (millis() - t0 < 3000) {
    if (g_faultActive) return;
    int16_t load = gripperLoad();
    if (load > contactLoad) {
      int16_t pos = busFor(SERVO_GRIPPER).readWord(SERVO_GRIPPER, REG_PRESENT_POS);
      if (pos >= 0 && pos < SERVO_STEPS_PER_REV) busFor(SERVO_GRIPPER).writeWord(SERVO_GRIPPER, REG_GOAL_POSITION, (uint16_t)pos);
      char buf[48];
      snprintf(buf, sizeof(buf), "[Gripper] Contact at load=%d — holding", load);
      wsLog(buf);
      return;
    }
    if (tickFn) tickFn();
    delay(20);
  }
  wsLog(F("[Gripper] No contact detected — fully closed"));
}

void printCurrentPositions() {
  int16_t volt = g_bus.readByte(SERVO_BASE, REG_PRESENT_VOLTAGE);
  int16_t temp = g_bus.readByte(SERVO_BASE, REG_PRESENT_TEMP);
  Serial.printf("  [Safety] scale=%.0f%%  fault=%s  volt=%.1fV  base_temp=%dC\n",
                g_safetyScale * 100.0f, g_faultActive ? "YES" : "no", volt >= 0 ? volt * 0.1f : -1.0f, temp);
  for (int i = 0; i < N_JOINTS; i++) {
    JointTracker& j = g_j[i];
    int16_t t = g_bus.readByte(j.id, REG_PRESENT_TEMP);
    int16_t l = g_bus.readWord(j.id, REG_PRESENT_LOAD);
    uint16_t lm = (l >= 0) ? (l & 0x03FF) : 0;
    Serial.printf("  %s (ID%d): %.1f deg  temp=%dC  load=%d  mv=%d\n", j.name, j.id, jDeg(j), t, lm, (int)j.mv);
  }
  int32_t gp = gripperPos();
  int16_t gl = gripperLoad();
  if (gp < 0) Serial.println("  Gripper: READ ERR");
  else        Serial.printf("  Gripper (ID%d): %d steps  load=%d\n", SERVO_GRIPPER, (int)gp, gl);
}
