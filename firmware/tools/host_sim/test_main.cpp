#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "../../Final_Firmware_Main/Final_Firmware_Main.cpp"

static int g_pass = 0, g_fail = 0;
#define CHECK(c, ...) do { if (c) { g_pass++; } else { g_fail++; printf("  FAIL line %d: %s  ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define SECTION(n) printf("\n== %s ==\n", n)

static void runMs(uint32_t ms) { uint64_t end = g_simUs + (uint64_t)ms * 1000; while (g_simUs < end) loop(); }
static bool runUntilIdle(uint32_t maxMs) {
  uint64_t end = g_simUs + (uint64_t)maxMs * 1000;
  loop();
  while (g_simUs < end) { loop(); if (allJointsIdle()) { runMs(200); return true; } }
  return false;
}
static double phys(int i) { return g_sim.servo[i + 1].phys; }
static String post(const char* path, const char* body = "{}") { return g_teachServer.request(true, path, body); }
static String get(const char* path) { return g_teachServer.request(false, path); }
static bool ok(const String& r) { return r.find("\"ok\":true") != std::string::npos; }

// The old 1-start worm's one-rev base backlash. The firmware now defaults to
// none, but a big backlash is what exercises the reversal logic, so the tests
// keep it (setup() reloads it from the simulated NVS once saved).
static const int32_t TEST_BASE_BL = 4096;

static void resetGlobalsForReboot() {
  g_armed = false; g_faultActive = false; g_faultReason = ""; g_teachActive = false;
  g_autoMode = false; g_pendingState = -1; g_arm.fsm = GrabFSM::IDLE; g_safetyScale = SAFETY_SCALE_DEFAULT;
  g_env = RuntimeEnvelope(); g_moveMsg = ""; g_lastMoveOk = true; g_targetFresh = false;
  g_wheelRevBit = WHEEL_REVERSE_BIT; g_baseSpin = BaseSpin();
  for (int i = 0; i < 4; i++) { g_j[i].ratio = (i==0?RATIO_BASE:i==1?RATIO_SHOULDER:i==2?RATIO_ELBOW:RATIO_FOREARM); g_j[i].sign = 1;
                                g_j[i].bl = (i == 0) ? TEST_BASE_BL : 0; g_j[i].tq = (i == 0) ? BASE_TORQUE : DEFAULT_TORQUE; }
  g_sim.dropAll = false; g_sim.dropReads = 0; g_sim.junctionLimit = 0;
  for (int i = 1; i <= 4; i++) g_sim.servo[i].port = 1;
  g_sim.servo[5].port = GRIPPER_OWN_BUS ? 2 : 1;          // wired the way the firmware expects
  for (int i = 1; i <= 5; i++) { g_sim.servo[i].torque = false; g_sim.servo[i].handVel = 0; g_sim.servo[i].modeWritesWithTorqueOn = 0; g_sim.servo[i].wheelRevBit = 0x8000;
                                g_sim.servo[i].springBack = 0; g_sim.servo[i].holdDither = 0; }
  g_sim.servo[1].reg[0x21] = 0;
}
static double stepsFor(int j, double deg) { return deg * g_j[j].ratio * 4096.0 / 360.0; }
static int modeViolations() { int n = 0; for (int i = 1; i <= 5; i++) n += g_sim.servo[i].modeWritesWithTorqueOn; return n; }

static void bootWithPose(double b, double s, double e, double f, double g, int junctionLimit = 0, int gripPort = -1) {
  resetGlobalsForReboot();
  g_sim.junctionLimit = junctionLimit;
  if (gripPort > 0) g_sim.servo[5].port = gripPort;
  g_sim.servo[1].phys = b; g_sim.servo[2].phys = s; g_sim.servo[3].phys = e; g_sim.servo[4].phys = f; g_sim.servo[5].phys = g;
  g_sim.servo[1].reg[0x21] = 1; g_sim.servo[1].reg[0x2E] = 0;   // pretend base was left in wheel mode by a power cut
  setup();
}

