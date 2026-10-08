// ============================================================
//  ARACHNAE — one-shot servo ID changer (XIAO ESP32S3)
//  PlatformIO env:  servo_setid     (pio run -e servo_setid -t upload)
//
//  Default job: the GRIPPER servo is still ID 1 -> make it ID 5.
//
//  !!  CONNECT ONLY THE SERVO YOU ARE RENAMING  !!
//  Every fresh servo is ID 1. If the base servo (also ID 1) is on the
//  bus at the same time they answer together and you corrupt the bus —
//  this tool refuses to write unless exactly ONE servo answers.
//
//  Serial 115200. Commands (type + Enter):
//    scan            list every ID 1-20 that answers
//    go              change ID 1 -> 5 (the default gripper job)
//    set <old> <new> change any ID (still needs exactly one servo on the bus)
//    cfg5            gripper settings on ID 5: Mode 0, torque limit 350
//    off / on        torque off / on for the single servo found
//    help
//
//  After 'go': POWER-CYCLE the servo and run 'scan' again — it must
//  still answer as ID 5. That proves the ID is in EEPROM, not just RAM.
// ============================================================
#include <Arduino.h>

#define PIN_SERVO_RX   D7
#define PIN_SERVO_TX   D6
#define SERVO_BAUD     1000000
#define MAX_SCAN_ID    20
#define OLD_ID_DEFAULT 1
#define NEW_ID_DEFAULT 5
#define GRIP_TORQUE    350

#define REG_ID            0x05
#define REG_MODE          0x21
#define REG_TORQUE_LIMIT  0x22
#define REG_TORQUE_EN     0x28
#define REG_LOCK          0x37
#define REG_PRESENT_POS   0x38

HardwareSerial& bus = Serial1;

// ── bus layer (echo-aware: works on a resistor-junction bus and on the Waveshare board) ──
static int parseReply(const uint8_t* buf, size_t cnt, const uint8_t* pkt, size_t n,
                      uint8_t id, uint8_t nd, uint8_t* out) {
  size_t off = 0;
  if (cnt >= n && memcmp(buf, pkt, n) == 0) off = n;
  else if (cnt < n && memcmp(buf, pkt, cnt) == 0) return 0;
  size_t need = 6 + nd;
  if (cnt - off < need) return 0;
  const uint8_t* f = buf + off;
  if (f[0] != 0xFF || f[1] != 0xFF || f[2] != id || f[3] != 2 + nd) return -1;
  uint8_t s = 0;
  for (size_t i = 2; i < need - 1; i++) s += f[i];
  if ((uint8_t)~s != f[need - 1]) return -1;
  if (nd) memcpy(out, f + 5, nd);
  return 1;
}

static size_t buildPkt(uint8_t* pkt, uint8_t id, uint8_t instr, const uint8_t* p, uint8_t np) {
  size_t n = 0;
  pkt[n++] = 0xFF; pkt[n++] = 0xFF; pkt[n++] = id;
  pkt[n++] = np + 2; pkt[n++] = instr;
  for (uint8_t i = 0; i < np; i++) pkt[n++] = p[i];
  uint8_t s = 0;
  for (size_t i = 2; i < n; i++) s += pkt[i];
  pkt[n++] = ~s;
  return n;
}

static void drain() { while (bus.available()) bus.read(); }

static bool rd(uint8_t id, uint8_t reg, uint8_t nBytes, uint16_t& v) {
  uint8_t p[2] = {reg, nBytes}, pkt[16], buf[40], d[2] = {0, 0};
  size_t n = buildPkt(pkt, id, 0x02, p, 2), cnt = 0;
  for (int attempt = 0; attempt < 2; attempt++) {           // one retry
    cnt = 0;
    drain();
    bus.write(pkt, n); bus.flush();
    uint32_t t0 = millis();
    while (millis() - t0 < 12 && cnt < sizeof(buf)) {
      while (bus.available() && cnt < sizeof(buf)) buf[cnt++] = bus.read();
      int r = parseReply(buf, cnt, pkt, n, id, nBytes, d);
      if (r == 1) { v = d[0] | (nBytes == 2 ? (d[1] << 8) : 0); return true; }
      if (r == -1) break;
    }
  }
  return false;
}

static void wr8(uint8_t id, uint8_t reg, uint8_t v) {
  uint8_t p[2] = {reg, v}, pkt[16];
  size_t n = buildPkt(pkt, id, 0x03, p, 2);
  drain();
  bus.write(pkt, n); bus.flush();
  delay(4);                                                 // echo + status reply, then discard
  drain();
}
static void wr16(uint8_t id, uint8_t reg, uint16_t v) {
  uint8_t p[3] = {reg, (uint8_t)(v & 0xFF), (uint8_t)(v >> 8)}, pkt[16];
  size_t n = buildPkt(pkt, id, 0x03, p, 3);
  drain();
  bus.write(pkt, n); bus.flush();
  delay(4);
  drain();
}

// ── helpers ──────────────────────────────────────────────────
static uint8_t g_found[MAX_SCAN_ID + 1];
static int scanBus(bool verbose) {
  int n = 0;
  if (verbose) Serial.println(F("--- scan IDs 1-20 ---"));
  for (uint8_t id = 1; id <= MAX_SCAN_ID; id++) {
    uint16_t pos;
    if (rd(id, REG_PRESENT_POS, 2, pos)) {
      g_found[n++] = id;
      if (verbose) Serial.printf("  ID %u answers (position %u)\n", id, pos);
    }
  }
  if (verbose) Serial.printf("--- %d servo(s) found ---\n", n);
  return n;
}

