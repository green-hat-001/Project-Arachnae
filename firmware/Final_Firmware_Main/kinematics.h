#pragma once
// ============================================================
//  PROJET ARACHNAE v2 — Kinematics + Body Collision Avoidance
//
//  MOUNT CONTEXT
//  ─────────────
//  The arm is bolted to the RIGHT shoulder blade. Worn, the base
//  (cycloid) axis sticks straight out of the back, roughly level,
//  so the base swings the whole arm sideways in the plane of the
//  back, and the shoulder/elbow/forearm pitch the arm within a
//  plane that contains the base axis.
//
//  TWO FRAMES (config.h "Mount"), same origin (shoulder axis on
//  the base axis):
//    ARM frame  — +Z = base axis, +X = where base angle 0 points
//                 (hanging plumb), +Y = Z x X. FK/IK, joint angles,
//                 tipToBase() and "frame":"base" targets.
//    BODY frame — +X forward, +Y left, +Z up. The body model,
//                 "above" (approach / lift), tool pitch and the
//                 taught height envelope.
//  armToBody() / bodyToArm() convert; upInArm() is "up" in the arm frame.
//
//  IK STRATEGY
//  ───────────
//    1. Base angle from the target's direction around the base axis,
//       limited by the taught left/right sweep.
//    2. Tool pitch is chosen as an elevation in the BODY frame and
//       turned into the arm-plane wrist angle for that base angle.
//    3. Every solution goes through the body collision checker; if
//       it fails, the alternate elbow configuration is tried.
//
//  COLLISION MODEL (body frame, config.h)
//  ───────────────
//    • Torso     → vertical elliptic cylinder, down to the ground
//    • Right arm → vertical cylinder (your own arm, hanging)
//    • Head, neck, both shoulders → spheres
//  Each link is sampled at COLLISION_SEGMENTS points.
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
  Vec3 cross(const Vec3& b)     const { return {y*b.z - z*b.y, z*b.x - x*b.z, x*b.y - y*b.x}; }
  float norm()                  const { return sqrtf(x*x + y*y + z*z); }
};

// ── Mount rotation (arm frame <-> body frame) ────────────────
// The arm frame's axes expressed in the body frame. Z = the base axis
// (tilted/yawed per config), X = "down" projected onto the plane the base
// turns in, so base angle 0 hangs plumb. With the axis straight up
// (tilt 90) there is no "down" in that plane and X falls back to forward,
// which reproduces the old upright-axis model exactly.
struct MountAxes { Vec3 X, Y, Z; };

inline const MountAxes& mountAxes() {
  static const MountAxes m = [] {
    const float t = MOUNT_AXIS_TILT_DEG * (float)M_PI / 180.0f;
    const float w = MOUNT_AXIS_YAW_DEG  * (float)M_PI / 180.0f;
    Vec3 a = { -cosf(t) * cosf(w), -cosf(t) * sinf(w), sinf(t) };
    Vec3 down = {0.0f, 0.0f, -1.0f};
    Vec3 x = down - a * down.dot(a);
    if (x.norm() < 1e-3f) { Vec3 fwd = {1.0f, 0.0f, 0.0f}; x = fwd - a * fwd.dot(a); }
    x = x * (1.0f / x.norm());
    return MountAxes{ x, a.cross(x), a };
  }();
  return m;
}

inline Vec3 armToBody(const Vec3& p) {
  const MountAxes& m = mountAxes();
  return m.X * p.x + m.Y * p.y + m.Z * p.z;
}
inline Vec3 bodyToArm(const Vec3& p) {
  const MountAxes& m = mountAxes();
  return { m.X.dot(p), m.Y.dot(p), m.Z.dot(p) };
}
// Straight up (body frame) expressed in the arm frame.
inline Vec3 upInArm() { return bodyToArm({0.0f, 0.0f, 1.0f}); }
// Height above the origin (body-frame z) of an arm-frame point.
inline float bodyZ(const Vec3& p) { return armToBody(p).z; }

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
  OUTSIDE_ENVELOPE,   // beyond the taught reach / height / depth / sweep limits
  NO_SOLUTION
};

// ── Runtime envelope (filled in from taught positions) ───────
// Until teach data exists these are just the hard config limits.
struct RuntimeEnvelope {
  float baseMinDeg = BASE_MIN_DEG;
  float baseMaxDeg = BASE_MAX_DEG;
  bool  rzValid    = false;   // reach/height/depth taught?
  bool  rValid = false, zMaxValid = false, zMinValid = false;
  float rMax = 0.0f;              // mm, straight-line distance from the origin
  float zMax = 0.0f, zMin = 0.0f; // mm, height (BODY-frame z)
};
static RuntimeEnvelope g_env;

