#pragma once
// ============================================================
//  PROJET ARACHNAE v2 — Servo Control
//  ST3215 × 5 via Waveshare Serial Bus Servo Driver
//  SCSerial half-duplex UART, 1 Mbaud
//
//  BUG FIXES:
//   [FIX 1] readDWord: response is 10 bytes not 9. Checksum
//           left in UART buffer caused serial desync.
//   [FIX 2] readWord: response is 8 bytes not 7. Same issue.
//   [FIX 3] gripperCloseToContact used readDWord/writeDWord on
//           a 16-bit standard-mode servo, corrupting 0x2C-0x2D.
//   [FIX 4] REG_PRESENT_LOAD was 0x3E (temperature register!).
//           Real load is 0x3C. Contact sensing now actually works.
//
//  SAFETY ADDITIONS:
//   • g_safetyScale (0.0–1.0) — multiplied into every speed and
//     acceleration command. Default 0.10 (10%). Adjust via the
//     WebSocket command {"type":"safety","scale":0.25}.
//   • runSafetyMonitor() — call from tickServers(). Polls one
//     servo per call in round-robin. Triggers emergencyStop() if
//     temperature, load, or voltage exceed thresholds.
//   • emergencyStop() — cuts all torque instantly, sets
//     g_faultActive. FSM and motion wait both check this flag.
// ============================================================

#include <Arduino.h>
#include <HardwareSerial.h>
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
#define REG_PRESENT_LOAD    0x3C   // [FIX 4] was 0x3E (temperature). Load = bits[9:0]
#define REG_PRESENT_VOLTAGE 0x3D   // byte, ×0.1 V
#define REG_PRESENT_TEMP    0x3E   // byte, °C

// ── GrabFSM and ArmState ─────────────────────────────────────
enum class GrabFSM : uint8_t {
  IDLE, SCANNING, APPROACH, DESCEND, GRIP, LIFT, HANDOFF, HOME
};

struct ArmState {
  JointAngles current;
  bool torqueEnabled = false;
  GrabFSM fsm = GrabFSM::IDLE;
};

// ============================================================
//  SAFETY GLOBALS
//  These are extern-visible throughout the sketch.
// ============================================================

// Master speed/acceleration scalar.
// All speed arguments are multiplied by this before being sent.
// Range: 0.0–1.0. Start at 0.10 (10%) and increase gradually.
// At 10%: APPROACH_SPEED 500 → 50 steps/sec (~3°/sec output on shoulder).
// Send {"type":"safety","scale":0.5} over WebSocket to change at runtime.
float g_safetyScale = SAFETY_SCALE_DEFAULT;

// Fault latch. Set by emergencyStop(). Blocks all FSM motion.
// Cleared only by an explicit {"type":"fault_reset"} command.
bool   g_faultActive = false;
String g_faultReason = "";

// Strike counters for load — require N consecutive over-threshold
// readings before tripping, to ignore brief contact spikes.
static uint8_t s_loadStrikes[5] = {0, 0, 0, 0, 0};

// ── safeSpeed ─────────────────────────────────────────────────
// Scale a raw speed constant by g_safetyScale.
// Minimum 1 so servos always accept the command.
inline uint16_t safeSpeed(uint16_t raw) {
  return (uint16_t)max(1, (int)(raw * g_safetyScale));
}

// ── safeAccel ─────────────────────────────────────────────────
// Scale the goal acceleration by g_safetyScale.
// 0 = hardware-maximum acceleration (no limiting).
// Returns a value in 1–254 steps/ms².
inline uint8_t safeAccel() {
  uint8_t a = (uint8_t)max(1, (int)(SAFETY_ACCEL_MAX * g_safetyScale));
  return a;
}

// ============================================================
//  SCSerial Bus Driver
//
//  Packet format reference:
//    Write:  FF FF  ID  LEN  0x03  REG  DATA...  CHKSUM
//    Read:   FF FF  ID  4    0x02  REG  N        CHKSUM
//    Resp:   FF FF  ID  LEN  ERR   DATA...        CHKSUM
//      Response total bytes = 6 + N  (verified: 1-byte=7, 2-byte=8, 4-byte=10)
// ============================================================
class SCSerialBus {
public:
  HardwareSerial* _serial;
  int8_t _dirPin;

