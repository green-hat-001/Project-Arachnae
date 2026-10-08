#include "Arduino.h"
uint64_t g_simUs = 0;
SerialUSB Serial;
SimBus g_sim;
HardwareSerial Serial1(1);
HardwareSerial Serial2(2);

void sim_advance_us(uint64_t us) {
  // integrate in <=5 ms slices
  while (us > 0) {
    uint64_t s = std::min<uint64_t>(us, 5000);
    for (int i = 1; i <= 5; i++) if (g_sim.servo[i].present) g_sim.servo[i].step(s / 1e6);
    g_simUs += s; us -= s;
  }
}

static uint8_t chk(const uint8_t* p, int from, int to) { uint8_t s = 0; for (int i = from; i < to; i++) s += p[i]; return ~s; }

size_t HardwareSerial::write(const uint8_t* d, size_t n) {
  sim_advance_us(n * 10);                       // 1 Mbaud, 10 bits/byte
  if (g_sim.echo) rx.insert(rx.end(), d, d + n);
  if (n < 6 || d[0] != 0xFF || d[1] != 0xFF) return n;
  if (!begun) { g_sim.log.push_back("write on a UART that was never begin()'d"); return n; }
  if (g_sim.junctionLimit > 0) {
    int wired = 0; for (int i = 1; i <= 5; i++) if (g_sim.servo[i].present && g_sim.servo[i].port == port) wired++;
    if (wired > g_sim.junctionLimit) return n;   // overloaded junction: no servo decodes the packet
  }
  uint8_t id = d[2], len = d[3], instr = d[4];
  if (chk(d, 2, n - 1) != d[n - 1]) { g_sim.log.push_back("BAD CHECKSUM from firmware"); return n; }
  auto handle = [&](SimServo& sv) {
    if (instr == 0x03) {                         // WRITE
      g_sim.writes++;
      uint8_t reg = d[5]; int nd = len - 3;
      bool wasTorque = sv.torque;
      for (int i = 0; i < nd; i++) sv.reg[reg + i] = d[6 + i];
      if (reg <= 0x05 && reg + nd > 0x05) sv.id = sv.reg[0x05];      // ID change (models RAM effect)
      if (reg <= 0x28 && reg + nd > 0x28) sv.torque = sv.reg[0x28] & 1;
      if (reg == 0x21 && sv.torque) sv.modeWritesWithTorqueOn++;
      if (reg == 0x28 && (sv.reg[0x28] & 1)) sv.torque = true;
      if (wasTorque && !sv.torque) sv.dropTorque();
      if (id != 0xFE) {                          // status reply
        uint8_t r[6] = {0xFF, 0xFF, sv.id, 2, 0, 0}; r[5] = chk(r, 2, 5);
        rx.insert(rx.end(), r, r + 6);
      }
    } else if (instr == 0x02) {                  // READ
      g_sim.reads++;
      if (g_sim.dropAll) return;
      if (g_sim.dropReads > 0) { g_sim.dropReads--; return; }
      uint8_t reg = d[5], nb = d[6];
      uint8_t r[24] = {0xFF, 0xFF, sv.id, (uint8_t)(2 + nb), 0};
      for (int i = 0; i < nb; i++) {
        uint8_t v;
        if (reg + i == 0x38) v = sv.withinRev() & 0xFF;
        else if (reg + i == 0x39) v = sv.withinRev() >> 8;
        else v = sv.reg[reg + i];
        r[5 + i] = v;
      }
      r[5 + nb] = chk(r, 2, 5 + nb);
      if (g_sim.corruptReads > 0) { g_sim.corruptReads--; r[5] ^= 0x5A; }
      else if (g_sim.foreignReads > 0) { g_sim.foreignReads--; r[2] = (uint8_t)(sv.id + 1); r[5 + nb] = chk(r, 2, 5 + nb); }
      rx.insert(rx.end(), r, r + 6 + nb);
    }
  };
  auto onPort = [&](const SimServo& sv) { return sv.present && sv.port == port; };
  int matches = 0; for (int i = 1; i <= 5; i++) if (onPort(g_sim.servo[i]) && g_sim.servo[i].id == id) matches++;
  if (instr == 0x02 && id != 0xFE && matches > 1) {           // two servos answer at once -> garbled frame
    uint8_t junk[8] = {0xFF, 0xFF, id, 4, 0x00, 0x12, 0x34, 0x00};   // wrong checksum
    rx.insert(rx.end(), junk, junk + 8);
    return n;
  }
  if (id == 0xFE) { for (int i = 1; i <= 5; i++) if (onPort(g_sim.servo[i])) handle(g_sim.servo[i]); }
  else { for (int i = 1; i <= 5; i++) if (onPort(g_sim.servo[i]) && g_sim.servo[i].id == id) handle(g_sim.servo[i]); }
  return n;
}