int main() {
  // ----------------------------------------------------------------
  SECTION("1. boot: tracks encoders, holds in place, DISARMED, no motion");
  for (int e = 0; e < 2; e++) {
    g_sim.echo = (e == 0);
    printf("  (bus echo %s)\n", g_sim.echo ? "ON" : "OFF");
    bootWithPose(1000, 4090, 100, 2048, 2560);
    double p0[4]; for (int i = 0; i < 4; i++) p0[i] = phys(i);
    runMs(3000);
    for (int i = 0; i < 4; i++) CHECK(g_j[i].ok, "joint %d not tracked", i);
    for (int i = 1; i <= 5; i++) CHECK(g_sim.servo[i].torque, "servo %d torque off after boot", i);
    for (int i = 0; i < 4; i++) CHECK(std::fabs(phys(i) - p0[i]) < 1.0, "joint %d moved at boot by %.1f", i, phys(i) - p0[i]);
    CHECK(!g_armed, "should boot disarmed");
    CHECK(std::fabs(jDeg(g_j[1]) - HOME_SHOULDER_DEG) < 0.05, "boot pose should read as HOME angle, got %.2f", jDeg(g_j[1]));
    CHECK(g_sim.servo[1].reg[0x21] == 0, "base must be forced back to mode 0 at boot (was %d)", g_sim.servo[1].reg[0x21]);
    CHECK(g_sim.log.empty(), "bus checksum errors from firmware");
  }
  g_sim.echo = true;

  // ----------------------------------------------------------------
  SECTION("2. disarmed = no powered motion");
  bootWithPose(1000, 4090, 100, 2048, 2560);
  CHECK(!ok(post("/teach/jog", "{\"joint\":\"shoulder\",\"delta\":2}")), "jog accepted while disarmed");
  CHECK(!ok(post("/teach/start")), "teach start accepted while disarmed");
  CHECK(!ok(post("/teach/save", "{\"pos\":\"home\"}")), "save accepted while disarmed");
  JointAngles t = {0, 0, 0, 0};
  CHECK(!armMoveToAngles(t, 1000), "armMoveToAngles accepted while disarmed");
  String st = get("/teach/status");
  CHECK(st.find("\"armed\":false") != std::string::npos, "status: %s", st.c_str());

  // ----------------------------------------------------------------
  SECTION("3. arm + jog: small, wrap-crossing, backlash reversal (leapfrog)");
  CHECK(ok(post("/teach/arm", "{}")), "arm failed");
  double s0 = phys(1);
  CHECK(ok(post("/teach/jog", "{\"joint\":\"shoulder\",\"delta\":5}")), "jog refused");
  CHECK(runUntilIdle(120000), "jog never finished");
  double want = stepsFor(1, 5);
  CHECK(std::fabs((phys(1) - s0) - want) < 12, "shoulder moved %.1f steps, want %.1f (crosses wrap: started at 4090)", phys(1) - s0, want);
  CHECK(std::fabs(jDeg(g_j[1]) - (HOME_SHOULDER_DEG + 5)) < 0.3, "shoulder reads %.2f", jDeg(g_j[1]));
  // base + then - : reversal must eat the 1-rev backlash slack and leapfrog
  double b0 = phys(0);
  CHECK(ok(post("/teach/jog", "{\"joint\":\"base\",\"delta\":5}")), "base jog refused");
  CHECK(runUntilIdle(300000), "base + jog never finished");
  double bPlus = phys(0);
  CHECK(std::fabs((bPlus - b0) - stepsFor(0, 5)) < 12, "base + moved %.1f want %.1f", bPlus - b0, stepsFor(0, 5));
  CHECK(ok(post("/teach/jog", "{\"joint\":\"base\",\"delta\":-5}")), "base - jog refused");
  CHECK(runUntilIdle(600000), "base - jog never finished");
  double expected = bPlus - stepsFor(0, 5) - g_j[0].bl;   // 5 deg back AND one backlash rev
  CHECK(std::fabs(phys(0) - expected) < 12, "base - moved to %.1f want %.1f", phys(0), expected);
  CHECK(std::fabs(jDeg(g_j[0]) - 0.0) < 0.3, "base effective angle %.2f want 0", jDeg(g_j[0]));
  CHECK(modeViolations() == 0, "mode register written with torque ON %d times", modeViolations());
  CHECK(!g_faultActive, "unexpected fault: %s", g_faultReason.c_str());
  CHECK(g_sim.log.empty(), "bus errors");

  // ----------------------------------------------------------------
  SECTION("4. coordinated multi-joint move across many revolutions");
  {
    JointAngles a = { (float)radians(40), (float)radians(-20), (float)radians(80), (float)radians(-30) };
    double startAbs[4]; for (int i = 0; i < 4; i++) startAbs[i] = phys(i);
    CHECK(armMoveToAngles(a, 3000), "refused");
    // the four joints must all be moving at once
    runMs(500);
    int moving = 0; for (int i = 0; i < 4; i++) if (g_j[i].mv != JMove::HOLD) moving++;
    CHECK(moving == 4, "only %d joints moving", moving);
    CHECK(runUntilIdle(900000), "never finished");
    CHECK(g_lastMoveOk, "move reported failure: %s", g_moveMsg.c_str());
    float want[4] = {40, -20, 80, -30};
    for (int i = 0; i < 4; i++) CHECK(std::fabs(jDeg(g_j[i]) - want[i]) < 0.4, "joint %d = %.2f want %.1f", i, jDeg(g_j[i]), want[i]);
    // independent check against the simulator's true multi-turn position
    for (int i = 1; i < 4; i++) {
      double trueDeg = HOME_SHOULDER_DEG * 0; 
      double refDeg = g_j[i].refDeg;
      double d = refDeg + (phys(i) - g_j[i].zeroAbs) * 360.0 / (g_j[i].ratio * 4096.0);
      CHECK(std::fabs(d - want[i]) < 0.4, "TRUE joint %d angle %.2f want %.1f", i, d, want[i]); (void)trueDeg;
    }
    CHECK(modeViolations() == 0, "mode written with torque on");
    CHECK(!g_faultActive, "fault: %s", g_faultReason.c_str());
  }

  // ----------------------------------------------------------------
  SECTION("5. hard limits: commands are clamped, watchdog trips on overshoot");
  {
    JointAngles a = { (float)radians(500), 0, 0, 0 };
    CHECK(armMoveToAngles(a, 3000), "refused");
    CHECK(runUntilIdle(900000), "never finished");
    CHECK(jDeg(g_j[0]) <= BASE_MAX_DEG + 0.5f, "base went to %.1f (limit %.1f)", jDeg(g_j[0]), BASE_MAX_DEG);
    CHECK(!g_faultActive, "clamped move must not fault");
    // now physically shove the shoulder beyond its limit (a slipped belt / external force)
    g_sim.servo[2].pushVel = 12000;        // a slipping belt / external force, overpowers the servo
    runMs(4000);
    g_sim.servo[2].pushVel = 0;
    CHECK(g_faultActive, "watchdog did not trip");
    CHECK(g_faultReason.find("beyond hard limit") != std::string::npos, "reason: %s", g_faultReason.c_str());
    for (int i = 1; i <= 5; i++) CHECK(!g_sim.servo[i].torque, "servo %d still powered after E-STOP", i);
  }

  // ----------------------------------------------------------------
  SECTION("6. E-STOP: cuts torque, keeps tracking, reset HOLDS (no snap-back)");
  bootWithPose(1000, 4090, 100, 2048, 2560);
  post("/teach/arm");
  post("/teach/jog", "{\"joint\":\"elbow\",\"delta\":15}");
  runMs(1500);
  CHECK(g_j[2].mv != JMove::HOLD, "elbow should still be moving");
  post("/teach/estop");
  for (int i = 1; i <= 5; i++) CHECK(!g_sim.servo[i].torque, "servo %d still powered", i);
  CHECK(g_faultActive && allJointsIdle(), "state not latched");
  CHECK(!ok(post("/teach/jog", "{\"joint\":\"elbow\",\"delta\":1}")), "jog accepted during fault");
  // arm droops under gravity while unpowered: tracker must follow, even across a wrap
  for (int i = 1; i <= 4; i++) g_sim.servo[i].handVel = -400;
  runMs(6000);
  for (int i = 1; i <= 4; i++) g_sim.servo[i].handVel = 0;
  runMs(500);
  for (int i = 0; i < 4; i++) {
    double d = g_j[i].refDeg + (phys(i) - g_j[i].zeroAbs) * 360.0 / (g_j[i].ratio * 4096.0);
    CHECK(std::fabs(jDeg(g_j[i]) - d) < 0.3, "joint %d tracker %.2f vs truth %.2f after droop", i, jDeg(g_j[i]), d);
  }
  double before[4]; for (int i = 0; i < 4; i++) before[i] = phys(i);
  CHECK(ok(post("/teach/fault_reset")), "reset failed");
  runMs(5000);
  for (int i = 0; i < 4; i++) CHECK(std::fabs(phys(i) - before[i]) < 1.0, "joint %d moved %.1f steps after fault reset", i, phys(i) - before[i]);
  for (int i = 1; i <= 5; i++) CHECK(g_sim.servo[i].torque, "servo %d not re-powered", i);
  CHECK(!g_faultActive, "fault not cleared");

  // ----------------------------------------------------------------
  SECTION("7. teach: hand-move across many revs, save 7, lock without snap-back");
  static int32_t taughtEff[7][4];
  const char* names[7] = {"home", "fwd_reach", "handoff", "left_limit", "right_limit", "height_max", "depth_max"};
  {
    post("/teach/fault_reset");
    // Teach from HOME, so the poses below are the angles their comment says.
    JointAngles h = { (float)radians(HOME_BASE_DEG), (float)radians(HOME_SHOULDER_DEG), (float)radians(HOME_ELBOW_DEG), (float)radians(HOME_FOREARM_DEG) };
    CHECK(armMoveToAngles(h, 3000) && runUntilIdle(1500000), "move to HOME before teaching");
    CHECK(ok(post("/teach/start")), "teach start");
    for (int i = 1; i <= 5; i++) CHECK(!g_sim.servo[i].torque, "servo %d powered in teach", i);
    // cumulative hand moves in servo steps [base, shoulder, elbow, forearm],
    // at the default ratios. As angles from HOME (0, 30, -30, 0) the poses are
    //   fwd_reach (+60, 0, +20, 0)     reach out to the right
    //   handoff   (+80, -40, +40, -20)
    //   left/right_limit: base -20 / +125
    //   height_max (+120, 0, +30, 0)   straight arm, raised to the side: ~460 mm up
    //   depth_max  (0, -10, +30, 0)    straight arm hanging low: ~1000 mm down
    long d[7][4] = { {0,0,0,0}, {34133,0,1365,0}, {11378,-3034,1365,-512}, {-56889,0,0,0},
                     {82489,0,0,0}, {-2844,3034,-683,512}, {-68267,-759,0,0} };
    for (int k = 0; k < 7; k++) {
      for (int i = 0; i < 4; i++) g_sim.servo[i + 1].handVel = d[k][i] / 4.0;
      runMs(k == 0 ? 100 : 4000);
      for (int i = 1; i <= 4; i++) g_sim.servo[i].handVel = 0;
      runMs(300);
      String r = post("/teach/save", (String("{\"pos\":\"") + names[k] + "\"}").c_str());
      CHECK(ok(r), "save %s: %s", names[k], r.c_str());
      for (int i = 0; i < 4; i++) {
        taughtEff[k][i] = (int32_t)std::llround(phys(i) - g_j[i].zeroAbs);   // ground truth from the simulator
        CHECK(std::labs(g_taught.pose[k].eff[i] - taughtEff[k][i]) <= 2, "%s joint %d saved %ld truth %ld", names[k], i, (long)g_taught.pose[k].eff[i], (long)taughtEff[k][i]);
      }
    }
    String s = get("/teach/status");
    CHECK(s.find("\"calibrated\":true") != std::string::npos, "not calibrated: %s", s.c_str());
    double before[4]; for (int i = 0; i < 4; i++) before[i] = phys(i);
    double gpos = g_sim.servo[5].phys;
    CHECK(ok(post("/teach/done")), "done");
    runMs(6000);
    for (int i = 0; i < 4; i++) CHECK(std::fabs(phys(i) - before[i]) < 1.0, "joint %d snapped %.1f steps on Lock", i, phys(i) - before[i]);
    CHECK(std::fabs(g_sim.servo[5].phys - gpos) < 2, "gripper snapped");
    for (int i = 1; i <= 5; i++) CHECK(g_sim.servo[i].torque, "servo %d not powered after Lock", i);
    CHECK(modeViolations() == 0, "mode violations");
    CHECK(!g_faultActive, "fault after Lock: %s", g_faultReason.c_str());
  }

  // ----------------------------------------------------------------
  SECTION("8. reboot: taught data survives; every preview lands on the taught pose");
  {
    bootWithPose(1000, 4090, 100, 2048, 2560);      // arm placed back in its boot pose
    CHECK(g_taught.calibrated, "calibration lost across reboot");
    CHECK(g_sigWarn.length() == 0, "false boot-pose warning: %s", g_sigWarn.c_str());
    CHECK(g_env.rValid && g_env.zMaxValid && g_env.zMinValid, "envelope not rebuilt");
    CHECK(ok(post("/teach/arm")), "arm");
    for (int k = 0; k < 7; k++) {
      String r = post("/teach/preview", (String("{\"pos\":\"") + names[k] + "\"}").c_str());
      CHECK(ok(r), "preview %s refused: %s", names[k], r.c_str());
      CHECK(runUntilIdle(1500000), "preview %s never finished", names[k]);
      for (int i = 0; i < 4; i++)
        CHECK(std::labs(jEff(g_j[i]) - taughtEff[k][i]) <= 10, "%s joint %d at %ld want %ld", names[k], i, (long)jEff(g_j[i]), (long)taughtEff[k][i]);
      CHECK(!g_faultActive, "fault during %s: %s (%s)", names[k], g_faultReason.c_str(), g_moveMsg.c_str());
    }
    CHECK(modeViolations() == 0, "mode violations %d", modeViolations());
    CHECK(g_sim.log.empty(), "bus errors");
  }

  // ----------------------------------------------------------------
  SECTION("9. boot pose signature: wrong pose is flagged, needs explicit override");
  {
    bootWithPose(1000, 4090 + 1000, 100, 2048, 2560);       // shoulder found 1000 steps off
    CHECK(g_sigWarn.length() > 0, "no warning for a different boot pose");
    String r = post("/teach/arm");
    CHECK(!ok(r) && r.find("SIGNATURE") != std::string::npos, "arm without force must be refused: %s", r.c_str());
    CHECK(!g_armed, "armed anyway");
    CHECK(ok(post("/teach/arm", "{\"force\":true}")), "forced arm failed");
  }

  // ----------------------------------------------------------------
  SECTION("10. wheel direction: wrong bit is DETECTED, arm holds, flip fixes it");
  {
    bootWithPose(1000, 500, 100, 2048, 2560);
    post("/teach/arm", "{\"force\":true}");
    g_sim.servo[2].wheelRevBit = 0x0400;                       // servo really uses bit 10
    double s0 = phys(1);
    JointAngles a = { 0, (float)radians(-100), (float)radians(120), (float)radians(30) };   // negative move that must cross a revolution -> wheel mode, reverse
    CHECK(armMoveToAngles(a, 3000), "refused");
    runUntilIdle(300000);
    CHECK(g_moveMsg.find("WRONG WAY") != std::string::npos, "msg: '%s'", g_moveMsg.c_str());
    CHECK(!g_lastMoveOk, "reported success");
    CHECK(std::fabs(phys(1) - s0) < 400, "shoulder ran away %.0f steps in the wrong direction", phys(1) - s0);
    CHECK(g_j[1].mv == JMove::HOLD && g_sim.servo[2].torque, "shoulder should be holding, powered");
    CHECK(!g_faultActive, "a wrong-direction abort should not latch an E-STOP");
    CHECK(ok(post("/teach/cfg", "{\"revbit\":1}")), "flip failed");
    CHECK(g_wheelRevBit == 0x0400, "bit not flipped");
    s0 = phys(1);
    CHECK(armMoveToAngles(a, 3000), "refused after flip");
    CHECK(runUntilIdle(900000), "never finished");
    CHECK(g_lastMoveOk, "failed after flip: %s", g_moveMsg.c_str());
    CHECK(std::fabs(jDeg(g_j[1]) + 100.0) < 0.6, "shoulder %.2f want -100", jDeg(g_j[1]));
    post("/teach/cfg", "{\"revbit\":1}");           // restore the default in flash for the next sections
    CHECK(g_wheelRevBit == 0x8000, "did not flip back");
  }

  // ----------------------------------------------------------------
  SECTION("11. lost encoder feedback during a wheel leg -> E-STOP");
  {
    bootWithPose(1000, 4090, 100, 2048, 2560);
    post("/teach/arm");
    JointAngles a = { (float)radians(60), (float)radians(-60), (float)radians(120), (float)radians(30) };
    armMoveToAngles(a, 3000);
    runMs(3000);
    CHECK(g_j[0].mv == JMove::WHEEL, "base should be in a wheel leg");
    g_sim.dropAll = true;
    runMs(800);
    CHECK(g_faultActive && g_faultReason.find("Lost encoder") != std::string::npos, "no E-STOP: '%s'", g_faultReason.c_str());
    for (int i = 1; i <= 5; i++) CHECK(!g_sim.servo[i].torque, "servo %d powered", i);
    g_sim.dropAll = false;
  }

  // ----------------------------------------------------------------
  SECTION("12. taught envelope limits IK; body model still applies");
  {
    bootWithPose(1000, 4090, 100, 2048, 2560);
    post("/teach/arm");
    JointAngles out;
    printf("  envelope: r<=%.0f  z in [%.0f, %.0f]  base [%.1f, %.1f]\n", g_env.rMax, g_env.zMin, g_env.zMax, g_env.baseMinDeg, g_env.baseMaxDeg);
    IKResult far = solveIK(1200, 0, 0, out, 0);
    CHECK(far == IKResult::OUTSIDE_ENVELOPE || far == IKResult::OUT_OF_REACH, "far target gave %s", ikResultStr(far));
    // Height is body-frame "up", reach is distance from the origin. Points
    // inside the reach sphere and the base sweep, but above / below the
    // taught heights: out to the right and up (base 120), and straight down.
    Vec3 hiB = bodyToArm({0, -0.866f * (g_env.zMax + 20) / 0.5f, g_env.zMax + 20}), loB = bodyToArm({0, 0, g_env.zMin - 20});
    CHECK(hiB.norm() < g_env.rMax && loB.norm() < g_env.rMax, "test points outside the reach sphere (%.0f, %.0f)", hiB.norm(), loB.norm());
    IKResult hi = solveIK(hiB.x, hiB.y, hiB.z, out, 0);
    CHECK(hi == IKResult::OUTSIDE_ENVELOPE, "too-high target gave %s", ikResultStr(hi));
    IKResult lo = solveIK(loB.x, loB.y, loB.z, out, 0);
    CHECK(lo == IKResult::OUTSIDE_ENVELOPE, "too-low target gave %s", ikResultStr(lo));
    IKResult sweep = solveIK(300, 300 * tanf(radians(g_env.baseMaxDeg + 10)), 0, out, 0);
    CHECK(sweep == IKResult::JOINT_LIMIT, "beyond taught sweep gave %s", ikResultStr(sweep));
  }

  // ----------------------------------------------------------------
  SECTION("13. joint calibration edits persist and do not invalidate taught poses");
  {
    bootWithPose(1000, 4090, 100, 2048, 2560);
    post("/teach/arm");
    int32_t effBefore = g_taught.pose[P_FWD].eff[0];
    CHECK(ok(post("/teach/cfg", "{\"joint\":0,\"ratio\":31.5,\"sign\":-1}")), "cfg");
    CHECK(g_j[0].ratio == 31.5f && g_j[0].sign == -1, "not applied");
    bootWithPose(1000, 4090, 100, 2048, 2560);                 // 'reboot': ratio/sign must come back from flash
    CHECK(std::fabs(g_j[0].ratio - 31.5f) < 1e-3 && g_j[0].sign == -1, "ratio %.2f sign %d after reboot", g_j[0].ratio, g_j[0].sign);
    CHECK(g_taught.pose[P_FWD].eff[0] == effBefore, "taught steps changed");
    CHECK(!ok(post("/teach/cfg", "{\"joint\":0,\"ratio\":0.1,\"sign\":1}")), "absurd ratio accepted");
    post("/teach/cfg", "{\"joint\":0,\"ratio\":20,\"sign\":1}");
  }

  // ----------------------------------------------------------------
  SECTION("14. websocket: auto gated, stop/home semantics, tip-frame targets");
  {
    bootWithPose(1000, 4090, 100, 2048, 2560);
    g_wsServer.sent.clear();
    g_wsServer.inject("{\"type\":\"mode\",\"value\":\"auto\"}");
    CHECK(!g_autoMode, "auto accepted while disarmed");
    post("/teach/arm");
    g_wsServer.inject("{\"type\":\"mode\",\"value\":\"auto\"}");
    CHECK(!g_autoMode, "auto accepted with the gripper's open/closed positions unset");
    // Set them the way the user will: jog, Set open; jog, Set closed.
    CHECK(ok(post("/teach/cfg", "{\"grip\":\"open\"}")), "set open refused");
    for (int k = 0; k < 5; k++) { post("/teach/jog", "{\"joint\":\"gripper\",\"delta\":-5}"); runMs(1500); }
    CHECK(ok(post("/teach/cfg", "{\"grip\":\"close\"}")), "set closed refused");
    CHECK(gripperCalibrated(), "gripper not calibrated: open %d close %d", g_gripOpen, g_gripClose);
    g_wsServer.inject("{\"type\":\"mode\",\"value\":\"auto\"}");
    CHECK(g_autoMode, "auto refused although armed+calibrated");
    g_wsServer.inject("{\"type\":\"target\",\"x\":100,\"y\":0,\"z\":0,\"confidence\":0.9,\"frame\":\"tip\"}");
    Vec3 tipNow = forwardKinematics(jointAngleRad(0), jointAngleRad(1), jointAngleRad(2), jointAngleRad(3));
    JointAngles cur = currentAngles();
    float pitch = cur.shoulder + cur.elbow + cur.forearm;
    printf("  tip @ (%.0f, %.0f, %.0f) pitch %.0f deg -> target (%.0f, %.0f, %.0f)\n", tipNow.x, tipNow.y, tipNow.z, degrees(pitch), g_cvTarget.x_mm, g_cvTarget.y_mm, g_cvTarget.z_mm);
    CHECK(std::fabs(g_cvTarget.x_mm - (tipNow.x + 100 * cosf(pitch))) < 1.0f && std::fabs(g_cvTarget.z_mm - (tipNow.z + 100 * sinf(pitch))) < 1.0f, "tip transform wrong");
    g_wsServer.inject("{\"type\":\"stop\"}");
    CHECK(!g_autoMode, "stop kept auto on");
    loop();
    CHECK(g_arm.fsm == GrabFSM::IDLE, "stop should leave FSM idle (was %s)", FSM_NAMES[(int)g_arm.fsm]);
    g_wsServer.inject("{\"type\":\"estop\"}");
    CHECK(g_faultActive, "ws estop ignored");
  }

  // ----------------------------------------------------------------
  SECTION("15. full autonomous grab cycle (FSM) with a tip-frame target");
  {
    bootWithPose(1000, 4090, 100, 2048, 2560);
    post("/teach/arm");
    post("/teach/scale", "{\"scale\":0.5}");
    // put the taught poses where the FSM can use them: use the calibration from section 7 (already in flash)
    g_wsServer.inject("{\"type\":\"mode\",\"value\":\"auto\"}");
    CHECK(g_autoMode, "auto refused");
    // a target the IK can reach inside the taught envelope; use a base-frame target
    float tx = 0, ty = 0, tz = 0; bool found = false;
    auto reach = [](Vec3 p) { JointAngles o; float pd; return solveWithPitches(p.x, p.y, p.z, o, pd) == IKResult::OK; };
    for (float y : {40.0f, 250.0f, -250.0f})
      for (float x = 300; x < 800 && !found; x += 25) for (float z = -300; z < 300 && !found; z += 25) {
        Vec3 t = {x, y, z};
        if (reach(t + upInArm() * APPROACH_CLEARANCE_MM) && reach(t) && reach(t + upInArm() * LIFT_HEIGHT_MM)) {
          tx = x; ty = y; tz = z; found = true;
        }
      }
    printf("  feasible target: %d (%.0f, %.0f, %.0f)   env r<=%.0f z[%.0f,%.0f]\n", found, tx, ty, tz, g_env.rMax, g_env.zMin, g_env.zMax);
    CHECK(found, "no IK-feasible target found inside the envelope");
    runMs(100);
    char msg[160]; snprintf(msg, sizeof msg, "{\"type\":\"target\",\"x\":%.0f,\"y\":%.0f,\"z\":%.0f,\"confidence\":0.9}", tx, ty, tz);
    g_wsServer.inject(msg);
    bool sawApproach = false, sawHandoff = false;
    uint64_t end = g_simUs + 900000ULL * 1000;
    while (g_simUs < end) {
      loop();
      if (g_arm.fsm == GrabFSM::APPROACH) sawApproach = true;
      if (g_arm.fsm == GrabFSM::HANDOFF) sawHandoff = true;
      if (sawHandoff && g_arm.fsm == GrabFSM::SCANNING) break;
      if (g_faultActive) break;
    }
    printf("  fsm ended in %s; approach=%d handoff=%d fault=%d '%s' move='%s'\n", FSM_NAMES[(int)g_arm.fsm], sawApproach, sawHandoff, g_faultActive, g_faultReason.c_str(), g_moveMsg.c_str());
    CHECK(!g_faultActive, "fault during FSM");
    CHECK(sawApproach, "never reached APPROACH (IK may have rejected the sample target)");
  }

  // ----------------------------------------------------------------
  SECTION("26. wheel-mode reverse bit: mismatch is caught, magnitude never forges the bit");
  {
    // Regression for the real-arm fault: NVS held revbit 0x0400 while the
    // servos use bit 15, so startWheel wrote "reverse at 100" as 1124, which
    // the servo read as FORWARD at 11x the commanded speed. Symptom on the
    // bench was: first jog fine, next jog lurches back the wrong way.
    for (int matched = 1; matched >= 0; matched--) {
      bootWithPose(3593, 694, 3119, 1511, 2048);
      for (int i = 1; i <= 5; i++) g_sim.servo[i].wheelRevBit = 0x8000;   // what the real ST3215 uses
      g_wheelRevBit = matched ? 0x8000 : 0x0400;
      runMs(1500);
      CHECK(armNow(true) == "", "arm refused");
      // Two -5 deg presses: the second crosses a revolution boundary downward,
      // which is the only path that writes the reverse bit.
      double moved2 = 0;
      for (int k = 0; k < 2; k++) {
        double p0 = phys(1);
        CHECK(ok(post("/teach/jog", "{\"joint\":\"shoulder\",\"delta\":-5}")), "jog %d rejected", k);
        runUntilIdle(30000);
        if (k == 1) moved2 = phys(1) - p0;
      }
      double want = -stepsFor(1, 5);
      printf("  revbit %s: 2nd press moved %+.1f (wanted %+.1f) msg='%s'\n",
             matched ? "MATCHED" : "MISMATCHED", moved2, want, g_moveMsg.c_str());
      if (matched) {
        CHECK(std::fabs(moved2 - want) < 12.0, "matched revbit should jog cleanly, moved %+.1f", moved2);
        CHECK(g_moveMsg.length() == 0, "matched revbit raised '%s'", g_moveMsg.c_str());
      } else {
        // We cannot make a mis-set bit move correctly, but it must be caught
        // and named rather than silently lurching.
        CHECK(g_moveMsg.find("WRONG WAY") != std::string::npos,
              "mismatched revbit not caught, msg='%s'", g_moveMsg.c_str());
      }
    }

    // Magnitude must never set the direction bit on its own. With revbit 0x0400
    // any commanded speed >= 1024 has bit 10 set, so a FORWARD wheel leg was
    // written as a reverse command and the joint ran backwards. DEFAULT_SPEED
    // (2000) at scale 1.0 is exactly that case.
    bootWithPose(3593, 694, 3119, 1511, 2048);
    for (int i = 1; i <= 5; i++) g_sim.servo[i].wheelRevBit = 0x0400;
    g_wheelRevBit = 0x0400;
    g_safetyScale = 1.0f;
    runMs(1500);
    CHECK(armNow(true) == "", "arm refused");
    // Shoulder +50 deg from home crosses a revolution boundary UPWARD (boot 694,
    // boundary at +3402 steps = +44.9 deg), so this is a forward wheel leg.
    double p0 = phys(1);
    JointAngles fast = currentAngles();
    fast.shoulder = radians(HOME_SHOULDER_DEG + 50.0);
    CHECK(armMoveToAngles(fast, DEFAULT_SPEED), "fast move rejected");
    runUntilIdle(60000);
    double got = phys(1) - p0;
    printf("  scale 1.0 revbit 0x0400, forward wheel leg @%d: moved %+.1f (wanted %+.1f) msg='%s'\n",
           (int)DEFAULT_SPEED, got, stepsFor(1, 50.0), g_moveMsg.c_str());
    CHECK(got > 0, "fast FORWARD wheel leg ran backwards - magnitude forged the reverse bit (moved %+.1f)", got);
    g_safetyScale = SAFETY_SCALE_DEFAULT;
  }

  // ----------------------------------------------------------------
  SECTION("27. re-energise never drives to a stale goal when position is unreadable");
  {
    bootWithPose(3593, 694, 3119, 1511, 2048);
    runMs(1500);
    CHECK(armNow(true) == "", "arm refused");
    JointTracker& j = g_j[1];
    // Load a goal far from here, then make every read fail across a mode change.
    g_bus.writeWord(j.id, REG_GOAL_POSITION, 3000);
    double p0 = phys(1);
    g_sim.dropAll = true;
    jointSetMode(j, 0);
    g_sim.dropAll = false;
    runMs(500);
    printf("  unreadable re-energise: powered=%d goalReg=%d lastRaw=%d moved %+.1f\n",
           (int)j.powered, (int)g_sim.servo[j.id].w(REG_GOAL_POSITION), (int)j.lastRaw, phys(1) - p0);
    // The guard must replace the stale 3000 with the tracker's last good read,
    // so the joint holds where it is instead of lunging across the range.
    CHECK(g_sim.servo[j.id].w(REG_GOAL_POSITION) != 3000,
          "stale goal 3000 left loaded at torque-on");
    CHECK((int)g_sim.servo[j.id].w(REG_GOAL_POSITION) == (int)j.lastRaw,
          "goal should be lastRaw (%d), got %d", (int)j.lastRaw,
          (int)g_sim.servo[j.id].w(REG_GOAL_POSITION));
    CHECK(std::fabs(phys(1) - p0) < 20.0, "joint lunged to a stale goal (moved %+.1f)", phys(1) - p0);
  }

  // ----------------------------------------------------------------
  SECTION("28. leapfrog handoff: spring-back / hold dither at the wrap never drives the long way");
  {
    // Regression for the first wear test: base and elbow "rotate some, go
    // back, rotate some" instead of moving continuously. The WHEEL leg handed
    // off to Mode 0 the instant the tracker saw the target's revolution, i.e.
    // at raw 0-3. The real ST3215 is single-turn in Mode 0, so a few steps of
    // drivetrain unwind during the torque-off mode switch, or of position
    // hunting, put it back at raw ~4090 and the goal (say 1180) was then
    // ~2900 steps BEHIND it: it drove backwards until WRONG WAY aborted.
    struct Case { double spring, dither; float scale; };
    const Case cases[] = { {0, 0, 0.1f}, {25, 0, 0.1f}, {0, 2, 0.1f}, {25, 2, 0.1f}, {100, 2, 0.1f}, {400, 2, 0.1f},
                           {25, 2, 1.0f}, {100, 2, 1.0f} };
    for (const Case& c : cases) {
      printf("  (spring-back %.0f, hold dither %.0f, scale %.1f)\n", c.spring, c.dither, c.scale);
      bootWithPose(1000, 694, 3900, 1511, 2048);   // elbow 196 steps below a wrap
      for (int i = 1; i <= 4; i++) { g_sim.servo[i].springBack = c.spring; g_sim.servo[i].holdDither = c.dither; }
      runMs(1500);
      CHECK(armNow(true) == "", "arm refused");
      g_safetyScale = c.scale;

      // Jog press + worst excursion AGAINST the jog direction while it ran.
      auto press = [&](const char* joint, int ji, double delta, double& moved, double& backslide) {
        char body[80]; snprintf(body, sizeof body, "{\"joint\":\"%s\",\"delta\":%.1f}", joint, delta);
        double p0 = phys(ji), worst = 0;
        bool acc = ok(post("/teach/jog", body));
        uint64_t end = g_simUs + 60000ULL * 1000;
        loop();
        while (g_simUs < end) {
          loop();
          double against = (delta > 0) ? (p0 - phys(ji)) : (phys(ji) - p0);
          if (against > worst) worst = against;
          if (allJointsIdle()) { runMs(300); break; }
        }
        moved = phys(ji) - p0; backslide = worst;
        return acc;
      };

      // Elbow: 1 deg presses up across the wrap and back down again.
      double sumMoved = 0, e0 = phys(2);
      bool allClean = true;
      for (int k = 0; k < 16; k++) {
        double d = (k < 8) ? 1.0 : -1.0, moved, back;
        CHECK(press("elbow", 2, d, moved, back), "elbow jog %d refused", k);
        double want = stepsFor(2, d);
        bool clean = std::fabs(moved - want) < 12 + c.dither && back < 40 + c.spring && g_moveMsg.length() == 0;
        if (!clean) printf("    elbow press %2d (%+.0f): moved %+7.1f want %+6.1f, backslide %.0f  msg='%s'\n",
                           k, d, moved, want, back, g_moveMsg.c_str());
        allClean &= clean; sumMoved += moved;
      }
      CHECK(allClean, "elbow jogs across the wrap were not clean");
      CHECK(std::fabs(phys(2) - e0) < 12 + c.dither, "elbow ended %.1f steps from where it started", phys(2) - e0);
      CHECK(std::fabs(jDeg(g_j[2]) - HOME_ELBOW_DEG) < 0.3, "elbow reads %.2f want %.1f", jDeg(g_j[2]), HOME_ELBOW_DEG);

      // Base: + then - (1-rev backlash leapfrog), then + again (another reversal).
      double b0 = phys(0), moved, back;
      CHECK(press("base", 0, 2.0, moved, back), "base + refused");
      CHECK(std::fabs(moved - stepsFor(0, 2)) < 12 + c.dither && back < 40 + c.spring, "base +2: moved %+.1f backslide %.0f msg='%s'", moved, back, g_moveMsg.c_str());
      CHECK(press("base", 0, -2.0, moved, back), "base - refused");
      CHECK(std::fabs(moved + stepsFor(0, 2) + g_j[0].bl) < 12 + c.dither && back < 40 + c.spring, "base -2 (+backlash): moved %+.1f backslide %.0f msg='%s'", moved, back, g_moveMsg.c_str());
      CHECK(press("base", 0, 2.0, moved, back), "base + refused");
      CHECK(std::fabs(moved - stepsFor(0, 2) - g_j[0].bl) < 12 + c.dither && back < 40 + c.spring, "base +2 (+backlash): moved %+.1f backslide %.0f msg='%s'", moved, back, g_moveMsg.c_str());
      CHECK(std::fabs(jDeg(g_j[0]) - 2.0) < 0.2, "base reads %.2f want 2.0", jDeg(g_j[0]));
      (void)b0;

      // A big coordinated move: several wheel legs finishing at different times.
      JointAngles a = { (float)radians(45), (float)radians(-20), (float)radians(70), (float)radians(-40) };
      CHECK(armMoveToAngles(a, 3000), "refused");
      CHECK(runUntilIdle(900000), "never finished");
      CHECK(g_lastMoveOk, "coordinated move failed: %s", g_moveMsg.c_str());
      float want[4] = {45, -20, 70, -40};
      for (int i = 0; i < 4; i++) CHECK(std::fabs(jDeg(g_j[i]) - want[i]) < 0.5, "joint %d = %.2f want %.1f", i, jDeg(g_j[i]), want[i]);
      CHECK(!g_faultActive, "fault: %s", g_faultReason.c_str());
      CHECK(modeViolations() == 0, "mode written with torque on %d times", modeViolations());
    }
    for (int i = 1; i <= 4; i++) { g_sim.servo[i].springBack = 0; g_sim.servo[i].holdDither = 0; }
    g_safetyScale = SAFETY_SCALE_DEFAULT;
  }

  // ----------------------------------------------------------------
  SECTION("29. reads reject corrupted and foreign reply frames");
  {
    // First full-bus test: with all five servos on the marginal bus, _read()
    // accepted any frame starting FF FF — the gripper's torque limit came back
    // as -14316, and a garbled PRESENT_POS would have jumped the tracker.
    bootWithPose(1000, 694, 3119, 1511, 2048);
    runMs(500);
    g_sim.corruptReads = 1;
    CHECK(g_bus.readWord(SERVO_ELBOW, REG_PRESENT_POS) == -1, "bad-checksum frame accepted");
    g_sim.foreignReads = 1;
    CHECK(g_bus.readWord(SERVO_ELBOW, REG_PRESENT_POS) == -1, "frame from another servo ID accepted");
    CHECK(g_bus.readWord(SERVO_ELBOW, REG_PRESENT_POS) == 3119, "clean read after bad ones failed");
    // A burst of damaged position replies must not move the tracker at all.
    int32_t before = g_j[2].absSteps;
    g_sim.corruptReads = 6;
    runMs(400);
    g_sim.corruptReads = 0;
    runMs(200);
    CHECK(g_j[2].absSteps == before, "tracker moved %ld steps on corrupted reads", (long)(g_j[2].absSteps - before));
    CHECK(!g_faultActive, "fault: %s", g_faultReason.c_str());
  }

  // ----------------------------------------------------------------
  SECTION("30. gripper on its own junction: five servos work where one junction could only drive four");
  {
    // 2026-09-27: four joints alone clean, gripper alone clean, all five on
    // one resistor junction -> base/elbow never answer. Model that limit and
    // prove the two-junction wiring clears it.
    CHECK(GRIPPER_OWN_BUS == 1, "firmware not configured for the gripper bus");
    bootWithPose(1000, 694, 3119, 1511, 2600, /*junctionLimit=*/4);
    runMs(1500);
    for (int i = 0; i < 4; i++) CHECK(g_j[i].ok && g_j[i].powered, "joint %d not ok/powered on a 4-servo junction", i);
    CHECK(g_sim.servo[5].torque, "gripper not energised at boot on its own bus");
    CHECK(g_sim.servo[5].w(REG_TORQUE_LIMIT) == GRIP_TORQUE, "gripper torque limit %d, want %d",
          (int)g_sim.servo[5].w(REG_TORQUE_LIMIT), GRIP_TORQUE);
    for (int i = 1; i <= 4; i++) CHECK(g_sim.servo[i].w(REG_TORQUE_LIMIT) == g_j[i - 1].tq, "joint id%d torque limit wrong", i);
    CHECK(g_j[0].tq == BASE_TORQUE && g_j[1].tq == DEFAULT_TORQUE, "base %u / shoulder %u torque limits", g_j[0].tq, g_j[1].tq);
    CHECK(armNow(true) == "", "arm refused");
    // diag must reach all five
    String d = get("/teach/diag");
    CHECK(d.find("\"id\":5,\"mode\":0") != std::string::npos, "diag can't see the gripper: %s", d.c_str());
    CHECK(d.find("\"id\":1,\"mode\":0") != std::string::npos, "diag can't see the base: %s", d.c_str());
    // gripper jog moves the gripper and nothing else
    double g0 = g_sim.servo[5].phys, b0 = phys(0);
    // (section 14 taught open/closed below this start; jog back toward that window)
    CHECK(ok(post("/teach/jog", "{\"joint\":\"gripper\",\"delta\":-2}")), "gripper jog refused");
    runMs(3000);
    CHECK(g_sim.servo[5].phys != g0, "gripper did not move");
    CHECK(std::fabs(phys(0) - b0) < 1, "base moved during a gripper jog");
    // a joint jog still works with the gripper wired
    double e0 = phys(2);
    CHECK(ok(post("/teach/jog", "{\"joint\":\"elbow\",\"delta\":2}")), "elbow jog refused");
    CHECK(runUntilIdle(30000), "elbow jog never finished");
    CHECK(std::fabs((phys(2) - e0) - stepsFor(2, 2)) < 12, "elbow moved %.1f", phys(2) - e0);
    // E-STOP cuts the gripper too, on its own wire
    post("/teach/estop");
    for (int i = 1; i <= 5; i++) CHECK(!g_sim.servo[i].torque, "servo %d still powered after E-STOP", i);
    CHECK(g_sim.log.empty(), "bus errors: %s", g_sim.log.empty() ? "" : g_sim.log[0].c_str());

    // Control: the old single-junction wiring under the same limit fails the
    // way the real arm did — which is exactly why the gripper moved.
    bootWithPose(1000, 694, 3119, 1511, 2600, /*junctionLimit=*/4, /*gripPort=*/1);
    runMs(1500);
    int deadJoints = 0; for (int i = 0; i < 4; i++) if (!g_j[i].ok) deadJoints++;
    printf("  control (all five on one junction): %d/4 joints unreachable\n", deadJoints);
    CHECK(deadJoints == 4, "single-junction control should reproduce the failure");
    CHECK(armNow(true) != "", "arming should be refused when joints don't answer");
    g_sim.junctionLimit = 0; g_sim.servo[5].port = 2;
  }

  // ----------------------------------------------------------------
  SECTION("31. gripper: jog is relative, open/closed are taught, nothing drives to a guessed window");
  {
    // Old jog clamped an ABSOLUTE angle into 0..90 deg around raw 2048: with
    // the jaws resting at raw 500, the first 1-degree press jumped them
    // ~1550 steps into their hard stop.
    post("/teach/cfg", "{\"grip\":\"clear\"}");
    bootWithPose(1000, 694, 3119, 1511, 500);
    runMs(1000);
    CHECK(armNow(true) == "", "arm refused");
    CHECK(!gripperCalibrated(), "open/closed should be clear");
    double g0 = g_sim.servo[5].phys;
    CHECK(ok(post("/teach/jog", "{\"joint\":\"gripper\",\"delta\":1}")), "jog refused");
    runMs(1500);
    double step = g_sim.servo[5].phys - g0;
    printf("  gripper at raw 500, +1 deg jog moved %+.1f steps\n", step);
    CHECK(std::fabs(step - 4096.0 / 360.0) < 3, "+1 deg jog moved %+.1f steps (want ~+11)", step);
    // uncalibrated open/close must not move it
    double g1 = g_sim.servo[5].phys;
    gripperOpen(); runMs(1500);
    gripperClose(); runMs(1500);
    CHECK(std::fabs(g_sim.servo[5].phys - g1) < 1, "uncalibrated open/close moved the gripper %+.1f", g_sim.servo[5].phys - g1);
    // teach: here = closed; jog +20 deg = open
    CHECK(ok(post("/teach/cfg", "{\"grip\":\"close\"}")), "set closed");
    for (int k = 0; k < 4; k++) { post("/teach/jog", "{\"joint\":\"gripper\",\"delta\":5}"); runMs(1500); }
    CHECK(ok(post("/teach/cfg", "{\"grip\":\"open\"}")), "set open");
    int16_t op = g_gripOpen, cl = g_gripClose;
    printf("  taught: open %d, closed %d\n", op, cl);
    CHECK(op > cl + 200, "open %d should be ~227 above closed %d", op, cl);
    // jog is clamped to the taught window
    for (int k = 0; k < 3; k++) { post("/teach/jog", "{\"joint\":\"gripper\",\"delta\":5}"); runMs(1500); }
    CHECK(g_sim.servo[5].withinRev() <= op + 1, "jog went past open: %d > %d", g_sim.servo[5].withinRev(), op);
    gripperClose(); runMs(3000);
    CHECK(std::abs(g_sim.servo[5].withinRev() - cl) <= 1, "close went to %d, want %d", g_sim.servo[5].withinRev(), cl);
    for (int k = 0; k < 3; k++) { post("/teach/jog", "{\"joint\":\"gripper\",\"delta\":-5}"); runMs(1500); }
    CHECK(g_sim.servo[5].withinRev() >= cl - 1, "jog went past closed: %d < %d", g_sim.servo[5].withinRev(), cl);
    gripperOpen(); runMs(3000);
    CHECK(std::abs(g_sim.servo[5].withinRev() - op) <= 1, "open went to %d, want %d", g_sim.servo[5].withinRev(), op);
    // survives a reboot, and status reports it
    bootWithPose(1000, 694, 3119, 1511, 500);
    CHECK(g_gripOpen == op && g_gripClose == cl, "grip positions lost across reboot (%d/%d)", g_gripOpen, g_gripClose);
    String st = get("/teach/status");
    CHECK(st.find(("\"grip_open\":" + std::to_string(op)).c_str()) != std::string::npos, "status missing grip_open: %s", st.c_str());
    CHECK(!ok(post("/teach/cfg", "{\"grip\":\"sideways\"}")), "bad grip value accepted");
    CHECK(!g_faultActive, "fault: %s", g_faultReason.c_str());
  }

  // ----------------------------------------------------------------
  SECTION("32. temperature: one bad reading warns, a sustained over-temp E-STOPs");
  {
    bootWithPose(1000, 694, 3119, 1511, 2048);
    runMs(1500);
    CHECK(armNow(true) == "", "arm refused");
    // One elevated reading for the elbow (each servo is polled every ~750 ms,
    // so a 400 ms window catches at most one poll).
    g_sim.servo[3].reg[0x3F] = 67;
    runMs(400);
    g_sim.servo[3].reg[0x3F] = 30;
    runMs(3000);
    CHECK(!g_faultActive, "single 67C reading tripped an E-STOP: %s", g_faultReason.c_str());
    // Sustained: must trip within a few seconds, and name the servo.
    g_sim.servo[3].reg[0x3F] = 70;
    runMs(4000);
    CHECK(g_faultActive && g_faultReason.find("ID3 temp 70C") != std::string::npos,
          "sustained 70C did not E-STOP: '%s'", g_faultReason.c_str());
    for (int i = 1; i <= 5; i++) CHECK(!g_sim.servo[i].torque, "servo %d still powered", i);
    g_sim.servo[3].reg[0x3F] = 30;
  }

  // ----------------------------------------------------------------
  SECTION("33. base spin test: free motor, no angle limit, live speed, stops clean anywhere, re-references");
  {
    struct Case { double spring, dither; };
    const Case cases[] = { {0, 0}, {100, 2}, {400, 2} };
    for (const Case& c : cases) {
      printf("  (spring-back %.0f, hold dither %.0f)\n", c.spring, c.dither);
      bootWithPose(1000, 694, 3119, 1511, 2048);
      for (int i = 1; i <= 4; i++) { g_sim.servo[i].springBack = c.spring; g_sim.servo[i].holdDither = c.dither; }
      runMs(1500);
      const float startDeg = jDeg(g_j[0]);

      CHECK(!ok(post("/test/base/spin", "{\"dir\":1,\"pct\":50}")), "spin accepted while disarmed");
      CHECK(armNow(true) == "", "arm refused");
      CHECK(!ok(post("/test/base/spin", "{\"dir\":1,\"pct\":5}")), "5%% accepted");
      CHECK(!ok(post("/test/base/spin", "{\"dir\":1,\"pct\":110}")), "110%% accepted");
      CHECK(!ok(post("/test/base/spin", "{\"dir\":0,\"pct\":50}")), "dir 0 accepted");
      g_autoMode = true;
      CHECK(!ok(post("/test/base/spin", "{\"dir\":1,\"pct\":50}")), "spin accepted in auto mode");
      g_autoMode = false;
      CHECK(allJointsIdle() && !g_baseSpin.active, "a refused spin left something moving");

      // Runs at the chosen share of the wheel ceiling, NOT scaled by the 10 % safety scale.
      CHECK(g_safetyScale == SAFETY_SCALE_DEFAULT, "scale %.2f", g_safetyScale);
      CHECK(ok(post("/test/base/spin", "{\"dir\":1,\"pct\":50}")), "spin refused");
      runMs(500);
      double p0 = phys(0); runMs(1000);
      CHECK(std::fabs((phys(0) - p0) - 1700) < 60, "base ran %.0f steps/s, want 1700", phys(0) - p0);
      CHECK(g_sim.servo[1].reg[0x21] == 1, "base not in wheel mode (mode %d)", g_sim.servo[1].reg[0x21]);
      String st = get("/test/status");
      CHECK(st.find("\"active\":true") != std::string::npos && st.find("\"speed\":1700") != std::string::npos, "status: %s", st.c_str());
      CHECK(!ok(post("/teach/jog", "{\"joint\":\"elbow\",\"delta\":1}")), "jog accepted mid-spin");
      CHECK(!ok(post("/test/base/spin", "{\"dir\":-1,\"pct\":50}")), "direction change accepted mid-spin");

      // Live speed change, same direction.
      CHECK(ok(post("/test/base/spin", "{\"dir\":1,\"pct\":100}")), "speed change refused: %s", g_moveMsg.c_str());
      runMs(300);
      p0 = phys(0); runMs(1000);
      CHECK(std::fabs((phys(0) - p0) - 3400) < 80, "after speed change base ran %.0f steps/s, want 3400", phys(0) - p0);

      // No angle limit: runs well past BASE_MAX_DEG + slack, no E-STOP, no stop.
      uint64_t end = g_simUs + 120000ULL * 1000;
      while (g_simUs < end && jDeg(g_j[0]) < BASE_MAX_DEG + JOINT_LIMIT_SLACK_DEG + 40) loop();
      runMs(1000);
      CHECK(!g_faultActive && g_baseSpin.active && g_j[0].mv == JMove::WHEEL,
            "free spin ended past the limit: deg %.1f fault='%s' msg='%s'", jDeg(g_j[0]), g_faultReason.c_str(), g_moveMsg.c_str());
      CHECK(std::labs(g_baseSpin.steps - (g_j[0].absSteps - g_baseSpin.startAbs)) < 200, "turn counter %ld off", (long)g_baseSpin.steps);
      CHECK(ok(post("/test/base/spin", "{\"dir\":1,\"pct\":50}")), "slow-down refused");

      // Stop at points all round the servo revolution, including just either
      // side of the wrap, at several speeds and both directions: it must hold
      // near where it was stopped, never slide back the long way, stay put, and
      // read the angle it read before the spin (no limit E-STOP afterwards).
      const int windows[][2] = { {0, 120}, {3950, 4095}, {200, 320}, {2000, 2150}, {0, 120}, {3950, 4095} };
      const int pcts[]       = { 50, 100, 10, 30, 100, 40 };
      const int dirs[]       = { 1, 1, 1, 1, -1, -1 };
      for (int k = 0; k < 6; k++) {
        float degBefore = k ? jDeg(g_j[0]) : startDeg;   // spin 0 is already running
        if (k) {
          char body[48]; snprintf(body, sizeof body, "{\"dir\":%d,\"pct\":%d}", dirs[k], pcts[k]);
          CHECK(ok(post("/test/base/spin", body)), "restart %d refused: %s", k, g_moveMsg.c_str());
          runMs(dirs[k] < 0 ? 15000 : 3000);   // carry the reading well past a limit first
        }
        end = g_simUs + 60000ULL * 1000;
        while (g_simUs < end && !(g_j[0].mv == JMove::WHEEL &&
               g_sim.servo[1].withinRev() >= windows[k][0] && g_sim.servo[1].withinRev() <= windows[k][1])) loop();
        int rawAtStop = g_sim.servo[1].withinRev();
        double pStop = phys(0), worstBack = 0;
        int d = dirs[k];
        CHECK(ok(post("/test/base/stop")), "stop refused");
        end = g_simUs + 30000ULL * 1000;
        while (g_simUs < end && g_baseSpin.active) { loop(); worstBack = std::max(worstBack, d * (pStop - phys(0))); }
        double pHold = phys(0);
        runMs(2000);
        worstBack = std::max(worstBack, d * (pStop - phys(0)));
        double over = d * (pHold - pStop);
        // An unwind bigger than WHEEL_HANDOFF_STEPS costs one wrap replan, and
        // that drops torque twice (into Mode 0, back to wheel), so up to two
        // spring-backs — the same as any leapfrog move with that drivetrain.
        bool clean = !g_baseSpin.active && g_lastMoveOk && !g_faultActive && worstBack < 40 + 2 * c.spring &&
                     over < WHEEL_HANDOFF_STEPS * 4 && std::fabs(phys(0) - pHold) < 12 + c.dither &&
                     g_sim.servo[1].reg[0x21] == 0 && g_sim.servo[1].torque &&
                     std::fabs(jDeg(g_j[0]) - degBefore) < 0.5f;
        if (!clean) printf("    stop %d at raw %d (%d%%, dir %d): overrun %+.0f, backslide %.0f, drift %+.1f, mode %d, deg %.2f (was %.2f), msg='%s' fault='%s'\n",
                           k, rawAtStop, pcts[k], d, over, worstBack, phys(0) - pHold, g_sim.servo[1].reg[0x21],
                           jDeg(g_j[0]), degBefore, g_moveMsg.c_str(), g_faultReason.c_str());
        CHECK(clean, "stop %d (raw %d) was not clean", k, rawAtStop);
        CHECK(std::labs(g_baseSpin.steps) > 4096, "stop %d: last-spin counter %ld", k, (long)g_baseSpin.steps);
      }

      // A base jog after a spin moves by the jog, not back toward a limit.
      double pj = phys(0);
      CHECK(ok(post("/teach/jog", "{\"joint\":\"base\",\"delta\":2}")), "jog after spin refused");
      CHECK(runUntilIdle(120000) && !g_faultActive, "jog after spin: fault='%s'", g_faultReason.c_str());
      double jogSteps = 2.0 * g_j[0].ratio * 4096 / 360;
      CHECK(std::fabs(phys(0) - pj) < jogSteps + g_j[0].bl + 100, "jog +2 after spin moved %.0f steps", phys(0) - pj);

      // A jammed base: the stall watchdog ends the spin and holds, no E-STOP.
      CHECK(ok(post("/test/base/spin", "{\"dir\":1,\"pct\":30}")), "spin refused before jam");
      runMs(1000);
      g_sim.servo[1].maxSpeed = 0;
      runMs(3000);
      CHECK(!g_baseSpin.active && !g_lastMoveOk && !g_faultActive && g_sim.servo[1].torque && g_sim.servo[1].reg[0x21] == 0,
            "jam: active %d ok %d fault='%s' msg='%s'", g_baseSpin.active, g_lastMoveOk, g_faultReason.c_str(), g_moveMsg.c_str());
      g_sim.servo[1].maxSpeed = 3400;
      runMs(1000);
      CHECK(!g_faultActive, "E-STOP after the jam: %s", g_faultReason.c_str());

      // E-STOP mid-spin, well past a limit: torque cut, spin over, reset holds with no limit trip.
      CHECK(ok(post("/test/base/spin", "{\"dir\":-1,\"pct\":100}")), "spin refused before E-STOP");
      runMs(20000);
      CHECK(g_baseSpin.active && !g_faultActive, "spin not running before E-STOP: %s", g_faultReason.c_str());
      CHECK(ok(post("/teach/estop")), "estop refused");
      runMs(200);
      CHECK(g_faultActive && !g_baseSpin.active, "E-STOP did not end the spin");
      for (int i = 1; i <= 5; i++) CHECK(!g_sim.servo[i].torque, "servo %d still powered after E-STOP", i);
      CHECK(ok(post("/teach/fault_reset")), "reset refused");
      double pr = phys(0); runMs(2000);
      CHECK(!g_faultActive, "fault came back after reset: %s", g_faultReason.c_str());
      CHECK(std::fabs(phys(0) - pr) < 12 + c.dither + c.spring, "base moved %.0f after fault reset", phys(0) - pr);
      CHECK(!g_baseSpin.active && allJointsIdle(), "something is moving after fault reset");
      CHECK(modeViolations() == 0, "mode written with torque on %d times", modeViolations());
    }
    for (int i = 1; i <= 4; i++) { g_sim.servo[i].springBack = 0; g_sim.servo[i].holdDither = 0; }
  }

  // ----------------------------------------------------------------
  SECTION("34. CV test panel: cv_obs never moves the arm; base point + IK match the FSM; approach-only is gated");
  {
    bootWithPose(1000, 694, 3119, 1511, 2048);
    runMs(1500);
    CHECK(armNow(true) == "", "arm refused");
    // Tool-frame coords that land on base point B from the current pose (inverse of tipToBase).
    auto tipFor = [](Vec3 B) {
      JointAngles a = currentAngles();
      Vec3 t = forwardKinematics(a.base, a.shoulder, a.elbow, a.forearm);
      Vec3 d = B - t;
      float cb = cosf(a.base), sb = sinf(a.base), p = a.shoulder + a.elbow + a.forearm, cp = cosf(p), sp = sinf(p);
      float rx = d.x * cb + d.y * sb, ry = -d.x * sb + d.y * cb, rz = d.z;
      return Vec3{ rx * cp + rz * sp, ry, -rx * sp + rz * cp };
    };
    auto obs = [&](Vec3 B, float conf) {
      Vec3 l = tipFor(B);
      char j[320];
      snprintf(j, sizeof j, "{\"type\":\"cv_obs\",\"det\":true,\"auto\":false,\"fps\":9.5,\"infer_ms\":57,\"n\":1,"
               "\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"confidence\":%.2f,\"fill\":0.31,\"depth\":213,\"px\":300,\"py\":250,"
               "\"bw\":90,\"bh\":150,\"grip\":false}", l.x, l.y, l.z, conf);
      g_wsServer.inject(j);
    };
    // A point the IK can reach inside the taught envelope: the tip of a known
    // pose, used as the approach point; the target is straight below it.
    Vec3 appr = {0, 0, 0};
    {
      const float cand[][4] = { {20, -20, 70, -110}, {60, 40, -20, -60}, {90, 0, 40, -60}, {45, 60, -40, -30},
                                {30, 80, -60, -20}, {70, 20, 30, -90}, {100, 30, 0, -40} };
      for (const auto& c : cand) {
        Vec3 p = forwardKinematics(radians(c[0]), radians(c[1]), radians(c[2]), radians(c[3]));
        JointAngles o; float pd;
        if (solveWithPitches(p.x, p.y, p.z, o, pd) == IKResult::OK) { appr = p; break; }
      }
      printf("  approach point (%.0f, %.0f, %.0f), body (%.0f, %.0f, %.0f)\n", appr.x, appr.y, appr.z,
             armToBody(appr).x, armToBody(appr).y, armToBody(appr).z);
    }
    Vec3 B = appr - upInArm() * APPROACH_CLEARANCE_MM;
    double p0[4]; for (int i = 0; i < 4; i++) p0[i] = phys(i);
    for (int k = 0; k < 20; k++) { obs(B, 0.9f); runMs(200); }
    for (int i = 0; i < 4; i++) CHECK(std::fabs(phys(i) - p0[i]) < 2, "cv_obs moved joint %d by %.1f", i, phys(i) - p0[i]);
    CHECK(allJointsIdle(), "cv_obs started a move");
    CHECK(std::fabs(g_cvObs.base.x - B.x) < 1 && std::fabs(g_cvObs.base.y - B.y) < 1 && std::fabs(g_cvObs.base.z - B.z) < 1,
          "base point (%.1f,%.1f,%.1f) want (%.1f,%.1f,%.1f)", g_cvObs.base.x, g_cvObs.base.y, g_cvObs.base.z, B.x, B.y, B.z);
    JointAngles ref; float refPitch;
    IKResult want = solveWithPitches(appr.x, appr.y, appr.z, ref, refPitch);
    CHECK(want == IKResult::OK, "test point should be reachable: %s", ikResultStr(want));
    CHECK(g_cvObs.ikAppr == want && g_cvObs.pitchAppr == refPitch, "approach IK %s pitch %.0f, FSM would get %s pitch %.0f",
          ikResultStr(g_cvObs.ikAppr), g_cvObs.pitchAppr, ikResultStr(want), refPitch);
    String st = get("/test/status");
    CHECK(st.find("\"det_pct\":100") != std::string::npos && st.find("\"ik_appr\":\"OK\"") != std::string::npos, "status: %s", st.c_str());
    // A frame with no bottle lowers the detection rate but keeps the last point.
    g_wsServer.inject("{\"type\":\"cv_obs\",\"det\":false,\"fps\":9}");
    CHECK(!g_cvObs.det && g_cvObs.histN == 21 && cvDetectPct() == 95, "det rate %d over %d", cvDetectPct(), (int)g_cvObs.histN);

    // Approach-only is refused until calibrated, when stale, low-confidence, in auto, or unarmed.
    obs(B, 0.9f);
    g_taught.calibrated = false;   // earlier sections leave 7 taught poses in the simulated NVS
    CHECK(!ok(post("/test/cv/approach")), "approach allowed while uncalibrated");
    g_taught.calibrated = true;
    runMs(1200);
    CHECK(!ok(post("/test/cv/approach")), "approach allowed on a stale detection");
    obs(B, 0.3f);
    CHECK(!ok(post("/test/cv/approach")), "approach allowed at confidence 0.3");
    obs(B, 0.9f);
    g_autoMode = true;
    CHECK(!ok(post("/test/cv/approach")), "approach allowed in auto mode");
    g_autoMode = false;
    Vec3 far = { 2000, 0, 0 };
    obs(far, 0.9f);
    CHECK(!ok(post("/test/cv/approach")), "approach allowed to an unreachable point");

    // Allowed: lands on the approach point and stops there.
    obs(B, 0.9f);
    String ar = post("/test/cv/approach");
    CHECK(ok(ar), "approach refused: %s", ar.c_str());
    CHECK(runUntilIdle(900000), "approach never finished");
    Vec3 tip = forwardKinematics(jointAngleRad(0), jointAngleRad(1), jointAngleRad(2), jointAngleRad(3));
    CHECK((tip - appr).norm() < 6, "tip ended at (%.0f,%.0f,%.0f), approach point (%.0f,%.0f,%.0f)", tip.x, tip.y, tip.z, appr.x, appr.y, appr.z);
    CHECK(!g_faultActive && g_lastMoveOk, "approach: msg='%s' fault='%s'", g_moveMsg.c_str(), g_faultReason.c_str());
    // Detections while it moved are flagged and can't be approached.
    JointAngles back = { radians(10), radians(-40), radians(90), radians(-30) };
    armMoveToAngles(back, 3000);
    runMs(300);
    obs(B, 0.9f);
    CHECK(g_cvObs.whileMoving, "a detection during motion was not flagged");
    CHECK(runUntilIdle(900000), "move never finished");
    CHECK(!ok(post("/test/cv/approach")), "approach allowed on a detection taken while moving");
    g_taught.calibrated = false;

    // Camera settings persist across a reboot, clamped to the sensor's range.
    CHECK(ok(post("/test/cam", "{\"brightness\":1,\"wb_mode\":3,\"hmirror\":1,\"ae_level\":9}")), "cam refused");
    g_cam = CamSettings();
    bootWithPose(1000, 694, 3119, 1511, 2048);
    CHECK(g_cam.brightness == 1 && g_cam.wbMode == 3 && g_cam.hmirror == 1 && g_cam.aeLevel == 2 && g_cam.contrast == 0,
          "cam after reboot: br %d wb %d hm %d ae %d", g_cam.brightness, g_cam.wbMode, g_cam.hmirror, g_cam.aeLevel);
    CHECK(get("/test/status").find("\"wb_mode\":3") != std::string::npos, "status lacks cam settings");
    post("/test/cam", "{\"brightness\":0,\"wb_mode\":0,\"hmirror\":0,\"ae_level\":0}");
  }

  // ----------------------------------------------------------------
  SECTION("35. worn mount: body frame, plumb home, base + = right, pitch as elevation, collisions");
  {
    // Rotation is orthonormal and round-trips.
    const MountAxes& m = mountAxes();
    CHECK(std::fabs(m.X.norm() - 1) < 1e-4 && std::fabs(m.Y.norm() - 1) < 1e-4 && std::fabs(m.Z.norm() - 1) < 1e-4 &&
          std::fabs(m.X.dot(m.Y)) < 1e-4 && std::fabs(m.X.dot(m.Z)) < 1e-4 && std::fabs(m.Y.dot(m.Z)) < 1e-4, "mount axes not orthonormal");
    Vec3 q = {123, -45, 67}, rt = armToBody(bodyToArm(q));
    CHECK((rt - q).norm() < 1e-3, "body->arm->body round trip off by %.4f", (rt - q).norm());
    // Default mount: base axis straight back, base 0 hangs plumb, base + swings right.
    CHECK(std::fabs(m.Z.x + 1) < 1e-4 && std::fabs(m.X.z + 1) < 1e-4 && std::fabs(m.Y.y + 1) < 1e-4,
          "default mount: Z (%.2f,%.2f,%.2f) X (%.2f,%.2f,%.2f)", m.Z.x, m.Z.y, m.Z.z, m.X.x, m.X.y, m.X.z);
    // HOME: link 1 30 deg back from plumb, the rest plumb, nothing swung sideways.
    Vec3 pts[4];
    forwardKinematicsFull(radians(HOME_BASE_DEG), radians(HOME_SHOULDER_DEG), radians(HOME_ELBOW_DEG), radians(HOME_FOREARM_DEG), pts);
    Vec3 el = armToBody(pts[1]), wr = armToBody(pts[2]), tp = armToBody(pts[3]);
    CHECK(std::fabs(el.x + L1_MM * 0.5f) < 0.5 && std::fabs(el.z + L1_MM * 0.866f) < 0.5 && std::fabs(el.y) < 0.5,
          "home elbow at body (%.1f,%.1f,%.1f)", el.x, el.y, el.z);
    CHECK(std::fabs(wr.x - el.x) < 0.5 && std::fabs(tp.x - el.x) < 0.5 && tp.z < -1000, "home: links below the elbow not plumb");
    JointAngles home = { (float)radians(HOME_BASE_DEG), (float)radians(HOME_SHOULDER_DEG), (float)radians(HOME_ELBOW_DEG), (float)radians(HOME_FOREARM_DEG) };
    CHECK(!poseCollidesBody(home), "the hanging HOME pose collides with the body model");
    Vec3 sw = armToBody(forwardKinematics(radians(30), radians(30), radians(-30), 0));
    CHECK(sw.y < -400, "base +30 should swing the arm to the right (body y %.0f)", sw.y);
    // Link 1 swung forward into the back collides; straight back does not.
    JointAngles intoBack = { 0, (float)radians(-80), 0, 0 }, outBack = { 0, (float)radians(90), 0, 0 };
    CHECK(poseCollidesBody(intoBack), "arm swung forward through the back was not flagged");
    CHECK(!poseCollidesBody(outBack), "arm straight back flagged as a collision");
    // Approach / lift offsets are straight up in the body frame.
    g_activeTarget.x_mm = 500; g_activeTarget.y_mm = 200; g_activeTarget.z_mm = -300;
    Vec3 up = armToBody(aboveTarget(80)) - armToBody({500, 200, -300});
    CHECK(std::fabs(up.x) < 1e-3 && std::fabs(up.y) < 1e-3 && std::fabs(up.z - 80) < 1e-3, "approach offset (%.2f,%.2f,%.2f)", up.x, up.y, up.z);

    // Pitch = body-frame elevation of the tool, whatever the base angle.
    g_env = RuntimeEnvelope();   // hard limits only
    int checked = 0, bad = 0;
    const float shapes[][3] = { {20, 30, 20}, {60, -40, 0}, {90, -60, -20}, {45, 0, -45}, {70, -70, 30} };
    for (float bdeg : {0.0f, 40.0f, 75.0f})
      for (const auto& sh : shapes)
      for (float elev : {-90.0f, -60.0f, -30.0f}) {
        Vec3 tA = forwardKinematics(radians(bdeg), radians(sh[0]), radians(sh[1]), radians(sh[2]));
        float phi[2];
        int n = armPitchesForElevation(tA.x, tA.y, tA.z, elev, phi);
        for (int k = 0; k < n; k++) {
          JointAngles o;
          if (solveIK(tA.x, tA.y, tA.z, o, phi[k]) != IKResult::OK) continue;
          Vec3 p3[4];
          forwardKinematicsFull(o.base, o.shoulder, o.elbow, o.forearm, p3);
          Vec3 dir = armToBody(p3[3] - p3[2]);
          float got = degrees(asinf(dir.z / dir.norm()));
          // reachable elevation range for this arm plane
          Vec3 g = upInArm();
          float A = cosf(o.base) * g.x + sinf(o.base) * g.y, M = sqrtf(A * A + g.z * g.z);
          float lim = degrees(asinf(std::min(1.0f, M)));
          float want = std::max(-lim, std::min(lim, elev));
          checked++;
          if (std::fabs(got - want) > 0.5f || (p3[3] - tA).norm() > 0.5f) {
            bad++; printf("    base %.0f elev %.0f: got %.1f want %.1f, tip off %.1f\n", bdeg, elev, got, want, (p3[3] - tA).norm());
          }
        }
      }
    printf("  pitch-as-elevation: %d IK solutions checked\n", checked);
    CHECK(checked >= 10 && bad == 0, "pitch-as-elevation: %d of %d solutions wrong", bad, checked);
  }

  // ----------------------------------------------------------------
  SECTION("36. runtime backlash / torque limit; base stall cut-off is faster than the other joints'");
  {
    bootWithPose(1000, 694, 3119, 1511, 2048);
    runMs(1500);
    CHECK(armNow(true) == "", "arm refused");
    CHECK(ok(post("/teach/cfg", "{\"joint\":0,\"ratio\":50,\"sign\":1,\"backlash\":70,\"torque\":420}")), "cfg refused");
    CHECK(g_j[0].bl == 70 && g_j[0].tq == 420, "base bl %ld tq %u", (long)g_j[0].bl, g_j[0].tq);
    CHECK(g_sim.servo[1].w(REG_TORQUE_LIMIT) == 420, "torque limit not written live: %d", (int)g_sim.servo[1].w(REG_TORQUE_LIMIT));
    CHECK(!ok(post("/teach/cfg", "{\"joint\":0,\"ratio\":50,\"sign\":1,\"torque\":50}")), "torque 50 accepted");
    CHECK(!ok(post("/teach/cfg", "{\"joint\":0,\"ratio\":50,\"sign\":1,\"backlash\":99999}")), "backlash 99999 accepted");
    CHECK(ok(post("/teach/cfg", "{\"joint\":1,\"ratio\":6.667,\"sign\":1}")), "cfg without backlash/torque refused");
    CHECK(g_j[1].tq == DEFAULT_TORQUE && g_j[1].bl == 0, "omitted fields changed: bl %ld tq %u", (long)g_j[1].bl, g_j[1].tq);
    String st = get("/teach/status");
    CHECK(st.find("\"backlash\":70,\"torque\":420") != std::string::npos, "status lacks backlash/torque: %s", st.c_str());
    bootWithPose(1000, 694, 3119, 1511, 2048);
    CHECK(g_j[0].bl == 70 && g_j[0].tq == 420 && g_sim.servo[1].w(REG_TORQUE_LIMIT) == 420, "not persisted across reboot");
    runMs(1500);
    CHECK(armNow(true) == "", "arm refused");
    // A small backlash is taken up on reversal like the big one was.
    double b0 = phys(0);
    post("/teach/jog", "{\"joint\":\"base\",\"delta\":1}"); runUntilIdle(120000);
    post("/teach/jog", "{\"joint\":\"base\",\"delta\":-1}"); runUntilIdle(120000);
    CHECK(std::fabs(phys(0) - (b0 - 70)) < 12 && std::fabs(jDeg(g_j[0]) - HOME_BASE_DEG) < 0.05,
          "base after +1/-1 at 70 steps backlash: %.1f want %.1f, reads %.2f", phys(0), b0 - 70, jDeg(g_j[0]));

    // Stall cut-off: time from jam to the move being dropped (held, no fault).
    auto stallMs = [&](int ji, const char* name) {
      g_sim.servo[ji + 1].maxSpeed = 0;
      char body[64]; snprintf(body, sizeof body, "{\"joint\":\"%s\",\"delta\":3}", name);
      post("/teach/jog", body);
      uint64_t t0 = g_simUs;
      while (!allJointsIdle() && g_simUs - t0 < 5000000ULL) loop();
      g_sim.servo[ji + 1].maxSpeed = 3400;
      return (double)(g_simUs - t0) / 1000.0;
    };
    double tb = stallMs(0, "base"), ts = stallMs(1, "shoulder");
    printf("  stall cut-off: base %.0f ms, shoulder %.0f ms\n", tb, ts);
    CHECK(tb < BASE_NO_MOTION_MS + 200 && ts > NO_MOTION_MS - 50 && !g_faultActive && !g_lastMoveOk,
          "base %.0f ms, shoulder %.0f ms, fault '%s'", tb, ts, g_faultReason.c_str());
    // Put the defaults back for anything after this.
    post("/teach/cfg", "{\"joint\":0,\"ratio\":50,\"sign\":1,\"backlash\":0,\"torque\":350}");
  }

  // ----------------------------------------------------------------
  SECTION("37. handoff releases the object; demo sequence records, plays, stops, persists");
  {
    bootWithPose(1000, 694, 3119, 1511, 2048);
    runMs(1500);
    CHECK(armNow(true) == "", "arm refused");
    // Gripper open/closed: jog, Set (as the user did).
    CHECK(ok(post("/teach/cfg", "{\"grip\":\"open\"}")), "set open");
    for (int k = 0; k < 4; k++) { post("/teach/jog", "{\"joint\":\"gripper\",\"delta\":-5}"); runMs(1500); }
    CHECK(ok(post("/teach/cfg", "{\"grip\":\"close\"}")), "set closed");
    CHECK(gripperCalibrated(), "gripper not calibrated");
    auto gripGoal = []() { return (int)g_sim.servo[5].w(0x2A); };

    // HANDOFF: holds the object out, then OPENS (it used to close and never let go).
    if (g_taught.savedFlags & (1u << P_HANDOFF)) {
      setGripperSteps(g_gripClose, 300); runMs(1500);
      g_wsServer.sent.clear();
      g_gripped = true;
      g_safetyScale = 1.0f;   // the taught handoff is a long base move: keep it inside MOTION_TIMEOUT_MS
      g_arm.fsm = GrabFSM::HANDOFF;
      uint64_t t0 = g_simUs;
      runFSM();
      double ms = (g_simUs - t0) / 1000.0;
      bool said = false; for (auto& m : g_wsServer.sent) if (m.find("Handoff: take it") != std::string::npos) said = true;
      CHECK(said && gripGoal() == g_gripOpen && g_arm.fsm == GrabFSM::HOME,
            "handoff: announced %d, gripper goal %d (open %d), next state %s", said, gripGoal(), g_gripOpen, FSM_NAMES[(int)g_arm.fsm]);
      CHECK(ms >= HANDOFF_HOLD_MS + HANDOFF_RELEASE_MS, "handoff took only %.0f ms", ms);
      CHECK(!g_faultActive, "fault in handoff: %s", g_faultReason.c_str());
      g_arm.fsm = GrabFSM::IDLE; g_gripped = false; g_safetyScale = SAFETY_SCALE_DEFAULT;
    } else {
      CHECK(false, "no taught handoff pose to test with");
    }
    runUntilIdle(600000);

    // Record three steps by jogging.
    post("/teach/demo", "{\"op\":\"clear\"}");
    int32_t eff[3][4];
    auto jogAdd = [&](const char* joint, float d, int grip, int pause, int k) {
      char b[80]; snprintf(b, sizeof b, "{\"joint\":\"%s\",\"delta\":%.1f}", joint, d);
      CHECK(ok(post("/teach/jog", b)) && runUntilIdle(300000), "jog %s", joint);
      snprintf(b, sizeof b, "{\"op\":\"add\",\"grip\":%d,\"pause\":%d}", grip, pause);
      String r = post("/teach/demo", b);
      CHECK(ok(r), "add step %d: %s", k, r.c_str());
      for (int i = 0; i < 4; i++) eff[k][i] = jEff(g_j[i]);
    };
    jogAdd("shoulder", 10, DEMO_GRIP_OPEN, 200, 0);
    jogAdd("elbow", 10, DEMO_GRIP_CLOSE, 300, 1);
    jogAdd("shoulder", -10, DEMO_GRIP_KEEP, 0, 2);
    CHECK(g_demo.n == 3, "demo has %d steps", g_demo.n);
    CHECK(!ok(post("/teach/demo", "{\"op\":\"add\",\"grip\":7}")), "bad grip accepted");
    CHECK(get("/teach/status").find("\"demo\":{\"n\":3") != std::string::npos, "status lacks the demo");

    // Go somewhere else, then play: ends on step 3, gripper closed, "done".
    JointAngles away = currentAngles(); away.shoulder += radians(15.0f); away.forearm -= radians(20.0f);
    armMoveToAngles(away, 3000); runUntilIdle(300000);
    CHECK(ok(post("/teach/demo", "{\"op\":\"play\"}")), "play refused");
    CHECK(demoRunning(), "not playing");
    CHECK(!ok(post("/teach/jog", "{\"joint\":\"base\",\"delta\":1}")), "jog accepted while the demo plays");
    CHECK(!ok(post("/test/base/spin", "{\"dir\":1,\"pct\":10}")), "base spin accepted while the demo plays");
    uint64_t end = g_simUs + 600000ULL * 1000;
    while (demoRunning() && g_simUs < end) loop();
    CHECK(!demoRunning() && g_demoRun.msg == "done", "demo ended '%s'", g_demoRun.msg.c_str());
    for (int i = 0; i < 4; i++) CHECK(std::labs(jEff(g_j[i]) - eff[2][i]) <= 10, "joint %d at %ld want %ld", i, (long)jEff(g_j[i]), (long)eff[2][i]);
    CHECK(gripGoal() != g_gripOpen, "gripper not closed by step 2 (goal %d)", gripGoal());
    CHECK(!g_faultActive && g_lastMoveOk, "fault '%s' msg '%s'", g_faultReason.c_str(), g_moveMsg.c_str());

    // Stop mid-move holds; E-STOP mid-move ends it.
    CHECK(ok(post("/teach/demo", "{\"op\":\"play\"}")), "replay refused");
    runMs(300);
    CHECK(ok(post("/teach/demo", "{\"op\":\"stop\"}")) && !demoRunning(), "stop");
    runMs(500);
    CHECK(allJointsIdle() && !g_faultActive, "still moving after stop");
    CHECK(ok(post("/teach/demo", "{\"op\":\"play\"}")), "replay refused");
    runMs(300);
    post("/teach/estop");
    runMs(100);
    CHECK(!demoRunning() && g_demoRun.msg.find("fault") != std::string::npos, "E-STOP: demo '%s'", g_demoRun.msg.c_str());
    post("/teach/fault_reset");

    // Persists across a reboot; a re-zero needs force and wipes it.
    bootWithPose(1000, 694, 3119, 1511, 2048);
    CHECK(g_demo.n == 3 && g_demo.step[1].grip == DEMO_GRIP_CLOSE && g_demo.step[0].pauseMs == 200, "demo lost on reboot (n %d)", g_demo.n);
    runMs(1500);
    armNow(true);
    String z = post("/teach/setzero", "{}");
    CHECK(!ok(z) && z.find("SAVED") != std::string::npos, "re-zero with demo steps not refused: %s", z.c_str());
    CHECK(ok(post("/teach/setzero", "{\"force\":true}")) && g_demo.n == 0, "forced re-zero kept %d demo steps", g_demo.n);
    bootWithPose(1000, 694, 3119, 1511, 2048);
    CHECK(g_demo.n == 0, "wiped demo came back after reboot");
  }

  SECTION("38. base torque cap: fast -> slow down, still capped -> stop and hold, no fault");
  {
    SimServo& bs = g_sim.servo[1];
    auto spinPct = [&](int pct) { char b[48]; snprintf(b, sizeof b, "{\"dir\":1,\"pct\":%d}", pct); return ok(post("/test/base/spin", b)); };
    auto spinEnd = [&]() { post("/test/base/stop"); uint64_t t0 = g_simUs; while (g_baseSpin.active && g_simUs - t0 < 5000000ULL) loop(); };
    bootWithPose(1000, 694, 3119, 1511, 2048);
    runMs(1500);
    CHECK(armNow(true) == "", "arm refused");

    // Load under the cap: a full-speed spin is left alone.
    bs.loadPerSpeed = 0.05;   // 170 of 350 at 3400 steps/s
    CHECK(spinPct(100), "spin refused");
    runMs(1500);
    CHECK(g_baseSpin.speed == 3400 && g_baseSpin.active && g_baseCap.load > 150, "under-cap spin changed: speed %u load %u",
          (unsigned)g_baseSpin.speed, (unsigned)g_baseCap.load);
    spinEnd();

    // Speed-driven load (the real wheel-mode register): capped at 100 %, fine slow.
    bs.loadPerSpeed = 0.11;   // 374 -> pinned at 350 at full speed, 75 at the slow speed
    CHECK(spinPct(100), "spin refused");
    runMs(1500);
    double p0 = phys(0); runMs(1000);
    CHECK(g_baseSpin.active && g_baseSpin.speed == BASE_CAP_SLOW_SPEED && g_baseSpin.pct == 20 && !g_faultActive,
          "fast capped spin: active %d speed %u pct %u fault '%s'", g_baseSpin.active, (unsigned)g_baseSpin.speed,
          (unsigned)g_baseSpin.pct, g_faultReason.c_str());
    CHECK(std::fabs((phys(0) - p0) - BASE_CAP_SLOW_SPEED) < 40, "after step-down base ran %.0f steps/s", phys(0) - p0);
    String st = get("/test/status");
    CHECK(st.find("\"pct\":20,\"speed\":680") != std::string::npos, "status: %s", st.c_str());
    // A live speed change re-arms it: back up, and it steps down again.
    CHECK(spinPct(100), "speed change refused");
    runMs(1500);
    CHECK(g_baseSpin.active && g_baseSpin.speed == BASE_CAP_SLOW_SPEED, "no second step-down: speed %u", (unsigned)g_baseSpin.speed);
    spinEnd();
    CHECK(!g_faultActive && allJointsIdle(), "stop after step-down: fault '%s'", g_faultReason.c_str());

    // A spike shorter than BASE_CAP_HOLD_MS is ignored.
    bs.loadPerSpeed = 0.05;
    CHECK(spinPct(100), "spin refused");
    runMs(800);
    bs.loadBase = 400; runMs(BASE_CAP_HOLD_MS / 2); bs.loadBase = 0;
    runMs(800);
    CHECK(g_baseSpin.active && g_baseSpin.speed == 3400, "short spike stepped down: speed %u", (unsigned)g_baseSpin.speed);
    spinEnd();

    // Capped whatever the speed (jam / overload): slow down, then stop and hold.
    bs.loadPerSpeed = 0; bs.loadBase = 400;
    CHECK(spinPct(100), "spin refused");
    uint64_t t0 = g_simUs;
    while (g_baseSpin.active && g_simUs - t0 < 5000000ULL) loop();
    double tStop = (double)(g_simUs - t0) / 1000.0;
    printf("  capped spin: stepped down and stopped after %.0f ms\n", tStop);
    CHECK(!g_baseSpin.active && g_j[0].mv == JMove::HOLD && !g_faultActive && !g_lastMoveOk &&
          g_moveMsg.find("torque cap") != std::string::npos && g_moveMsg.find("680 steps/s - stopped") != std::string::npos,
          "capped spin: active %d mv %d fault '%s' msg '%s'", g_baseSpin.active, (int)g_j[0].mv,
          g_faultReason.c_str(), g_moveMsg.c_str());
    CHECK(tStop < 2 * (BASE_CAP_GRACE_MS + BASE_CAP_HOLD_MS) + 300, "took %.0f ms", tStop);
    CHECK(g_sim.servo[1].torque && g_sim.servo[1].reg[0x21] == 0, "base not holding in Mode 0 after the stop");
    p0 = phys(0); runMs(1000);
    CHECK(std::fabs(phys(0) - p0) < 10, "base drifted %.0f steps after the stop", phys(0) - p0);

    // A normal (safety-scaled, already slow) move just stops; other joints unaffected.
    double b0 = phys(0);
    CHECK(ok(post("/teach/jog", "{\"joint\":\"base\",\"delta\":5}")), "jog refused");
    runUntilIdle(5000);
    CHECK(!g_faultActive && g_moveMsg.find("Base at torque cap") != std::string::npos &&
          g_moveMsg.find("slowing") == std::string::npos && std::fabs(phys(0) - b0) > 5,
          "capped jog: fault '%s' msg '%s' moved %.0f", g_faultReason.c_str(), g_moveMsg.c_str(), phys(0) - b0);
    bs.loadBase = 0; bs.setW(0x3C, 0);
    CHECK(ok(post("/teach/jog", "{\"joint\":\"shoulder\",\"delta\":2}")), "shoulder jog refused");
    runUntilIdle(30000);
    CHECK(g_lastMoveOk && !g_faultActive, "shoulder jog after base cap stop: '%s'", g_moveMsg.c_str());
    // Load gone: the base jogs normally again.
    CHECK(ok(post("/teach/jog", "{\"joint\":\"base\",\"delta\":2}")), "jog refused");
    runUntilIdle(120000);
    CHECK(g_lastMoveOk && !g_faultActive, "base jog after load cleared: '%s'", g_moveMsg.c_str());
    bs.loadBase = 0; bs.loadPerSpeed = 0; bs.setW(0x3C, 0);
  }

  SECTION("39. CV proof capture: needs the laptop agent, only messages it, never moves / arms / goes auto");
  {
    bootWithPose(1000, 694, 3119, 1511, 2048);
    runMs(1500);
    g_cvCap = CvCapture();
    double p0[5]; for (int i = 0; i < 5; i++) p0[i] = g_sim.servo[i + 1].phys;
    auto capMsgs = [&](int* to = nullptr) { int n = 0;
      for (size_t i = 0; i < g_wsServer.sent.size(); i++)
        if (g_wsServer.sent[i].find("\"type\":\"cv_capture\"") != std::string::npos) { n++; if (to) *to = g_wsServer.sentTo[i]; }
      return n; };
    auto armState = [&]() { return std::string(g_armed ? "A" : "a") + (g_autoMode ? "U" : "u") + (g_faultActive ? "F" : "f") +
                                   (g_teachActive ? "T" : "t") + (allJointsIdle() ? "i" : "m"); };

    // Disarmed, no agent: clear error, nothing sent.
    std::string s0 = armState();
    g_wsServer.sent.clear(); g_wsServer.sentTo.clear();
    String r = post("/test/cv/capture", "{\"secs\":10}");
    CHECK(!ok(r) && r.find("capture agent not running on the laptop") != std::string::npos, "no agent: %s", r.c_str());
    CHECK(capMsgs() == 0, "trigger sent with no agent");
    CHECK(get("/test/status").find("\"capture\":{\"agent\":false") != std::string::npos, "status without agent");

    // A hello with another role is not an agent.
    g_wsServer.inject("{\"type\":\"agent_hello\",\"role\":\"viewer\"}", 3);
    CHECK(g_cvCap.agent == -1, "non-capture hello registered as agent");

    // Agent on client 4: the trigger goes to it alone, secs clamped.
    g_wsServer.inject("{\"type\":\"agent_hello\",\"role\":\"capture\"}", 4);
    CHECK(g_cvCap.agent == 4, "agent num %d", g_cvCap.agent);
    g_wsServer.sent.clear(); g_wsServer.sentTo.clear();
    CHECK(ok(post("/test/cv/capture", "{\"secs\":10}")), "capture refused with agent");
    int to = -9;
    CHECK(capMsgs(&to) == 1 && to == 4, "trigger count %d to %d", capMsgs(), to);
    bool has10 = false; for (auto& m : g_wsServer.sent) if (m.find("\"secs\":10") != std::string::npos) has10 = true;
    CHECK(has10, "secs not in trigger");
    g_wsServer.sent.clear(); g_wsServer.sentTo.clear();
    post("/test/cv/capture", "{\"secs\":999}");
    post("/test/cv/capture", "{\"secs\":1}");
    CHECK(g_wsServer.sent.size() >= 2 && g_wsServer.sent[0].find("\"secs\":120") != std::string::npos &&
          g_wsServer.sent[1].find("\"secs\":3") != std::string::npos, "clamp failed");

    // Status relay (with characters that need escaping).
    g_wsServer.inject("{\"type\":\"cv_capture_status\",\"state\":\"recording\",\"msg\":\"12/20 s \\\"x\\\"\",\"file\":\"\"}", 4);
    String st = get("/test/status");
    CHECK(st.find("\"capture\":{\"agent\":true") != std::string::npos && st.find("\"state\":\"recording\"") != std::string::npos &&
          st.find("12/20 s \\\"x\\\"") != std::string::npos, "status relay: %s", st.c_str());
    g_wsServer.inject("{\"type\":\"cv_capture_status\",\"state\":\"saved\",\"file\":\"CV_PROOF_2026-10-06_12-00-00_20s.mp4\"}", 4);
    CHECK(get("/test/status").find("CV_PROOF_2026-10-06_12-00-00_20s.mp4") != std::string::npos, "saved file not relayed");

    // Armed, with the arm holding: the same, and nothing changes state or moves.
    CHECK(armNow(true) == "", "arm refused");
    runMs(500);
    s0 = armState();
    for (int i = 0; i < 5; i++) p0[i] = g_sim.servo[i + 1].phys;
    CHECK(ok(post("/test/cv/capture", "{\"secs\":20}")), "capture refused when armed");
    g_wsServer.inject("{\"type\":\"cv_capture_status\",\"state\":\"starting\"}", 4);
    runMs(2000);
    CHECK(armState() == s0, "arm state changed %s -> %s", s0.c_str(), armState().c_str());
    for (int i = 0; i < 5; i++) CHECK(std::fabs(g_sim.servo[i + 1].phys - p0[i]) < 3, "capture moved servo %d by %.0f", i + 1,
                                      g_sim.servo[i + 1].phys - p0[i]);

    // Another client dropping keeps the agent; the agent dropping clears it.
    g_wsServer.disconnect(3);
    CHECK(g_cvCap.agent == 4, "other client's disconnect cleared the agent");
    g_wsServer.disconnect(4);
    CHECK(g_cvCap.agent == -1, "agent disconnect not noticed");
    r = post("/test/cv/capture", "{\"secs\":10}");
    CHECK(!ok(r) && r.find("capture agent not running") != std::string::npos, "after agent left: %s", r.c_str());
    CHECK(g_armed && !g_faultActive && !g_autoMode, "capture flow changed arm state");
  }

  printf("\n%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