  void begin(HardwareSerial* s, int8_t dirPin = -1) {
    _serial = s;
    _dirPin = dirPin;
    if (_dirPin >= 0) {
      pinMode(_dirPin, OUTPUT);
      digitalWrite(_dirPin, LOW);
    }
  }

  // ── Writes ───────────────────────────────────────────────
  void writeDWord(uint8_t id, uint8_t reg, int32_t val) {
    uint8_t b0=val&0xFF, b1=(val>>8)&0xFF,
            b2=(val>>16)&0xFF, b3=(val>>24)&0xFF;
    uint8_t pkt[11];
    pkt[0]=0xFF; pkt[1]=0xFF; pkt[2]=id;
    pkt[3]=7; pkt[4]=0x03; pkt[5]=reg;
    pkt[6]=b0; pkt[7]=b1; pkt[8]=b2; pkt[9]=b3;
    pkt[10]=~(id+7+0x03+reg+b0+b1+b2+b3)&0xFF;
    _send(pkt, 11);
  }

  void writeWord(uint8_t id, uint8_t reg, uint16_t val) {
    uint8_t lo=val&0xFF, hi=(val>>8)&0xFF;
    uint8_t pkt[9];
    pkt[0]=0xFF; pkt[1]=0xFF; pkt[2]=id;
    pkt[3]=5; pkt[4]=0x03; pkt[5]=reg;
    pkt[6]=lo; pkt[7]=hi;
    pkt[8]=~(id+5+0x03+reg+lo+hi)&0xFF;
    _send(pkt, 9);
  }

  void writeByte(uint8_t id, uint8_t reg, uint8_t val) {
    uint8_t pkt[8];
    pkt[0]=0xFF; pkt[1]=0xFF; pkt[2]=id;
    pkt[3]=4; pkt[4]=0x03; pkt[5]=reg; pkt[6]=val;
    pkt[7]=~(id+4+0x03+reg+val)&0xFF;
    _send(pkt, 8);
  }

  // ── Reads ────────────────────────────────────────────────
  // [FIX 1] DWord response: 10 bytes (was 9 — checksum left in buffer)
  int32_t readDWord(uint8_t id, uint8_t reg) {
    uint8_t req[8];
    req[0]=0xFF; req[1]=0xFF; req[2]=id; req[3]=4;
    req[4]=0x02; req[5]=reg; req[6]=4;
    req[7]=~(id+4+0x02+reg+4)&0xFF;
    _send(req, 8);
    uint8_t resp[10];
    uint32_t t=millis();
    while (_serial->available()<10 && millis()-t<8) delayMicroseconds(200);
    if (_serial->available()<10) return INT32_MIN;
    _serial->readBytes(resp, 10);
    if (resp[0]!=0xFF||resp[1]!=0xFF) return INT32_MIN;
    return (int32_t)(resp[5]|(resp[6]<<8)|(resp[7]<<16)|(resp[8]<<24));
  }

  // [FIX 2] Word response: 8 bytes (was 7 — checksum left in buffer)
  int16_t readWord(uint8_t id, uint8_t reg) {
    uint8_t req[8];
    req[0]=0xFF; req[1]=0xFF; req[2]=id; req[3]=4;
    req[4]=0x02; req[5]=reg; req[6]=2;
    req[7]=~(id+4+0x02+reg+2)&0xFF;
    _send(req, 8);
    uint8_t resp[8];
    uint32_t t=millis();
    while (_serial->available()<8 && millis()-t<8) delayMicroseconds(200);
    if (_serial->available()<8) return -1;
    _serial->readBytes(resp, 8);
    if (resp[0]!=0xFF||resp[1]!=0xFF) return -1;
    return (int16_t)(resp[5]|(resp[6]<<8));
  }

