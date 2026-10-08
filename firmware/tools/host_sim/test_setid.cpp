#include "../../servo_setid/setid.cpp"
static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { fails++; printf("FAIL: %s\n", m); } else printf("ok: %s\n", m); } while (0)
static void cmd(const char* c) { char b[40]; strncpy(b, c, 39); b[39] = 0; handle(b); }
int main() {
  Serial.quiet = false;
  // A: lone gripper servo (factory ID 1) -> ID 5
  for (int i = 2; i <= 5; i++) g_sim.servo[i].present = false;
  g_sim.servo[1].present = true;
  g_sim.servo[1].torque = true;
  setup();
  g_sim.writes = 0;
  cmd("go");
  CHECK(g_sim.servo[1].id == 5 && g_sim.servo[1].reg[0x05] == 5, "servo renamed to 5");
  CHECK(g_sim.servo[1].reg[0x37] == 1, "EEPROM re-locked after write");
  CHECK(!g_sim.servo[1].torque, "torque was off during the write");
  cmd("scan"); CHECK(g_found[0] == 5, "scan finds ID 5");
  cmd("cfg5"); CHECK(g_sim.servo[1].reg[0x21] == 0 && g_sim.servo[1].w(0x22) == 350, "cfg5 applied");
  // B: 'go' again -> refuses (no ID 1 on the bus)
  long w = g_sim.writes; cmd("go"); CHECK(g_sim.writes == w, "second 'go' (already ID 5) writes nothing");
  // C: two servos both ID 1 on the bus -> NOTHING is written
  g_sim.servo[1].setId(1); g_sim.servo[2].present = true; g_sim.servo[2].setId(1);
  w = g_sim.writes; cmd("go");
  CHECK(g_sim.writes == w, "two servos on the bus: refuses to write");
  // D: the arm's base servo (ID1) plus a gripper already at 5: 'go' must refuse (2 servos)
  g_sim.servo[2].setId(5); w = g_sim.writes; cmd("go");
  CHECK(g_sim.writes == w, "servos 1 and 5 both present: refuses");
  // E: no-echo bus (Waveshare board)
  g_sim.servo[2].present = false; g_sim.servo[1].setId(1); g_sim.echo = false;
  cmd("go"); CHECK(g_sim.servo[1].id == 5, "works on a bus without echo");
  printf("%s\n", fails ? "SETID TESTS FAILED" : "SETID TESTS PASSED");
  return fails;
}
