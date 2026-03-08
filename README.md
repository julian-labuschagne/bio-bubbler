# Bio-Bubbler

Bio-Bubbler is a pump control system using an ESP32 WROOM microcontroller with RGB LED status indication and relay-controlled pumps.

## Overview

This project controls a machine with two pumps via relays. An RGB LED provides visual feedback about the machine's current state. The system has three operating modes:

- **IDLE (Green)** — Pumps off, safe to work and fiddle with the machine
- **CONTINUOUS (Blue)** — Both pumps running continuously (toggled via button)
- **PULSE (Red)** — Pumps running briefly (few seconds via button)

## Hardware Pinout

### Status LED (RGB)
- **Red LED** — GPIO 2
- **Green LED** — GPIO 4
- **Blue LED** — GPIO 5

### Pump Control (via Relays)
- **Pump 1** — GPIO 12
- **Pump 2** — GPIO 13

### Buttons
- **Mode Button** — GPIO 34 (cycles pending states)
- **Confirm Button** — GPIO 35 (confirms selection or executes action)

## States

The machine operates in one of three confirmed states, with a pending state system for safe state transitions.

### Confirmed States

| State | LED Color | Pump 1 | Pump 2 | Description |
|-------|-----------|--------|--------|-------------|
| `STATE_IDLE` | Solid Green | OFF | OFF | Safe idle state, no pump operation |
| `STATE_CONTINUOUS` | Solid Blue | ON* | ON* | Both pumps running continuously |
| `STATE_PULSE` | Solid Red | OFF (until trigger) | OFF (until trigger) | Pumps ready for manual 30s cycles |

*Emergency stop/start available in CONTINUOUS mode

### Pending States (LED Flashing)

| Pending State | LED Color | Action | Next |
|---------------|-----------|--------|------|
| `PENDING_PULSE` | Flashing Red | Press Mode to cycle | PENDING_CONTINUOUS |
| `PENDING_CONTINUOUS` | Flashing Blue | Press Mode to return to IDLE | IDLE (confirmed) |

## Button Behavior

### Mode Button (GPIO 34)
Cycles through pending states (flashing LEDs):
- **From IDLE:** Press → Flashing RED (PULSE pending)
- **From Flashing RED:** Press → Flashing BLUE (CONTINUOUS pending)
- **From Flashing BLUE:** Press → IDLE (confirmed, no flash)

### Confirm Button (GPIO 35)
Confirms pending state or executes action:
- **Flashing RED:** Press → Solid RED (PULSE confirmed, pumps ready)
- **Flashing BLUE:** Press → Solid BLUE (CONTINUOUS confirmed, pumps active)
- **Solid RED (PULSE):** Press → Triggers 30-second pump cycle
- **Solid BLUE (CONTINUOUS):** Press → Emergency stop/start toggle (pumps on/off)
- **Solid GREEN (IDLE):** Does nothing

## Operation Sequence Example

1. **Power on** → IDLE (solid green, safe)
2. **Press Mode** → Pending PULSE (flashing red)
3. **Press Confirm** → PULSE confirmed (solid red, pumps ready)
4. **Press Confirm** → 30s pump cycle starts (pumps on)
5. **Wait 30s** → Pumps turn off automatically
6. **Press Mode** → Pending CONTINUOUS (flashing blue)
7. **Press Mode** → Back to IDLE (confirmed, solid green)
8. **OR Press Confirm** → CONTINUOUS confirmed (solid blue, pumps active)
9. **Press Confirm** → Emergency stop (pumps off, LED stays blue)
10. **Press Confirm** → Emergency start (pumps on again)

## Current Implementation

- GPIO initialization and configuration
- State management via enum and state function
- RGB LED feedback for all confirmed states
- LED flashing for pending states (500ms flash interval)
- Pump relay control
- Button handling with debouncing (20ms)
- 30-second pulse timer for PULSE mode
- Emergency stop/start toggle for CONTINUOUS mode
- Button press detection with 10ms check interval

## TODO

- Consider adding status output to serial console for debugging
- Consider adding additional safety features (e.g., watchdog timer)
- Test button reliability and debounce timing in real hardware

## Building and Flashing

```bash
# Build the project
idf.py build

# Flash to device
idf.py flash

# Monitor serial output
idf.py monitor
```

## Notes

- Buttons are assumed to be active-low (pressing connects to GND)
- LED flash interval is 500ms (on for 250ms, off for 250ms)
- Button debounce delay is 20ms to filter electrical noise
- Pulse duration is 30 seconds before pumps automatically shut off
- Emergency stop/start is only available in CONTINUOUS mode (press Confirm to toggle)
- System starts in IDLE state for safe operation
- Relay control assumes active-high logic (1 = ON, 0 = OFF)
