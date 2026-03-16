# Bio-Bubbler

Bio-Bubbler is an ESP32-C3 Super Mini-based pump controller with:
- RGB status LED
- two relay-driven pumps
- two buttons (Mode and Confirm/Select)
- a 0.96 inch SPI OLED status screen
- built-in Wi-Fi AP + web UI for per-pump pulse calibration

## Features

- Three modes: `IDLE`, `PULSE`, `CONTINUOUS`
- Pending mode selection with flashing LED/text hints
- Independent pulse durations for Pump 1 and Pump 2
- NVS persistence for pump calibration values
- Continuous-mode brewing timer on OLED (`dd hh mm`)
- Idle OLED info page toggle (SSID/password/IP)

## Hardware Pinout

| Function | GPIO |
|---|---|
| LED Red | 0 |
| LED Green | 1 |
| LED Blue | 2 |
| Pump 1 relay | 20 |
| Pump 2 relay | 21 |
| OLED SCLK (`D0`) | 5 |
| OLED MOSI (`D1`) | 4 |
| OLED RST (`RES`) | 8 |
| OLED DC | 7 |
| OLED CS | 6 |
| Mode button | 3 |
| Confirm/Select button | 10 |

Buttons are active-low (button to GND, internal pull-up enabled).

## State Behavior

### Confirmed states

| State | LED | Pumps | OLED |
|---|---|---|---|
| `IDLE` | Solid Green | Off | `Idle` |
| `PULSE` | Solid Red | Off until triggered | `Tap` (or `Pouring` while active pulse runs) |
| `CONTINUOUS` | Solid Blue | On (or paused by emergency stop) | `Brewing` + timer |

### Pending states (from IDLE via Mode)

| Pending | LED | OLED text |
|---|---|---|
| `PENDING_PULSE` | Flashing Red | Flashing `Tap` |
| `PENDING_CONTINUOUS` | Flashing Blue | Flashing `Brew` |

## Button Behavior

### Mode button (GPIO 3)

- From `IDLE`: cycles pending selection
  - none -> `PENDING_PULSE`
  - `PENDING_PULSE` -> `PENDING_CONTINUOUS`
  - `PENDING_CONTINUOUS` -> back to `IDLE`
- From `PULSE`: switches to `PENDING_CONTINUOUS`
- From `CONTINUOUS`: immediate `IDLE`

### Confirm/Select button (GPIO 10)

- If `PENDING_PULSE`: enters `PULSE`
- If `PENDING_CONTINUOUS`: enters `CONTINUOUS`
- In `PULSE`: starts one pulse run (each pump uses its own duration)
- In `CONTINUOUS`: emergency stop/start toggle
- In `IDLE` with no pending mode: toggles OLED Wi-Fi info page

## OLED Behavior

### Main state view

- `Idle` in idle mode
- `Tap` in pulse mode
- `Pouring` while pulse pumps are active
- `Brewing` in continuous mode with timer line

### Brewing timer

- Format:
  - days shown when non-zero: `DDdHHhMMm`
  - otherwise: `HHhMMm`
- Counts only while continuous pumps are running
- Pauses when continuous emergency stop is active
- Resumes when pumps restart
- Resets when leaving continuous mode

### Idle info page (toggle with Confirm in IDLE)

Shows:
- SSID: `BioBubbler`
- Password: `bubbler123`
- IP: `192.168.4.1`

Press Confirm again to return to `Idle` display.

## Wi-Fi Calibration UI

- AP SSID: `BioBubbler`
- AP password: `bubbler123`
- Web URL: `http://192.168.4.1`

Presets:
- 1.5L -> 9000 ms
- 2L -> 12000 ms
- 5L -> 30000 ms

Custom values are allowed in the range `1..300000` ms and are saved to NVS.

## Pulse Timing Calibration

1. Connect to `BioBubbler` Wi-Fi AP.
2. Open `http://192.168.4.1`.
3. Start with a preset for each pump.
4. Run pulse cycles and measure output.
5. Adjust per pump with:

`new_duration_ms = current_duration_ms * target_volume / measured_volume`

Example:
- current = `30000`
- target = `5.0L`
- measured = `4.6L`

`new_duration_ms = 30000 * 5.0 / 4.6 = 32608`

## HTTP Endpoints

- `GET /status` -> current pump durations JSON
- `GET /set?pump=1&dur=12000` -> set Pump 1
- `GET /set?pump=2&dur=9000` -> set Pump 2

## Build and Flash

```bash
idf.py build
idf.py flash
idf.py monitor
```

## Notes

- Relay control is active-high (`1 = ON`, `0 = OFF`)
- LED flash interval is 500 ms
- Button debounce is 20 ms
- Main control loop interval is 10 ms
