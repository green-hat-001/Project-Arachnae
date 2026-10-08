#pragma once
// ============================================================
//  BASE SUBSYSTEM CONFIGURATION
//  1:20 cycloidal drive, ST3215 servo, Mode 0 operation
// ============================================================

// ── Servo Hardware ───────────────────────────────────────────
#define SERVO_BASE_ID            1
#define SERVO_STEPS_PER_REV   4096      // ST3215 12-bit resolution
#define SERVO_STEPS_CENTRE    2048

// ── Gear Ratio ───────────────────────────────────────────────
#define RATIO_BASE           20.0f      // 1:20 cycloidal reduction

// ── Joint Limits (output shaft degrees) ──────────────────────
#define BASE_MIN_DEG        -30.0f
#define BASE_MAX_DEG        130.0f

// ── Speed Configuration ──────────────────────────────────────
// SPEED_PERCENT: 0–100, percentage of ST3215 max speed.
// ST3215 @ 12V no-load: 545°/sec servo speed = 27.25°/sec output
// 100% = full speed (5.87s for full 160° travel)
//  50% = half speed (11.74s for full travel)
//  30% = conservative (19.6s for full travel)
#define SPEED_PERCENT       100

// ── ST3215 Specifications ────────────────────────────────────
// Max speed: 545°/sec (0.11 sec/60° @ 12V no-load)
// Stall current: 2.5A @ 12V
// Running current: 500-700mA typical
// Idle current: 50-100mA
// Torque: 23.5 kg·cm @ 12V

// ── Communication ────────────────────────────────────────────
#define SERVO_BAUD        1000000       // 1 Mbaud
#define SERVO_TIMEOUT_MS       150      // Read timeout

// ── Home Position ────────────────────────────────────────────
#define HOME_BASE_DEG         0.0f      // Centre of range

// ── Motion Tolerances ────────────────────────────────────────
#define POSITION_TOLERANCE_STEPS  5     // ±5 steps = ±0.4° servo = ±0.02° output
#define MOTION_TIMEOUT_MS      8000     // 8s max motion time