  // Byte response: 7 bytes (6 fixed + 1 data)
  int16_t readByte(uint8_t id, uint8_t reg) {
    uint8_t req[8];
    req[0]=0xFF; req[1]=0xFF; req[2]=id; req[3]=4;
    req[4]=0x02; req[5]=reg; req[6]=1;
    req[7]=~(id+4+0x02+reg+1)&0xFF;
    _send(req, 8);
    uint8_t resp[7];
    uint32_t t=millis();
    while (_serial->available()<7 && millis()-t<8) delayMicroseconds(200);
    if (_serial->available()<7) return -1;
    _serial->readBytes(resp, 7);
    if (resp[0]!=0xFF||resp[1]!=0xFF) return -1;
    return (int16_t)resp[5];  // 0–255 on success, -1 on error
  }

private:
  void _send(uint8_t* d, size_t len) {
    if (_dirPin>=0) digitalWrite(_dirPin, HIGH);
    _serial->write(d, len);
    _serial->flush();
    if (_dirPin>=0) { delayMicroseconds(20); digitalWrite(_dirPin, LOW); }
  }
};

static SCSerialBus g_bus;

// ============================================================
//  Angle ↔ Steps
// ============================================================
inline int32_t angleToSteps(float rad, float ratio) {
  return (int32_t)roundf(rad * ratio * (SERVO_STEPS_PER_REV / (2.0f * M_PI)));
}

inline float stepsToAngle(int32_t steps, float ratio) {
  return (float)steps * (2.0f * M_PI / SERVO_STEPS_PER_REV) / ratio;
}

