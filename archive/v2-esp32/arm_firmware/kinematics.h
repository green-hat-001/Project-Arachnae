#pragma once
// ============================================================
//  PROJET ARACHNAE v2 — Kinematics + Body Collision Avoidance
//
//  MOUNT CONTEXT
//  ─────────────
//  The arm is bolted to the RIGHT shoulder blade via a kydex
//  socket. The base cycloidal axis points straight UP (+Z).
//  Rotating the base sweeps the arm around the wearer's right
//  side and forward — like an arm reaching around the body.
//
//  IK STRATEGY
//  ───────────
//  Identical 3R geometric solver to v1, but:
//    1. Joint limits on base constrain motion to the safe arc
//       (forward-right → forward-left, NOT behind the back).
//    2. Every solution is passed through the body collision
//       checker before being accepted. If it fails, we try
//       the alternate elbow configuration, then give up.
//    3. A dedicated HANDOFF pose delivers the object to the
//       wearer's right hand at chest height.
//
//  COLLISION MODEL
//  ───────────────
//  Body parts modelled as primitive volumes in base frame:
//    • Torso    → vertical infinite capsule (cylinder)
//    • Head     → sphere
//    • L shoulder→ sphere
//    • Neck     → sphere
//  Each arm link is tested as a line segment against all
//  volumes. COLLISION_SEGMENTS intermediate points are sampled
//  per link for efficiency (vs full ray-capsule intersection).
// ============================================================

#include <math.h>
#include "config.h"

// ── Simple 3D vector ─────────────────────────────────────────
struct Vec3 {
  float x, y, z;
  Vec3 operator+(const Vec3& b) const { return {x+b.x, y+b.y, z+b.z}; }
  Vec3 operator-(const Vec3& b) const { return {x-b.x, y-b.y, z-b.z}; }
  Vec3 operator*(float s)       const { return {x*s,   y*s,   z*s};   }
  float dot(const Vec3& b)      const { return x*b.x + y*b.y + z*b.z; }
  float norm()                  const { return sqrtf(x*x + y*y + z*z); }
};

struct JointAngles {
  float base;
  float shoulder;
  float elbow;
  float forearm;
};

struct TargetPose {
  float x_mm, y_mm, z_mm;
  float tool_pitch_rad = 0.0f;
  float confidence     = 1.0f;
};

// ── IK result codes ──────────────────────────────────────────
enum class IKResult : uint8_t {
  OK,
  OUT_OF_REACH,
  JOINT_LIMIT,
  BODY_COLLISION,
  NO_SOLUTION
};

// ── Effective collision radius (with margin) ─────────────────
static constexpr float R_TORSO  = BODY_TORSO_RADIUS_MM    + COLLISION_MARGIN_MM;
static constexpr float R_HEAD   = BODY_HEAD_RADIUS_MM     + COLLISION_MARGIN_MM;
static constexpr float R_LSHO   = BODY_LEFT_SHOULDER_RADIUS_MM + COLLISION_MARGIN_MM;
static constexpr float R_NECK   = BODY_NECK_RADIUS_MM     + COLLISION_MARGIN_MM;

// ============================================================
//  forwardKinematics
//  Returns the 4 joint positions in base frame:
//    [0] = base origin (always 0,0,0)
//    [1] = shoulder joint
//    [2] = elbow joint
//    [3] = wrist / gripper face
// ============================================================
inline void forwardKinematicsFull(float base, float shoulder,
                                   float elbow,  float forearm,
                                   Vec3 pts[4]) {
  float a1 = shoulder;
  float a2 = shoulder + elbow;
  float a3 = shoulder + elbow + forearm;

  float cb = cosf(base), sb = sinf(base);

  // Radial distances in the arm's vertical plane
  float r1 = L1_MM * cosf(a1);
  float r2 = r1 + L2_MM * cosf(a2);
  float r3 = r2 + L3_MM * cosf(a3);

  float h1 = L1_MM * sinf(a1);
  float h2 = h1 + L2_MM * sinf(a2);
  float h3 = h2 + L3_MM * sinf(a3);

  pts[0] = {0.0f, 0.0f, 0.0f};
  pts[1] = {r1*cb, r1*sb, h1};
  pts[2] = {r2*cb, r2*sb, h2};
  pts[3] = {r3*cb, r3*sb, h3};
}

inline Vec3 forwardKinematics(float base, float shoulder,
                               float elbow,  float forearm) {
  Vec3 pts[4];
  forwardKinematicsFull(base, shoulder, elbow, forearm, pts);
  return pts[3];
}

// ============================================================
//  Body Collision Checker
//  Tests a LINE SEGMENT (p0→p1) against body-part volumes.
//  Samples COLLISION_SEGMENTS intermediate points per segment.
//  Returns true if the segment COLLIDES with any body part.
// ============================================================