// ── Effective collision sizes (with margin) ──────────────────
static constexpr float A_TORSO  = BODY_TORSO_HALF_DEPTH_MM + COLLISION_MARGIN_MM;
static constexpr float B_TORSO  = BODY_TORSO_HALF_WIDTH_MM + COLLISION_MARGIN_MM;
static constexpr float R_RARM   = BODY_RARM_RADIUS_MM     + COLLISION_MARGIN_MM;
static constexpr float R_HEAD   = BODY_HEAD_RADIUS_MM     + COLLISION_MARGIN_MM;
static constexpr float R_LSHO   = BODY_LEFT_SHOULDER_RADIUS_MM + COLLISION_MARGIN_MM;
static constexpr float R_RSHO   = BODY_RIGHT_SHOULDER_RADIUS_MM + COLLISION_MARGIN_MM;
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
//  Points are converted to the BODY frame and tested against the
//  body volumes (config.h). Each link is sampled at
//  COLLISION_SEGMENTS + 1 points.
// ============================================================

// Point-to-sphere distance
inline float distToSphere(const Vec3& p,
                           float sx, float sy, float sz) {
  float dx=p.x-sx, dy=p.y-sy, dz=p.z-sz;
  return sqrtf(dx*dx + dy*dy + dz*dz);
}

// p is in the BODY frame.
inline bool pointHitsBody(const Vec3& p, bool checkTorso) {
  // ── Torso: vertical elliptic cylinder ─────────────────────
  if (checkTorso && p.z >= BODY_TORSO_BOTTOM_Z_MM && p.z <= BODY_TORSO_TOP_Z_MM) {
    float ex = (p.x - BODY_TORSO_X_MM) / A_TORSO;
    float ey = (p.y - BODY_TORSO_Y_MM) / B_TORSO;
    if (ex*ex + ey*ey < 1.0f) return true;
  }
  // ── Your right arm: vertical cylinder ─────────────────────
  if (p.z >= BODY_RARM_BOTTOM_Z_MM && p.z <= BODY_RARM_TOP_Z_MM) {
    float dx = p.x - BODY_RARM_X_MM, dy = p.y - BODY_RARM_Y_MM;
    if (dx*dx + dy*dy < R_RARM*R_RARM) return true;
  }
  if (distToSphere(p, BODY_HEAD_X_MM, BODY_HEAD_Y_MM, BODY_HEAD_Z_MM) < R_HEAD) return true;
  if (distToSphere(p, BODY_NECK_X_MM, BODY_NECK_Y_MM, BODY_NECK_Z_MM) < R_NECK) return true;
  if (distToSphere(p, BODY_LEFT_SHOULDER_X_MM, BODY_LEFT_SHOULDER_Y_MM,
                      BODY_LEFT_SHOULDER_Z_MM) < R_LSHO) return true;
  if (distToSphere(p, BODY_RIGHT_SHOULDER_X_MM, BODY_RIGHT_SHOULDER_Y_MM,
                      BODY_RIGHT_SHOULDER_Z_MM) < R_RSHO) return true;
  return false;
}

// Check all three arm links for body collision. Links before
// TORSO_CHECK_START_SEG skip the torso test.
inline bool poseCollidesBody(const JointAngles& a) {
  Vec3 pts[4];
  forwardKinematicsFull(a.base, a.shoulder, a.elbow, a.forearm, pts);

  for (int seg = 0; seg < 3; seg++) {
    const Vec3& p0 = pts[seg];
    const Vec3& p1 = pts[seg + 1];

    for (int i = 0; i <= COLLISION_SEGMENTS; i++) {
      float t = (float)i / COLLISION_SEGMENTS;
      Vec3 p = p0 + (p1 - p0) * t;

      // The base mount itself: points within 80 mm of the origin are
      // inside the housing, not a real collision.
      if (p.norm() < 80.0f) continue;

      if (pointHitsBody(armToBody(p), seg >= TORSO_CHECK_START_SEG)) return true;
    }
  }
  return false;
}