// ============================================================
//  SAFETY — Emergency Stop
//  Cuts all torque instantly. Sets fault latch.
//  Only cleared by explicit fault_reset command.
// ============================================================
void emergencyStop(const String& reason) {
  if (g_faultActive) return;   // already stopped — avoid spam
  g_faultActive = true;
  g_faultReason = reason;

  // Cut torque on all servos as fast as possible (no delay between)
  uint8_t ids[] = {SERVO_BASE, SERVO_SHOULDER, SERVO_ELBOW,
                   SERVO_FOREARM, SERVO_GRIPPER};
  for (uint8_t id : ids) {
    g_bus.writeByte(id, REG_TORQUE_ENABLE, 0);
  }

  Serial.println(F("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
  Serial.println(F("  EMERGENCY STOP"));
  Serial.print  (F("  Reason: "));
  Serial.println(g_faultReason);
  Serial.println(F("  All torque cut. Send fault_reset to recover."));
  Serial.println(F("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"));
}

// ============================================================
//  SAFETY — Monitor (call from tickServers)
//  Samples ONE servo per call in round-robin so it doesn't
//  stall the bus. At SAFETY_MONITOR_MS=150ms and 5 servos,
//  each servo is checked every 750ms.
// ============================================================
void runSafetyMonitor() {
  if (g_faultActive) return;

  static uint8_t  idx    = 0;
  static uint32_t lastMs = 0;

  if (millis() - lastMs < SAFETY_MONITOR_MS) return;
  lastMs = millis();

  static const uint8_t ids[] = {
    SERVO_BASE, SERVO_SHOULDER, SERVO_ELBOW, SERVO_FOREARM, SERVO_GRIPPER
  };
  uint8_t id = ids[idx];

  // ── Temperature ──────────────────────────────────────────
  int16_t temp = g_bus.readByte(id, REG_PRESENT_TEMP);
  if (temp >= 0 && temp > SAFETY_TEMP_MAX_C) {
    emergencyStop("Servo ID" + String(id) +
                  " temp " + String(temp) + "°C > limit " +
                  String(SAFETY_TEMP_MAX_C) + "°C");
    return;
  }

  // ── Load / torque (bits[9:0] = magnitude 0–1000) ─────────
  int16_t loadRaw = g_bus.readWord(id, REG_PRESENT_LOAD);
  if (loadRaw >= 0) {
    uint16_t loadMag = (uint16_t)loadRaw & 0x03FF;  // strip direction bit
    if (loadMag > SAFETY_LOAD_MAX) {
      s_loadStrikes[idx]++;
      if (s_loadStrikes[idx] >= SAFETY_LOAD_STRIKES) {
        emergencyStop("Servo ID" + String(id) +
                      " load " + String(loadMag) + " > limit " +
                      String(SAFETY_LOAD_MAX) + " for " +
                      String(SAFETY_LOAD_STRIKES) + " polls");
        return;
      }
      // Not yet tripped — warn
      Serial.printf("[Safety] ⚠ Servo ID%d load=%d (strike %d/%d)\n",
                    id, loadMag, s_loadStrikes[idx], SAFETY_LOAD_STRIKES);
    } else {
      s_loadStrikes[idx] = 0;   // clear strikes on normal reading
    }
  }

  // ── Voltage (check on one servo only — supply is shared) ─
  // Only poll voltage on the base servo to avoid redundant reads.
  if (idx == 0) {
    int16_t volt = g_bus.readByte(id, REG_PRESENT_VOLTAGE);
    if (volt >= 0) {
      if (volt < SAFETY_VOLTAGE_MIN_DV) {
        emergencyStop("Supply voltage " + String(volt * 0.1f, 1) +
                      "V below minimum " +
                      String(SAFETY_VOLTAGE_MIN_DV * 0.1f, 1) + "V");
        return;
      }
      if (volt > SAFETY_VOLTAGE_MAX_DV) {
        emergencyStop("Supply voltage " + String(volt * 0.1f, 1) +
                      "V above maximum " +
                      String(SAFETY_VOLTAGE_MAX_DV * 0.1f, 1) + "V");
        return;
      }
    }
  }

  idx = (idx + 1) % 5;
}

// ============================================================
//  clearFault — called by fault_reset command in main.ino.
//  Re-enables torque and resets strike counters.
//  The FSM should command HOME immediately after.
// ============================================================
void clearFault() {
  g_faultActive = false;
  g_faultReason = "";
  for (uint8_t i = 0; i < 5; i++) s_loadStrikes[i] = 0;

  uint8_t ids[] = {SERVO_BASE, SERVO_SHOULDER, SERVO_ELBOW,
                   SERVO_FOREARM, SERVO_GRIPPER};
  for (uint8_t id : ids) {
    g_bus.writeWord(id, REG_TORQUE_LIMIT,  DEFAULT_TORQUE);
    g_bus.writeByte(id, REG_TORQUE_ENABLE, 1);
    delay(5);
  }
  Serial.println(F("[Safety] Fault cleared — torque re-enabled. Moving to HOME."));
}

// ============================================================
//  Public servo API
// ============================================================

// Apply acceleration register to all arm joints.
// Called in servoInit() and whenever g_safetyScale changes.
static void applyAccel() {
  uint8_t accel = safeAccel();
  uint8_t ids[] = {SERVO_BASE, SERVO_SHOULDER, SERVO_ELBOW,
                   SERVO_FOREARM, SERVO_GRIPPER};
  for (uint8_t id : ids) {
    g_bus.writeByte(id, REG_GOAL_ACCEL, accel);
    delay(2);
  }
  Serial.printf("[Servo] Scale=%.0f%%  speed×%.2f  accel=%d steps/ms²\n",
                g_safetyScale * 100.0f, g_safetyScale, accel);
}

void servoInit() {
  Serial2.begin(SERVO_BAUD, SERIAL_8N1, PIN_SERVO_RX, PIN_SERVO_TX);
  g_bus.begin(&Serial2, PIN_SERVO_DIR);
  delay(150);

  uint8_t ids[] = {SERVO_BASE, SERVO_SHOULDER, SERVO_ELBOW,
                   SERVO_FOREARM, SERVO_GRIPPER};

  for (uint8_t id : ids) { g_bus.writeByte(id, REG_TORQUE_ENABLE, 0); delay(8); }

  for (uint8_t id : ids) {
    g_bus.writeByte(id, REG_OPERATING_MODE, (id == SERVO_GRIPPER) ? 0 : 3);
    delay(8);
  }

  for (uint8_t id : ids) {
    g_bus.writeWord(id, REG_TORQUE_LIMIT,  DEFAULT_TORQUE);
    g_bus.writeByte(id, REG_TORQUE_ENABLE, 1);
    delay(8);
  }

  // Apply initial acceleration limit
  applyAccel();

  Serial.println(F("[Servo] Init OK — extended mode + safety limits active"));
  Serial.println(F("[Servo] *** ARM MUST BE AT MECHANICAL HOME BEFORE USE ***"));
  Serial.printf ("[Servo] Safety scale: %.0f%% (send {\"type\":\"safety\",\"scale\":x} to change)\n",
                 g_safetyScale * 100.0f);
}

// Move all four arm joints simultaneously.
// Speed is scaled by g_safetyScale automatically.
void armMoveToAngles(const JointAngles& a, uint16_t speed) {
  if (g_faultActive) {
    Serial.println(F("[Servo] Motion blocked — fault active"));
    return;
  }
  uint16_t s = safeSpeed(speed);
  struct { uint8_t id; float ang; float ratio; } j[] = {
    {SERVO_BASE,     a.base,     RATIO_BASE},
    {SERVO_SHOULDER, a.shoulder, RATIO_SHOULDER},
    {SERVO_ELBOW,    a.elbow,    RATIO_ELBOW},
    {SERVO_FOREARM,  a.forearm,  RATIO_FOREARM},
  };
  for (auto& jt : j) {
    g_bus.writeWord (jt.id, REG_GOAL_SPEED,    s);
    g_bus.writeDWord(jt.id, REG_GOAL_POSITION, angleToSteps(jt.ang, jt.ratio));
  }
}

// Move a single joint by ID, angle in link-frame degrees.
void setJointDeg(uint8_t id, float deg, uint16_t speed) {
  if (g_faultActive) return;
  uint16_t s = safeSpeed(speed);
  float ratio = RATIO_GRIPPER;
  switch(id) {
    case SERVO_BASE:     ratio = RATIO_BASE;     break;
    case SERVO_SHOULDER: ratio = RATIO_SHOULDER; break;
    case SERVO_ELBOW:    ratio = RATIO_ELBOW;    break;
    case SERVO_FOREARM:  ratio = RATIO_FOREARM;  break;
  }
  g_bus.writeWord(id, REG_GOAL_SPEED, s);
  if (id == SERVO_GRIPPER) {
    int16_t steps = (int16_t)constrain(
      SERVO_STEPS_CENTRE + (int32_t)roundf(
        (deg * M_PI / 180.0f) * (SERVO_STEPS_PER_REV / (2.0f * M_PI))),
      0, SERVO_STEPS_PER_REV - 1);
    g_bus.writeWord(id, REG_GOAL_POSITION, (uint16_t)steps);
  } else {
    g_bus.writeDWord(id, REG_GOAL_POSITION,
                     angleToSteps(deg * M_PI / 180.0f, ratio));
  }
}

void gripperOpen() {
  if (g_faultActive) return;
  g_bus.writeWord(SERVO_GRIPPER, REG_TORQUE_LIMIT, GRIP_TORQUE);
  g_bus.writeWord(SERVO_GRIPPER, REG_GOAL_SPEED,   safeSpeed(DEFAULT_SPEED));
  setJointDeg(SERVO_GRIPPER, GRIPPER_MAX_DEG, DEFAULT_SPEED);
}

void gripperClose() {
  if (g_faultActive) return;
  g_bus.writeWord(SERVO_GRIPPER, REG_TORQUE_LIMIT, GRIP_TORQUE);
  setJointDeg(SERVO_GRIPPER, GRIPPER_MIN_DEG, DEFAULT_SPEED / 2);
}

// [FIX 4] REG_PRESENT_LOAD is now 0x3C (real load register).
// Returns load magnitude 0–1000. Bit 10 (direction) is stripped.
int16_t gripperLoad() {
  int16_t raw = g_bus.readWord(SERVO_GRIPPER, REG_PRESENT_LOAD);
  if (raw < 0) return 0;
  return (int16_t)(raw & 0x03FF);  // magnitude only
}

void armGoHome() {
  JointAngles home;
  home.base     = HOME_BASE_DEG     * (M_PI / 180.0f);
  home.shoulder = HOME_SHOULDER_DEG * (M_PI / 180.0f);
  home.elbow    = HOME_ELBOW_DEG    * (M_PI / 180.0f);
  home.forearm  = HOME_FOREARM_DEG  * (M_PI / 180.0f);
  armMoveToAngles(home, HOME_SPEED);
  setJointDeg(SERVO_GRIPPER, HOME_GRIPPER_DEG, HOME_SPEED);
}

// Block until all joints settle or timeout.
// Checks g_faultActive each poll — returns false immediately on fault.
// tickFn: called each poll cycle (use to service WiFi/WS/safety monitor).
bool waitForMotionComplete(uint32_t timeout_ms, int16_t tol = 8,
                           void (*tickFn)() = nullptr) {
  uint8_t ids[] = {SERVO_BASE, SERVO_SHOULDER, SERVO_ELBOW,
                   SERVO_FOREARM, SERVO_GRIPPER};
  uint32_t t0 = millis();
  while (millis() - t0 < timeout_ms) {
    if (g_faultActive) return false;   // abort immediately on fault
    bool allDone = true;
    for (uint8_t id : ids) {
      int16_t spd = g_bus.readWord(id, REG_PRESENT_SPEED);
      if (abs(spd) > tol) { allDone = false; break; }
    }
    if (allDone) return true;
    if (tickFn) tickFn();
    delay(POLL_INTERVAL_MS);
  }
  Serial.println(F("[Servo] Motion timeout"));
  return false;
}

// [FIX 3] Gripper is Mode 0 (16-bit). Use readWord/writeWord.
void gripperCloseToContact(int16_t contactLoad = 200) {
  if (g_faultActive) return;
  g_bus.writeWord(SERVO_GRIPPER, REG_TORQUE_LIMIT, GRIP_TORQUE);
  g_bus.writeWord(SERVO_GRIPPER, REG_GOAL_SPEED,   safeSpeed(300));
  setJointDeg(SERVO_GRIPPER, GRIPPER_MIN_DEG, 300);

  uint32_t t0 = millis();
  while (millis() - t0 < 3000) {
    if (g_faultActive) return;
    int16_t load = gripperLoad();
    if (load > contactLoad) {
      int16_t pos = g_bus.readWord(SERVO_GRIPPER, REG_PRESENT_POS);
      if (pos >= 0 && pos < SERVO_STEPS_PER_REV) {
        g_bus.writeWord(SERVO_GRIPPER, REG_GOAL_POSITION, (uint16_t)pos);
      }
      Serial.printf("[Gripper] Contact at load=%d — holding\n", load);
      return;
    }
    delay(20);
  }
  Serial.println(F("[Gripper] No contact detected — fully closed"));
}

void printCurrentPositions() {
  const char* names[] = {"Base","Shoulder","Elbow","Forearm","Gripper"};
  uint8_t ids[]  = {SERVO_BASE,SERVO_SHOULDER,SERVO_ELBOW,
                    SERVO_FOREARM,SERVO_GRIPPER};
  float ratios[] = {RATIO_BASE,RATIO_SHOULDER,RATIO_ELBOW,
                    RATIO_FOREARM,RATIO_GRIPPER};
  int16_t volt   = g_bus.readByte(SERVO_BASE, REG_PRESENT_VOLTAGE);
  int16_t temp   = g_bus.readByte(SERVO_BASE, REG_PRESENT_TEMP);
  Serial.printf("  [Safety] scale=%.0f%%  fault=%s  volt=%.1fV  base_temp=%d°C\n",
                g_safetyScale*100.0f, g_faultActive?"YES":"no",
                volt>=0 ? volt*0.1f : -1.0f, temp);
  for (int i = 0; i < 5; i++) {
    if (i < 4) {
      int32_t s = g_bus.readDWord(ids[i], REG_PRESENT_POS);
      int16_t  t = g_bus.readByte(ids[i],  REG_PRESENT_TEMP);
      int16_t  l = g_bus.readWord(ids[i],  REG_PRESENT_LOAD);
      uint16_t lm = (l >= 0) ? (l & 0x03FF) : 0;
      if (s == INT32_MIN)
        Serial.printf("  %s (ID%d): READ ERR\n", names[i], ids[i]);
      else
        Serial.printf("  %s (ID%d): %.1f°  temp=%d°C  load=%d\n",
                      names[i], ids[i],
                      stepsToAngle(s, ratios[i]) * 180.0f / M_PI,
                      t, lm);
    } else {
      int16_t s = g_bus.readWord(ids[i], REG_PRESENT_POS);
      int16_t l = g_bus.readWord(ids[i], REG_PRESENT_LOAD);
      uint16_t lm = (l >= 0) ? (l & 0x03FF) : 0;
      if (s < 0) Serial.printf("  %s (ID%d): READ ERR\n", names[i], ids[i]);
      else        Serial.printf("  %s (ID%d): %d steps  load=%d\n",
                                names[i], ids[i], s, lm);
    }
  }
}
