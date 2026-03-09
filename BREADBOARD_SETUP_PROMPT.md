# Bio-Bubbler Breadboard Setup Prompt for LLM

Use this prompt with ChatGPT, Claude, or another LLM to get detailed breadboard wiring instructions.

---

## Prompt

I am building Bio-Bubbler with an ESP32 WROOM. Help me wire this on a breadboard from scratch.

### System Overview
- ESP32 WROOM
- RGB LED for machine status
- Two push buttons (Mode and Confirm/Select)
- Two 5V relay channels for Pump 1 and Pump 2
- 0.96 inch SPI OLED display with pins: `GND VCC D0 D1 RES DC CS`
- External 5V for relays and load, shared GND with ESP32

### Firmware Pin Mapping

GPIO outputs:
- GPIO 2 -> LED Red
- GPIO 4 -> LED Green
- GPIO 5 -> LED Blue
- GPIO 12 -> Relay 1 (Pump 1)
- GPIO 13 -> Relay 2 (Pump 2)

GPIO inputs:
- GPIO 32 -> Mode button (active-low)
- GPIO 33 -> Confirm/Select button (active-low)

OLED SPI:
- GPIO 18 -> OLED `D0` (SCLK)
- GPIO 23 -> OLED `D1` (MOSI)
- GPIO 16 -> OLED `RES`
- GPIO 17 -> OLED `DC`
- GPIO 27 -> OLED `CS`

Power:
- OLED VCC -> 3.3V
- OLED GND -> GND
- Buttons connect GPIO to GND (internal pull-ups in firmware)
- ESP32 GND and external 5V GND must be common

### Requirements

1. RGB LED wiring with suitable current-limit resistors.
2. Safe button wiring for active-low operation on GPIO 32/33.
3. Relay driver circuit for 5V relay coils from ESP32 GPIO (NPN + base resistor + flyback diode).
4. SPI OLED wiring and any level-shifting/power cautions.
5. Power distribution and decoupling recommendations.

### Please Provide

1. Step-by-step breadboard wiring instructions.
2. Schematics for:
   - RGB LED
   - buttons
   - relay driver channels
   - OLED SPI interface
   - power distribution
3. Component list with exact values/models.
4. Safety checks and common mistakes to avoid.

### Firmware Behavior Context

- Mode button cycles pending selections in IDLE.
- Confirm button confirms pending state.
- Confirm in IDLE (no pending) toggles OLED Wi-Fi info page.
- Pulse mode uses per-pump configurable durations.
- Continuous mode supports emergency stop/start and tracks brewing runtime.