static bool answers(uint8_t id) { uint16_t p; return rd(id, REG_PRESENT_POS, 2, p); }

static void changeId(uint8_t oldId, uint8_t newId) {
  if (oldId < 1 || oldId > 253 || newId < 1 || newId > 253) { Serial.println(F("IDs must be 1-253")); return; }
  if (oldId == newId) { Serial.println(F("old and new are the same — nothing to do")); return; }
  int n = scanBus(true);
  if (n == 0) {
    Serial.println(F("ABORT: no clean answer. Either nothing is connected/powered (check 12V/battery, data wire, TX/RX),"));
    Serial.println(F("       or TWO servos share the ID and are garbling each other. Leave only ONE servo on the bus."));
    return;
  }
  if (n > 1)  { Serial.println(F("ABORT: more than one servo on the bus. Disconnect every servo except the one you are renaming.")); return; }
  if (g_found[0] == newId) { Serial.printf("The servo is already ID %u — nothing to do.\n", newId); return; }
  if (g_found[0] != oldId) {
    Serial.printf("ABORT: the servo on the bus is ID %u, not %u. Use:  set %u %u\n", g_found[0], oldId, g_found[0], newId);
    return;
  }
  // Two servos with the SAME id and similar data can still look like one. Demand a perfectly
  // clean run of reads: overlapping replies from two different servos fail their checksums.
  for (int i = 0; i < 12; i++) {
    uint16_t p, v;
    if (!rd(oldId, REG_PRESENT_POS, 2, p) || !rd(oldId, 0x3E, 1, v)) {
      Serial.println(F("ABORT: bus is not clean (a read failed). Two servos on the same ID? Loose data wire? Nothing written."));
      return;
    }
  }

  Serial.printf("Changing ID %u -> %u ...\n", oldId, newId);
  wr8(oldId, REG_TORQUE_EN, 0);        // EEPROM writes need torque off
  delay(20);
  wr8(oldId, REG_LOCK, 0);             // unlock EEPROM
  delay(20);
  wr8(oldId, REG_ID, newId);           // write the new ID (servo answers as newId from now on)
  delay(150);
  wr8(newId, REG_LOCK, 1);             // re-lock -> commits to EEPROM
  delay(150);

  bool newOk = answers(newId);
  bool oldGone = !answers(oldId);
  uint16_t back = 0;
  bool idReg = rd(newId, REG_ID, 1, back) && back == newId;
  Serial.printf("  answers as ID %u : %s\n", newId, newOk ? "YES" : "NO");
  Serial.printf("  old ID %u silent : %s\n", oldId, oldGone ? "YES" : "NO");
  Serial.printf("  ID register reads back %u : %s\n", back, idReg ? "OK" : "MISMATCH");
  if (newOk && oldGone && idReg) {
    Serial.println(F("\nSUCCESS. Now power-cycle the servo and type 'scan' — it must still be the new ID."));
  } else {
    Serial.println(F("\nNOT CONFIRMED. Type 'scan' to see what the servo answers as; power-cycle and try again if needed."));
  }
}

static bool singleServo(uint8_t& id) {
  int n = scanBus(false);
  if (n != 1) { Serial.printf("need exactly one servo on the bus (found %d)\n", n); return false; }
  id = g_found[0];
  return true;
}

static void handle(char* l) {
  int a, b;
  if (!strcmp(l, "scan")) scanBus(true);
  else if (!strcmp(l, "go")) changeId(OLD_ID_DEFAULT, NEW_ID_DEFAULT);
  else if (sscanf(l, "set %d %d", &a, &b) == 2) changeId((uint8_t)a, (uint8_t)b);
  else if (!strcmp(l, "cfg5")) {
    if (!answers(5)) { Serial.println(F("no servo answers as ID 5 — run 'go' first")); return; }
    wr8(5, REG_TORQUE_EN, 0); delay(20);
    wr8(5, REG_LOCK, 0);      delay(20);
    wr8(5, REG_MODE, 0);      delay(20);
    wr16(5, REG_TORQUE_LIMIT, GRIP_TORQUE); delay(20);
    wr8(5, REG_LOCK, 1);      delay(50);
    uint16_t m = 99, t = 0;
    rd(5, REG_MODE, 1, m); rd(5, REG_TORQUE_LIMIT, 2, t);
    Serial.printf("ID 5: mode=%u (want 0)  torque limit=%u (want %u)\n", m, t, GRIP_TORQUE);
  }
  else if (!strcmp(l, "off")) { uint8_t id; if (singleServo(id)) { wr8(id, REG_TORQUE_EN, 0); Serial.printf("ID %u torque off\n", id); } }
  else if (!strcmp(l, "on"))  { uint8_t id; if (singleServo(id)) { uint16_t p; if (rd(id, REG_PRESENT_POS, 2, p)) { wr16(id, 0x2A, p); } wr8(id, REG_TORQUE_EN, 1); Serial.printf("ID %u torque on (holding here)\n", id); } }
  else Serial.println(F("cmds: scan | go (1->5) | set <old> <new> | cfg5 | off | on"));
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);
  bus.begin(SERVO_BAUD, SERIAL_8N1, PIN_SERVO_RX, PIN_SERVO_TX);
  delay(200);
  Serial.println(F("\n=== ARACHNAE servo ID changer ==="));
  Serial.println(F("Connect ONLY the servo you are renaming. Commands: scan | go | set <old> <new> | cfg5"));
  scanBus(true);
}

void loop() {
  static char buf[32]; static uint8_t len = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') { if (len) { buf[len] = 0; handle(buf); len = 0; } }
    else if (len < sizeof(buf) - 1) buf[len++] = c;
  }
  delay(2);
}