// ============================================================
//  solveIK — target in the ARM frame.
//  Returns IKResult code. Fills JointAngles on OK.
//  tool_pitch_rad: the tool's angle in the arm plane (shoulder + elbow +
//  forearm): 0 = pointing radially out from the base axis, +90 deg =
//  along +Z of the arm frame. For a pitch relative to the ground, use
//  solveWithPitches(), which converts body-frame elevations.
// ============================================================
inline IKResult solveIK(float x_mm, float y_mm, float z_mm,
                         JointAngles& out,
                         float tool_pitch_rad = 0.0f,
                         bool elbow_down = true) {

  // ── 1. Base angle ─────────────────────────────────────────
  float base    = atan2f(y_mm, x_mm);
  float baseDeg = base * (180.0f / M_PI);

  // Taught left/right sweep, inside the hard base limits.
  if (baseDeg < g_env.baseMinDeg || baseDeg > g_env.baseMaxDeg) {
    return IKResult::JOINT_LIMIT;
  }

  // ── 2. Radial distance from, and position along, the base axis ──
  float r = sqrtf(x_mm*x_mm + y_mm*y_mm);
  float h = z_mm;
  float phi = tool_pitch_rad;

  // Taught envelope: max reach (distance from the origin), and max / min
  // height in the BODY frame — "height" means up, wherever the base axis points.
  {
    Vec3 p = {x_mm, y_mm, z_mm};
    float zb = bodyZ(p);
    if ((g_env.rValid    && p.norm() > g_env.rMax) ||
        (g_env.zMaxValid && zb > g_env.zMax) ||
        (g_env.zMinValid && zb < g_env.zMin)) {
      return IKResult::OUTSIDE_ENVELOPE;
    }
  }

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
//  Tool-pitch retry — the ONE place the grab's pitch choice lives, so
//  the /test CV panel reports exactly what moveToXYZ() would do.
//  Pitches are tool ELEVATIONS in the body frame (0 = level, -90 =
//  straight down). Tries the height-based guess first, then LOW / MID /
//  HIGH. On failure, returns the result for the first (adaptive) pitch.
// ============================================================
inline float adaptivePitch(float bodyZ_mm) {
  if (bodyZ_mm < -50.0f)  return TOOL_PITCH_LOW_DEG;
  if (bodyZ_mm < 150.0f)  return TOOL_PITCH_MID_DEG;
  return TOOL_PITCH_HIGH_DEG;
}

// The arm-plane tool angles (for solveIK) that point the tool at body-frame
// elevation elevDeg, for the base angle a target at (x, y, z) needs. The tool
// always lies in the arm plane, so its elevation is
//   sin(elev) = cos(phi) * (radial . up) + sin(phi) * (axis . up)
// which has two solutions (or one, if the plane can't tilt the tool that
// far, in which case it gets as close as the plane allows). The one that
// points the tool further out along the target direction comes first.
// Returns how many angles were written to phi[].
inline int armPitchesForElevation(float x, float y, float z, float elevDeg, float phi[2]) {
  const Vec3 up = upInArm();
  const float b = atan2f(y, x);
  const float A = cosf(b) * up.x + sinf(b) * up.y;   // radial direction . up
  const float B = up.z;                               // base axis . up
  const float M = sqrtf(A*A + B*B);
  if (M < 1e-3f) { phi[0] = 0.0f; return 1; }         // plane is level: tool always level
  const float d = atan2f(B, A);
  const float c = fmaxf(-1.0f, fminf(1.0f, sinf(elevDeg * (float)M_PI / 180.0f) / M));
  const float w = acosf(c);
  float p1 = d + w, p2 = d - w;
  auto wrap = [](float a) { while (a > (float)M_PI) a -= 2.0f * (float)M_PI;
                            while (a <= -(float)M_PI) a += 2.0f * (float)M_PI; return a; };
  p1 = wrap(p1); p2 = wrap(p2);
  const float r = sqrtf(x*x + y*y);
  auto outward = [&](float a) { return cosf(a) * r + sinf(a) * z; };
  if (outward(p2) > outward(p1)) { float t = p1; p1 = p2; p2 = t; }
  phi[0] = p1;
  if (w < 1e-4f) return 1;
  phi[1] = p2;
  return 2;
}

// Target in the ARM frame. pitchDeg returns the body-frame elevation used.
inline IKResult solveWithPitches(float x, float y, float z, JointAngles& out, float& pitchDeg) {
  const float pitches[] = { adaptivePitch(bodyZ({x, y, z})), TOOL_PITCH_LOW_DEG, TOOL_PITCH_MID_DEG, TOOL_PITCH_HIGH_DEG };
  IKResult first = IKResult::NO_SOLUTION;
  for (int i = 0; i < 4; i++) {
    float phi[2];
    int n = armPitchesForElevation(x, y, z, pitches[i], phi);
    for (int k = 0; k < n; k++) {
      IKResult r = solveIK(x, y, z, out, phi[k]);
      if (r == IKResult::OK) { pitchDeg = pitches[i]; return r; }
      if (i == 0 && k == 0) first = r;
    }
  }
  pitchDeg = pitches[0];
  return first;
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
    case IKResult::OUTSIDE_ENVELOPE: return "OUTSIDE_ENVELOPE";
    default:                       return "NO_SOLUTION";
  }
}

// ============================================================
//  tipToBase — convert a point measured in the TOOL frame
//  (x forward along the gripper, y left, z up; what
//  cv_pipeline.py sends with "frame":"tip") into the base frame,
//  using the CURRENT measured joint angles.
// ============================================================
inline Vec3 tipToBase(const JointAngles& a, float lx, float ly, float lz) {
  Vec3 tip = forwardKinematics(a.base, a.shoulder, a.elbow, a.forearm);
  float pitch = a.shoulder + a.elbow + a.forearm;   // tool pitch in the arm plane
  float cp = cosf(pitch), sp = sinf(pitch);
  float cb = cosf(a.base), sb = sinf(a.base);
  // tool frame -> arm-plane frame (pitch about the lateral axis)
  float rx = lx * cp - lz * sp;      // radial component
  float rz = lx * sp + lz * cp;      // vertical component
  float ry = ly;                     // lateral component
  // arm-plane frame -> base frame (yaw about +Z)
  return { tip.x + rx * cb - ry * sb,
           tip.y + rx * sb + ry * cb,
           tip.z + rz };
}