// Point-to-infinite-vertical-cylinder distance (XY plane only)
inline float distToCylinderAxis(const Vec3& p,
                                 float cx, float cy) {
  float dx = p.x - cx, dy = p.y - cy;
  return sqrtf(dx*dx + dy*dy);
}

// Point-to-sphere distance
inline float distToSphere(const Vec3& p,
                           float sx, float sy, float sz) {
  float dx=p.x-sx, dy=p.y-sy, dz=p.z-sz;
  return sqrtf(dx*dx + dy*dy + dz*dz);
}

inline bool segmentCollidesBody(const Vec3& p0, const Vec3& p1) {
  for (int i = 0; i <= COLLISION_SEGMENTS; i++) {
    float t = (float)i / COLLISION_SEGMENTS;
    Vec3 p = p0 + (p1 - p0) * t;

    // The base mount is physically on the scapula — points within 80mm
    // of the mount origin are inside the housing, not a real collision.
    float rMount = sqrtf(p.x*p.x + p.y*p.y + p.z*p.z);
    if (rMount < 80.0f) continue;

    // ── Torso cylinder ─────────────────────────────────────
    if (p.z >= BODY_TORSO_BOTTOM_Z_MM && p.z <= BODY_TORSO_TOP_Z_MM) {
      float d = distToCylinderAxis(p, BODY_SPINE_X_MM, BODY_SPINE_Y_MM);
      if (d < R_TORSO) return true;
    }

    // ── Head sphere ────────────────────────────────────────
    if (distToSphere(p, BODY_HEAD_X_MM,
                        BODY_HEAD_Y_MM,
                        BODY_HEAD_Z_MM) < R_HEAD) return true;

    // ── Left shoulder sphere ───────────────────────────────
    if (distToSphere(p, BODY_LEFT_SHOULDER_X_MM,
                        BODY_LEFT_SHOULDER_Y_MM,
                        BODY_LEFT_SHOULDER_Z_MM) < R_LSHO) return true;

    // ── Neck sphere ────────────────────────────────────────
    if (distToSphere(p, BODY_NECK_X_MM,
                        BODY_NECK_Y_MM,
                        BODY_NECK_Z_MM) < R_NECK) return true;
  }
  return false;
}

// Check all three arm links for body collision
// seg0 (mount→shoulder) skips torso — it is physically attached to the body.
// All segments checked against head, neck, and left shoulder.
inline bool poseCollidesBody(const JointAngles& a) {
  Vec3 pts[4];
  forwardKinematicsFull(a.base, a.shoulder, a.elbow, a.forearm, pts);

  for (int seg = 0; seg < 3; seg++) {
    const Vec3& p0 = pts[seg];
    const Vec3& p1 = pts[seg + 1];

    for (int i = 0; i <= COLLISION_SEGMENTS; i++) {
      float t = (float)i / COLLISION_SEGMENTS;
      Vec3 p = p0 + (p1 - p0) * t;

      // Ignore points within the mount housing
      float rMount = sqrtf(p.x*p.x + p.y*p.y + p.z*p.z);
      if (rMount < 80.0f) continue;

      // Torso cylinder — only checked on seg1 and seg2 (not seg0).
      // The upper arm (seg0) is bolted to the body; it cannot self-collide.
      if (seg >= TORSO_CHECK_START_SEG) {
        if (p.z >= BODY_TORSO_BOTTOM_Z_MM && p.z <= BODY_TORSO_TOP_Z_MM) {
          float d = distToCylinderAxis(p, BODY_SPINE_X_MM, BODY_SPINE_Y_MM);
          if (d < R_TORSO) return true;
        }
      }

      // Head, neck, left shoulder — checked on ALL segments
      if (distToSphere(p, BODY_HEAD_X_MM,
                          BODY_HEAD_Y_MM,
                          BODY_HEAD_Z_MM) < R_HEAD)  return true;
      if (distToSphere(p, BODY_LEFT_SHOULDER_X_MM,
                          BODY_LEFT_SHOULDER_Y_MM,
                          BODY_LEFT_SHOULDER_Z_MM) < R_LSHO) return true;
      if (distToSphere(p, BODY_NECK_X_MM,
                          BODY_NECK_Y_MM,
                          BODY_NECK_Z_MM) < R_NECK)  return true;
    }
  }
  return false;
}

