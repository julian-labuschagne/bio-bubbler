# Bio-Bubbler

Bio-Bubbler is a pump control system using an ESP32 WROOM microcontroller with RGB LED status indication and relay-controlled pumps.

## Overview

This project controls a machine with two pumps via relays. An RGB LED provides visual feedback about the machine's current state. The system has three operating modes:

- **IDLE (Green)** — Pumps off, safe to work and fiddle with the machine
- **CONTINUOUS (Blue)** — Both pumps running continuously (toggled via button)
- **PULSE (Red)** — Pumps run for configurable per-pump durations

## Hardware Pinout

### Status LED (RGB)
- **Red LED** — GPIO 2
- **Green LED** — GPIO 4
- **Blue LED** — GPIO 5

### Pump Control (via Relays)
- **Pump 1** — GPIO 12
- **Pump 2** — GPIO 13

### Buttons
- **Mode Button** — GPIO 32
- **Confirm Button** — GPIO 33

## States

The machine operates in one of three confirmed states, with a pending state system for safe state transitions.

### Confirmed States

| State | LED Color | Pump 1 | Pump 2 | Description |
|-------|-----------|--------|--------|-------------|
| `STATE_IDLE` | Solid Green | OFF | OFF | Safe idle state, no pump operation |
| `STATE_CONTINUOUS` | Solid Blue | ON* | ON* | Both pumps running continuously |
| `STATE_PULSE` | Solid Red | OFF (until trigger) | OFF (until trigger) | Pumps ready for manual configured cycles |

*Emergency stop/start available in CONTINUOUS mode

### Pending States (LED Flashing)

| Pending State | LED Color | Action | Next |
|---------------|-----------|--------|------|
| `PENDING_PULSE` | Flashing Red | Press Mode to cycle | PENDING_CONTINUOUS |
| `PENDING_CONTINUOUS` | Flashing Blue | Press Mode to return to IDLE | IDLE (confirmed) |

## Button Behavior

### Mode Button (GPIO 32)
Behavior:
- **From IDLE:** Press → Flashing RED (PULSE pending)
- **From Flashing RED:** Press → Flashing BLUE (CONTINUOUS pending)
- **From Flashing BLUE:** Press → IDLE (confirmed, no flash)
- **From PULSE (active state):** Press → Flashing BLUE (CONTINUOUS pending)
- **From CONTINUOUS (active state):** Press → Immediate IDLE

### Confirm Button (GPIO 33)
Confirms pending state or executes action:
- **Flashing RED:** Press → Solid RED (PULSE confirmed, pumps ready)
- **Flashing BLUE:** Press → Solid BLUE (CONTINUOUS confirmed, pumps active)
- **Solid RED (PULSE):** Press → Triggers pulse cycle with per-pump configured durations
- **Solid BLUE (CONTINUOUS):** Press → Emergency stop/start toggle (pumps on/off)
- **Solid GREEN (IDLE):** Does nothing

## Operation Sequence Example

1. **Power on** → IDLE (solid green, safe)
2. **Press Mode** → Pending PULSE (flashing red)
3. **Press Confirm** → PULSE confirmed (solid red, pumps ready)
4. **Press Confirm** → Pump cycle starts (both pumps on)
5. **Wait until each timer expires** → Each pump turns off independently
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
- Independent pulse timers for Pump 1 and Pump 2
- Web-based pulse configuration (SoftAP + HTTP UI)
- Presets for 1.5L, 2L, and 5L plus custom millisecond values
- Pulse duration persistence using NVS (survives reboot)
- Emergency stop/start toggle for CONTINUOUS mode
- Button press detection with 10ms check interval

## Pulse Timing Setup

Use this procedure to calibrate each pump so both output the same target volume.

### 1. Connect To The Device

1. Power on the ESP32 running Bio-Bubbler firmware.
2. Connect your phone/laptop to Wi-Fi:
	- SSID: `BioBubbler`
	- Password: `bubbler123`
3. Open `http://192.168.4.1` in your browser.

### 2. Choose A Starting Preset

Each pump can be set independently.

| Preset | Duration |
|--------|----------|
| 1.5L | 9000 ms |
| 2L | 12000 ms |
| 5L | 30000 ms |

Press a preset button under Pump 1 and Pump 2 to apply it.

### 3. Fine Tune With Custom Values

1. Enter custom values in milliseconds for each pump.
2. Click **Save Custom**.
3. Values are saved to NVS automatically and persist after reboot.

Allowed range is `1` to `300000` ms.

### 4. Test In Pulse Mode

1. Press **Mode** until PULSE is pending (flashing red).
2. Press **Confirm** to enter PULSE (solid red).
3. Press **Confirm** again to run one pulse cycle.
4. Pump 1 and Pump 2 will stop independently when their timers expire.

### 5. Calibration Formula

If a pump under-delivers or over-delivers, update its duration with:

`new_duration_ms = current_duration_ms * target_volume / measured_volume`

Example: target is 5.0L, measured is 4.6L at 30000 ms.

`new_duration_ms = 30000 * 5.0 / 4.6 = 32608 ms`

Apply the new value in the web UI and test again.

### Optional API Endpoints

- `GET /status` returns current durations, for example: `{"pump1":30000,"pump2":30000}`
- `GET /set?pump=1&dur=12000` sets Pump 1 duration
- `GET /set?pump=2&dur=9000` sets Pump 2 duration

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
- Pulse durations are configurable per pump and stored in NVS
- Emergency stop/start is only available in CONTINUOUS mode (press Confirm to toggle)
- System starts in IDLE state for safe operation
- Relay control assumes active-high logic (1 = ON, 0 = OFF)
