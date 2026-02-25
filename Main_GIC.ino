// #include <WiFiS3.h>
// #include <Wire.h>
// #include <Adafruit_PWMServoDriver.h>
// #include <AccelStepper.h>
// #include <ArduinoGraphics.h> 
// #include "Arduino_LED_Matrix.h"
// #include "arduino_secrets.h"

// // --- PCA9685 PIN MAPPING ---
// #define S_SHOULDER 0  // DS3218 (High Torque)
// #define S_WRIST_J  1  // DS3218 (Wrist Pivot)
// #define S_FOREARM  2  // DS3218 (Elbow)

// #define CH_THUMB   4  // MG996R
// #define CH_ROT_W   5  // MG996R (Wrist Rotate)
// #define CH_INDEX   6  // MG996R

// // --- STEPPER CONFIG ---
// #define STEPPER_PUL_PIN 3
// #define STEPPER_DIR_PIN 2

// // --- CALIBRATION ---
// #define USMIN  100 
// #define USMAX  580 
// const int SPEED = 5; 

// Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
// AccelStepper baseStepper(AccelStepper::DRIVER, STEPPER_PUL_PIN, STEPPER_DIR_PIN);
// ArduinoLEDMatrix matrix;
// WiFiServer server(80);

// char ssid[] = SECRET_SSID;
// char pass[] = SECRET_PASS;

// // --- STATE TRACKING ---
// int angIndex = 0, angThumb = 60, angRotW = 0; 
// int angS = 90, angF = 90, angWJ = 90;         
// int lastP_Idx = 100, lastP_Thm = 266, lastP_Rot = 100;

// void setup() {
//   Serial.begin(115200);
//   delay(1500); 
//   Serial.println("\n--- PROJECT ARACHNAE STARTING ---");

//   pwm.begin();
//   pwm.setPWMFreq(50);
//   matrix.begin();

//   baseStepper.setMaxSpeed(1500);
//   baseStepper.setAcceleration(800);

//   Serial.print("Connecting to WiFi...");
//   WiFi.begin(ssid, pass); 
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }

//   server.begin();
//   Serial.println("\nConnected! IP: ");
//   Serial.println(WiFi.localIP());
  
//   displayIP(WiFi.localIP());

//   // Initialize all joints
//   syncHand(0, 60, 0); 
//   updateArm();
// }

// void loop() {
//   baseStepper.run(); 

//   // 1. Listen for Python AI (Serial)
//   if (Serial.available()) {
//     String cmd = Serial.readStringUntil('\n');
//     cmd.trim();
//     Serial.print("Serial Cmd: "); Serial.println(cmd);
//     processCommand(cmd);
//   }

//   // 2. Listen for Web Control (WiFi)
//   WiFiClient client = server.available();
//   if (client) {
//     String request = "";
//     while (client.connected() && client.available()) {
//       char c = client.read();
//       if (c == '\n') break;
//       request += c;
//     }
//     if (request.indexOf("/G") != -1) processCommand("G");
//     if (request.indexOf("/R") != -1) processCommand("R");
    
//     sendHTML(client);
//     client.stop();
//   }
// }

// void processCommand(String cmd) {
//   // GRIPPER
//   if (cmd == "G") syncHand(180, 60, 180);      
//   else if (cmd == "R") syncHand(0, 60, 0);     
  
//   // BASE
//   else if (cmd == "B_LEFT") baseStepper.move(-400);
//   else if (cmd == "B_RIGHT") baseStepper.move(400);

//   // ARM SEGMENTS
//   else if (cmd == "S+") { angS = constrain(angS + 10, 0, 180); updateArm(); }
//   else if (cmd == "S-") { angS = constrain(angS - 10, 0, 180); updateArm(); }
//   else if (cmd == "F+") { angF = constrain(angF + 10, 0, 180); updateArm(); }
//   else if (cmd == "F-") { angF = constrain(angF - 10, 0, 180); updateArm(); }
//   else if (cmd == "W+") { angWJ = constrain(angWJ + 10, 0, 180); updateArm(); }
//   else if (cmd == "W-") { angWJ = constrain(angWJ - 10, 0, 180); updateArm(); }
// }

