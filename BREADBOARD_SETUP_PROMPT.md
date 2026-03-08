# Bio-Bubbler Breadboard Setup Prompt for LLM

Use this prompt with ChatGPT, Claude, or another LLM to get detailed breadboard wiring instructions.

---

## Prompt

I'm building Bio-Bubbler, a machine control system with an ESP32 WROOM microcontroller. I need help setting up the hardware on a breadboard.

### System Overview:
- **Microcontroller:** ESP32 WROOM
- **Power Supply:** 5V (for relays and buttons)
- **Two Pumps:** Controlled via 5V relays (2 relays total)
- **RGB LED:** For status indication (Red, Green, Blue)
- **Two Push Buttons:** For user input (mode selection and confirmation)

### Pin Configuration:

**GPIO Outputs (from ESP32):**
- GPIO 2 → Red LED
- GPIO 4 → Green LED
- GPIO 5 → Blue LED
- GPIO 12 → Relay 1 (Pump 1)
- GPIO 13 → Relay 2 (Pump 2)

**GPIO Inputs (to ESP32):**
- GPIO 34 → Mode Button (input-only pin)
- GPIO 35 → Confirm Button (input-only pin)

**Power:**
- ESP32 GND and external 5V GND must be connected
- External 5V supply for relays and buttons
- ESP32 3.3V for logic

### Key Requirements:

1. **RGB LED Wiring:**
   - Common cathode or common anode? (I'm using common cathode with current-limiting resistors)
   - What resistor values for 3.3V GPIO pins driving LEDs?

2. **Button Wiring:**
   - Both buttons are active-low (pressing connects pin to GND)
   - GPIO 34 and 35 are input-only pins with internal pull-ups available
   - Should I use internal pull-ups on GPIO 34/35, or external pull-up resistors?
   - Do I need any debouncing capacitors on the breadboard?

3. **Relay Control:**
   - I need to drive two 5V relays from 3.3V GPIO pins
   - What transistor types would be suitable? (NPN BJT, MOSFET, ULN2003, etc.)
   - What are the typical current and voltage requirements for standard 5V relays?
   - Can you provide the full transistor circuit for relay drive (including base/gate resistors, freewheeling diodes)?

4. **Power Distribution:**
   - What's the safest way to power everything from a breadboard?
   - Should I use separate ground planes or is one common ground okay?
   - Any decoupling capacitor recommendations?

### Please Provide:

1. **Complete Breadboard Wiring Instructions**
   - Step-by-step connections from ESP32 to each component
   - Include resistor/transistor values
   - Component placement suggestions

2. **Circuit Diagrams**
   - Schematic for button inputs (with pull-ups)
   - Schematic for relay control (including transistor circuit)
   - RGB LED connection diagram
   - Power distribution overview

3. **Component List**
   - Exact components needed (transistor models, resistor values, etc.)
   - Where each component connects

4. **Safety Recommendations**
   - Any potential issues with GPIO 34/35 as inputs?
   - Proper relay handling and isolation?
   - Safe current limiting for LEDs?

---

## Notes:
- I'm using FreeRTOS on the ESP32
- Code expects active-low buttons (pressing = pin goes LOW)
- Code expects active-high relay control (HIGH = relay energized)
- Current breadboard setup is bare—no components yet, so starting from scratch
