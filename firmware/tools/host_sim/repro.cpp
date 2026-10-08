#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "../../Final_Firmware_Main/Final_Firmware_Main.cpp"
static void runMs(uint32_t ms){ uint64_t e=g_simUs+(uint64_t)ms*1000; while(g_simUs<e) loop(); }
static bool runUntilIdle(uint32_t maxMs){ uint64_t e=g_simUs+(uint64_t)maxMs*1000; loop();
  while(g_simUs<e){ loop(); if(allJointsIdle()){ runMs(200); return true; } } return false; }
static String post(const char*p,const char*b="{}"){ return g_teachServer.request(true,p,b); }
static bool ok(const String&r){ return r.find("\"ok\":true")!=std::string::npos; }

static void scenario(const char* name, uint16_t fwBit, uint16_t servoBit, float delta, int presses){
  printf("\n=== %s  (firmware revbit=0x%04X, servo really uses 0x%04X, delta=%+.0f) ===\n",
         name, fwBit, servoBit, delta);
  g_armed=false; g_faultActive=false; g_faultReason=""; g_teachActive=false;
  g_safetyScale=SAFETY_SCALE_DEFAULT; g_moveMsg=""; g_lastMoveOk=true;
  g_sim.echo=true; g_sim.dropAll=false; g_sim.dropReads=0;
  g_sim.servo[1].phys=3593; g_sim.servo[2].phys=694;
  g_sim.servo[3].phys=3119; g_sim.servo[4].phys=1511; g_sim.servo[5].present=false;
  for(int i=1;i<=4;i++){ g_sim.servo[i].reg[0x21]=0; g_sim.servo[i].torque=false;
                         g_sim.servo[i].wheelRevBit=servoBit; g_sim.servo[i].modeWritesWithTorqueOn=0; }
  for(int i=0;i<4;i++) g_j[i].sign=1;
  setup();
  g_wheelRevBit = fwBit;          // after setup(), which loads NVS
  runMs(1500); armNow(true);

  for(int k=1;k<=presses;k++){
    JointTracker& j=g_j[1];
    int32_t before=j.absSteps; double pb=g_sim.servo[2].phys; double db=jDeg(j);
    float nd = constrain(jDeg(j)+delta, JOINT_MIN_DEG[1], JOINT_MAX_DEG[1]);
    int32_t want = j.zeroAbs + j.blOff + (int32_t)lround((nd-j.refDeg)*j.sign*j.ratio*4096.0f/360.0f);
    bool same = (floor((double)j.absSteps/4096.0)==floor((double)want/4096.0));
    char body[96]; snprintf(body,sizeof body,"{\"joint\":\"shoulder\",\"delta\":%.1f}",delta);
    bool acc=ok(post("/teach/jog",body));
    bool idle=runUntilIdle(30000);
    double pa=g_sim.servo[2].phys;
    printf("press %2d: deg %7.2f->%7.2f | abs %6d->%6d (want %6d) | %s | acc=%d idle=%d | "
           "phys moved %+9.1f (wanted %+6.1f) | %s%s\n",
           k, db, jDeg(j), before, j.absSteps, want, same?"SETTLE":"WHEEL ",
           (int)acc,(int)idle, pa-pb, (double)(want-before),
           g_moveMsg.length()?g_moveMsg.c_str():"-",
           g_faultActive?"  *** FAULT ***":"");
    if(g_faultActive){ printf("   reason: %s\n", g_faultReason.c_str()); break; }
  }
}

int main(){
  scenario("A control: revbit matched 0x8000, jog DOWN", 0x8000, 0x8000, -5, 4);
  scenario("B YOUR ARM: firmware 0x0400, servo 0x8000, jog DOWN", 0x0400, 0x8000, -5, 4);
  scenario("C firmware 0x0400, servo 0x8000, jog UP",   0x0400, 0x8000,  5, 10);
  return 0;
}