// void syncHand(int tIdx, int tThm, int tRot) {
//   int pIdx = map(tIdx, 0, 180, USMIN, USMAX);
//   int pThm = map(tThm, 0, 180, USMIN, USMAX);
//   int pRot = map(tRot, 0, 180, USMIN, USMAX);

//   while (lastP_Idx != pIdx || lastP_Thm != pThm || lastP_Rot != pRot) {
//     if (lastP_Idx < pIdx) lastP_Idx++; else if (lastP_Idx > pIdx) lastP_Idx--;
//     if (lastP_Thm < pThm) lastP_Thm++; else if (lastP_Thm > pThm) lastP_Thm--;
//     if (lastP_Rot < pRot) lastP_Rot++; else if (lastP_Rot > pRot) lastP_Rot--;

//     pwm.setPWM(CH_INDEX, 0, lastP_Idx);
//     pwm.setPWM(CH_THUMB, 0, lastP_Thm);
//     pwm.setPWM(CH_ROT_W, 0, lastP_Rot);
//     delay(SPEED);
//     baseStepper.run(); 
//   }
//   angIndex = tIdx; angThumb = tThm; angRotW = tRot;
// }

// void updateArm() {
//   // Reverse logic applied to shoulder (USMAX to USMIN)
//   pwm.setPWM(S_SHOULDER, 0, map(angS, 0, 180, USMAX, USMIN)); 
//   pwm.setPWM(S_FOREARM, 0, map(angF, 0, 180, USMIN, USMAX));
//   pwm.setPWM(S_WRIST_J, 0, map(angWJ, 0, 180, USMIN, USMAX));
// }

// void sendHTML(WiFiClient c) {
//   c.println("HTTP/1.1 200 OK\nContent-Type: text/html\n");
//   c.print("<html><head><meta name='viewport' content='width=device-width, initial-scale=1'></head>");
//   c.print("<body style='text-align:center; font-family:sans-serif;'><h1>Project Arachnae</h1>");
//   c.print("<a href='/G'><button style='padding:30px; width:90%; background:red; color:white; font-size:20px;'>GRIP</button></a><br><br>");
//   c.print("<a href='/R'><button style='padding:30px; width:90%; background:green; color:white; font-size:20px;'>RELEASE</button></a>");
//   c.print("</body></html>");
// }

// void displayIP(IPAddress ip) {
//   String ipStr = "  " + ip.toString() + "  ";
//   matrix.beginDraw();
//   matrix.stroke(0xFFFFFFFF);
//   matrix.textScrollSpeed(60);
//   matrix.textFont(Font_5x7);
//   matrix.beginText(0, 1, 0xFFFFFFFF);
//   matrix.println(ipStr);
//   matrix.endText(SCROLL_LEFT);
//   matrix.endDraw();
// }



#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <AccelStepper.h>

// --- PCA9685 PIN MAPPING ---
// Arm Segments (DS3218)
#define S_SHOULDER 0  
#define S_WRIST_J  1  
#define S_ELBOW    2  // CHANGED NAME TO MATCH PYTHON

// Hand/Gripper (MG996R)
#define CH_THUMB   4  
#define CH_ROT_W   5  
#define CH_INDEX   6  

// --- STEPPER CONFIG ---
#define STEPPER_PUL_PIN 3
#define STEPPER_DIR_PIN 2

// --- CALIBRATION ---
#define USMIN  100 
#define USMAX  580 
const int SPEED = 5; 

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
AccelStepper baseStepper(AccelStepper::DRIVER, STEPPER_PUL_PIN, STEPPER_DIR_PIN);

// --- STATE TRACKING ---
int angIndex = 0, angThumb = 60, angRotW = 0; 
int angS = 90, angE = 90, angWJ = 90; // CHANGED angF -> angE
int lastP_Idx = 100, lastP_Thm = 266, lastP_Rot = 100;

