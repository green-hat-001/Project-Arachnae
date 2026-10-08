# Base Subsystem — Final Implementation

**Project Arachnae v2.2** — Cycloidal base drive control system

## Hardware

- **Servo**: ST3215 (ID 1)
- **Reduction**: 1:20 cycloidal drive
- **Range**: -30° to +130° output (160° total, 7.2 servo revolutions)
- **Controller**: Waveshare Serial Bus Servo Driver (USB → half-duplex UART)
- **Protocol**: SCServo half-duplex UART @ 1 Mbaud
- **Power**: 4200mAh 11.1V 35C LiPo battery

## Performance

- **Max output speed**: 27.25°/sec (at 100% SPEED_PERCENT)
- **Full travel time**: 5.87 seconds (160° at max speed)
- **Position resolution**: 0.02° output (5 steps tolerance)
- **Power draw**: 
  - Idle: 0.6-1.2W
  - Running: 6-8.4W
  - Stall: 30W (2.5A @ 12V)
- **Runtime**: 6.7 hours continuous motion (typical), 3.1 hours heavy load

## Operating Mode

**Mode 0 with software revolution tracking**

The ST3215 servo's firmware does not implement true Mode 3 (32-bit multi-turn accumulator) despite accepting MODE=3 register writes. Instead, we use **Mode 0** (standard 16-bit position control) with software-tracked revolution counter.

### Why Mode 0?

1. **Hardware limitation**: Servo reports MODE=3 in registers but only responds to 16-bit position commands
2. **Verified behavior**: Both our code and Waveshare's servo-tui exhibit the same limitation
3. **Simple solution**: Mode 0 position control is reliable; we handle multi-turn math in software

### How it works

- **Position wrapping**: Mode 0 position range is 0-4095 steps per revolution (wraps at 4096)
- **Revolution tracking**: `MultiTurnTracker` detects wrap boundaries and increments/decrements revolution counter
- **Absolute position**: `(revolution × 4096) + position_within_rev`
- **Target calculation**: For any output angle, compute total servo steps, then split into revolution + position components

## Files

### Control Script

**`base_mode0_multiturn.py`** — Python control script (primary interface)

```bash
# Show current state
python base_mode0_multiturn.py --port /dev/ttyACM0 --status

# Move to specific angle
python base_mode0_multiturn.py --port /dev/ttyACM0 --angle 45.0

# Return to home (0°)
python base_mode0_multiturn.py --port /dev/ttyACM0 --home

# Test full-range sweep
python base_mode0_multiturn.py --port /dev/ttyACM0 --test-sweep
```

**Features**:
- Revolution wrap detection
- Speed control via SPEED_PERCENT
- Joint limit enforcement (-30° to +130°)
- Motion timeout protection
- Real-time position tracking

### Configuration

**`config_base.h`** — Base subsystem configuration header

Key parameters:
- `SPEED_PERCENT` (0-100): Speed as percentage of max (default: 100)
- `BASE_MIN_DEG` / `BASE_MAX_DEG`: Joint limits
- `RATIO_BASE`: Gear reduction (20.0 for 1:20)

## Speed Tuning

Edit `SPEED_PERCENT` in `config_base.h`:

```c
#define SPEED_PERCENT       100    // 100% = max speed (5.87s full travel)
                                   //  50% = half speed (11.74s)
                                   //  30% = conservative (19.6s)
```

Then use directly in control script — no recompilation needed for Python control.

## Integration Notes

The Python script provides the control interface for testing and calibration. For integration into the full arm system:

1. **Arduino/C++ integration**: Port `MultiTurnTracker` logic to `servo_control.h`
2. **Persistent state**: Store revolution counter in EEPROM/flash if power-cycle recovery is needed
3. **Homing sequence**: On startup, move to a known hard stop to zero the revolution counter
4. **Collision detection**: Monitor `REG_PRESENT_LOAD` for mechanical binding

## Troubleshooting

**Servo doesn't move**:
- Check torque enable: `REG_TORQUE_ENABLE` (0x28) should be 1
- Verify power: `REG_PRESENT_VOLTAGE` (0x3E) should read ~126 (12.6V)
- Check battery: 11.1V nominal, must be charged

**Position drift**:
- Revolution counter lost on power cycle (expected in Mode 0)
- Solution: Re-home on startup, or implement persistent state storage

**Wrap detection fails**:
- Large position jumps (>3000 steps) trigger wrap detection
- For slow continuous motion across wrap, this works reliably
- For discontinuous jumps (teleport commands), manual revolution management may be needed

## Next Steps

Tomorrow: Other subsystems (shoulder, elbow, forearm, gripper)

---

*Last updated: 2026-09-08*