// ============================================================
//  solveIK
//  Returns IKResult code. Fills JointAngles on OK.
//  tool_pitch_rad: 0 = horizontal, negative = tilted down.
// ============================================================
inline IKResult solveIK(float x_mm, float y_mm, float z_mm,
                         JointAngles& out,
                         float tool_pitch_rad = 0.0f,
                         bool elbow_down = true) {

  // ── 1. Base angle ─────────────────────────────────────────
  float base    = atan2f(y_mm, x_mm);
  float baseDeg = base * (180.0f / M_PI);

  // Wrap to [0°, 360°) for limit check
  // Base limits: -30° to +130° (forward-right to left-forward)
  // This prevents reaching behind the wearer's back.
  if (baseDeg < BASE_MIN_DEG || baseDeg > BASE_MAX_DEG) {
    return IKResult::JOINT_LIMIT;
  }

  // ── 2. Radial reach and height ───────────────────────────
  float r = sqrtf(x_mm*x_mm + y_mm*y_mm);
  float h = z_mm;
  float phi = tool_pitch_rad;

  // ── 3. Wrist position (subtract L3 contribution) ─────────
  float wx = r - L3_MM * cosf(phi);
  float wz = h - L3_MM * sinf(phi);

  // ── 4. 2R subproblem ─────────────────────────────────────
  float D = (wx*wx + wz*wz - L1_MM*L1_MM - L2_MM*L2_MM)
            / (2.0f * L1_MM * L2_MM);

  if (D < -1.0f || D > 1.0f) return IKResult::OUT_OF_REACH;

  float sinTheta2 = elbow_down ? -sqrtf(1.0f - D*D)
                               :  sqrtf(1.0f - D*D);
  float theta2 = atan2f(sinTheta2, D);
  float theta1 = atan2f(wz, wx)
               - atan2f(L2_MM * sinTheta2, L1_MM + L2_MM * D);
  float theta3 = phi - theta1 - theta2;

  // ── 5. Joint limit checks ─────────────────────────────────
  auto deg = [](float r) { return r * (180.0f / M_PI); };
  auto inLim = [](float v, float lo, float hi) {
    return v >= lo && v <= hi;
  };

  if (!inLim(deg(theta1), SHOULDER_MIN_DEG, SHOULDER_MAX_DEG) ||
      !inLim(deg(theta2), ELBOW_MIN_DEG,    ELBOW_MAX_DEG)    ||
      !inLim(deg(theta3), FOREARM_MIN_DEG,  FOREARM_MAX_DEG)) {

    // Retry with opposite elbow config
    if (elbow_down) {
      return solveIK(x_mm, y_mm, z_mm, out, tool_pitch_rad, false);
    }
    return IKResult::JOINT_LIMIT;
  }

  // ── 6. Body collision check ───────────────────────────────
  JointAngles candidate = {base, theta1, theta2, theta3};
  if (poseCollidesBody(candidate)) {
    if (elbow_down) {
      return solveIK(x_mm, y_mm, z_mm, out, tool_pitch_rad, false);
    }
    return IKResult::BODY_COLLISION;
  }

  // ── 7. Accept solution ────────────────────────────────────
  out.base     = base;
  out.shoulder = theta1;
  out.elbow    = theta2;
  out.forearm  = theta3;
  return IKResult::OK;
}

// ============================================================
//  Handoff pose — pre-computed safe delivery position
//  Returns the JointAngles to present object to right hand
// ============================================================
inline IKResult solveHandoff(JointAngles& out) {
  float pitch = HANDOFF_TOOL_PITCH_DEG * (M_PI / 180.0f);
  return solveIK(HANDOFF_X_MM, HANDOFF_Y_MM, HANDOFF_Z_MM,
                 out, pitch);
}

// ============================================================
//  Pixel → world (camera on gripper, pointing forward)
//  Intrinsics: OV2640 on XIAO ESP32S3 Sense, SVGA 800×600
// ============================================================
inline void pixelToWorld(float px, float py, float depth_mm,
                         float& wx, float& wy, float& wz) {
  float nx = (px - CAM_CX) / CAM_FX;
  float ny = (py - CAM_CY) / CAM_FY;

  // Camera-frame point
  float cx =  nx * depth_mm;
  float cy =  ny * depth_mm;
  float cz =  depth_mm;

  // Camera tilts with the gripper; approximate as arm-axis-aligned
  // Full FK-based transform should replace this after calibration.
  wx =  cz;
  wy = -cx;
  wz = -cy;
}

// ============================================================
//  IKResult to string (for Serial debug)
// ============================================================
inline const char* ikResultStr(IKResult r) {
  switch (r) {
    case IKResult::OK:             return "OK";
    case IKResult::OUT_OF_REACH:   return "OUT_OF_REACH";
    case IKResult::JOINT_LIMIT:    return "JOINT_LIMIT";
    case IKResult::BODY_COLLISION: return "BODY_COLLISION";
    default:                       return "NO_SOLUTION";
  }
}