void setup() {
  Serial.begin(115200); 
  
  pwm.begin();
  pwm.setPWMFreq(50);

  baseStepper.setMaxSpeed(1500);
  baseStepper.setAcceleration(800);

  // Initialize to Neutral Position
  syncHand(0, 60, 0); 
  updateArm();
  
  Serial.println("--- ARACHNAE WIRED SYSTEM READY ---");
  Serial.println("Commands: G/R (Hand), S+/S- (Shoulder), E+/E- (Elbow), W+/W- (WristJ)");
}

void loop() {
  baseStepper.run(); 

  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim(); 
    processCommand(cmd);
  }
}

void processCommand(String cmd) {
  // DEBUG: Print what we received to prove Python is talking to us
  // Serial.print("RECEIVED: "); Serial.println(cmd);

  // --- GRIPPER COMMANDS ---
  if (cmd == "G") syncHand(180, 60, 180);      
  else if (cmd == "R") syncHand(0, 60, 0);     
  
  // --- BASE STEPPER COMMANDS ---
  else if (cmd == "B_LEFT") baseStepper.move(-400);
  else if (cmd == "B_RIGHT") baseStepper.move(400);

  // --- ARM SEGMENT COMMANDS ---
  // SHOULDER
  else if (cmd == "S+") { 
    angS = constrain(angS + 5, 0, 180); 
    updateArm(); 
    Serial.print("Shoulder: "); Serial.println(angS);
  }
  else if (cmd == "S-") { 
    angS = constrain(angS - 5, 0, 180); 
    updateArm(); 
    Serial.print("Shoulder: "); Serial.println(angS);
  }

  // ELBOW (Formerly Forearm) - Fixed to listen for "E"
  else if (cmd == "E+") { 
    angE = constrain(angE + 5, 0, 180); 
    updateArm(); 
    Serial.print("Elbow: "); Serial.println(angE);
  }
  else if (cmd == "E-") { 
    angE = constrain(angE - 5, 0, 180); 
    updateArm(); 
    Serial.print("Elbow: "); Serial.println(angE);
  }

  // WRIST JOINT
  else if (cmd == "W+") { 
    angWJ = constrain(angWJ + 5, 0, 180); 
    updateArm(); 
    Serial.print("WristJ: "); Serial.println(angWJ);
  }
  else if (cmd == "W-") { 
    angWJ = constrain(angWJ - 5, 0, 180); 
    updateArm(); 
    Serial.print("WristJ: "); Serial.println(angWJ);
  }
}

void syncHand(int tIdx, int tThm, int tRot) {
  int pIdx = map(tIdx, 0, 180, USMIN, USMAX);
  int pThm = map(tThm, 0, 180, USMIN, USMAX);
  int pRot = map(tRot, 0, 180, USMIN, USMAX);

  while (lastP_Idx != pIdx || lastP_Thm != pThm || lastP_Rot != pRot) {
    if (lastP_Idx < pIdx) lastP_Idx++; else if (lastP_Idx > pIdx) lastP_Idx--;
    if (lastP_Thm < pThm) lastP_Thm++; else if (lastP_Thm > pThm) lastP_Thm--;
    if (lastP_Rot < pRot) lastP_Rot++; else if (lastP_Rot > pRot) lastP_Rot--;

    pwm.setPWM(CH_INDEX, 0, lastP_Idx);
    pwm.setPWM(CH_THUMB, 0, lastP_Thm);
    pwm.setPWM(CH_ROT_W, 0, lastP_Rot);
    delay(SPEED);
    baseStepper.run(); 
  }
  angIndex = tIdx; angThumb = tThm; angRotW = tRot;
}

void updateArm() {
  // Pin 0: Shoulder | Pin 1: Wrist Joint | Pin 2: Elbow
  pwm.setPWM(S_SHOULDER, 0, map(angS, 0, 180, USMAX, USMIN)); // Reversed?
  pwm.setPWM(S_WRIST_J, 0, map(angWJ, 0, 180, USMIN, USMAX));
  pwm.setPWM(S_ELBOW,   0, map(angE, 0, 180, USMIN, USMAX));
}