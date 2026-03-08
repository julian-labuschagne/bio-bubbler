# Detailed Circuit Schematics

Complete ASCII circuit diagrams for breadboard assembly.

---

## 1. RGB LED Circuit (Common Cathode)

Each LED requires a current-limiting resistor. Using 3.3V GPIO pins with ~330Ω resistors.

```
ESP32 GPIO 2 (Red LED)
    │
    └──[330Ω]──┬─→ (+) Red LED
               │
               ├─────────────────────┐
               │                     │
ESP32 GPIO 4 (Green LED)             │
    │                                │
    └──[330Ω]──────┬─→ (+) Green LED │
                   │                 │
                   │                 │ Common
ESP32 GPIO 5 (Blue LED)              │ Cathode
    │                                │ Connection
    └──[330Ω]──────┬─→ (+) Blue LED  │
                   │                 │
                   └─────────────────┤
                                     │
                                    GND
```

### LED Current Calculation:
```
Resistor: 330Ω
LED Forward Voltage: ~2V (typical for RGB LEDs)
GPIO Voltage: 3.3V

Current = (3.3V - 2V) / 330Ω = 1.3V / 330Ω ≈ 3.9mA
This is safe (GPIO pins can source 40mA max, but 10-15mA is typical)
```

---

## 2. Button Input Circuits (Active-Low with Internal Pull-Up)

### Button 1 (Mode Button) - GPIO 34

```
        3.3V (Internal Pull-Up via ESP32 Firmware)
            │
            [10kΩ] ← Software pull-up enabled
            │
        ┌───┤───────────────────→ GPIO 34
        │   │
        │  ○○  Push Button
        │   │  (momentary switch)
        │   │
        └───┴──────────→ GND


When pressed:   GPIO 34 = 0V (LOW)
When released:  GPIO 34 = 3.3V (HIGH via pull-up)
```

### Button 2 (Confirm Button) - GPIO 35

```
        3.3V (Internal Pull-Up via ESP32 Firmware)
            │
            [10kΩ] ← Software pull-up enabled
            │
        ┌───┤───────────────────→ GPIO 35
        │   │
        │  ○○  Push Button
        │   │  (momentary switch)
        │   │
        └───┴──────────→ GND


When pressed:   GPIO 35 = 0V (LOW)
When released:  GPIO 35 = 3.3V (HIGH via pull-up)
```

### Debouncing (Optional Hardware)
```
If you want additional debouncing capacitor on breadboard:

        3.3V
            │
            [10kΩ]
            │
    ┌───────┤──────────→ GPIO
    │       │
    │      ○○ Button
    │       │
    │   ┌───┴───┐
    │   [0.1µF]
    │   │       │
    └───┴───────┴──→ GND

Capacitor value: 0.1µF (ceramic or film)
This creates RC low-pass filter: τ = RC = 10k × 0.1µ = 1ms
```

---

## 3. Relay Control Circuit with NPN Transistor

### Single Relay Control (Repeat for each relay)

```
+5V Supply
    │
    │
    ├────────────────1───┐
    │                    │
    │              [Relay Coil]
    │                    │
    │                    2└─────────────┬──→ To Pump/Load
    │                                   │
    │                                   │
    │                           [1N4007 Diode]↑
    │                                   │
    │                      Freewheeling Diode
    │                      (prevents voltage spike)
    │                                   │
    │                                   │
ESP32 GPIO 12/13                        │
    (3.3V output)                       │
    │                                   │
    └──[1kΩ]──→ Base (b)            │
                │                   │
            ┌───┤ NPN Transistor ├──┴──→ GND
            │   │ (2N2222)         │
            │   │ (BC547)          │
            │   │ (2N3904)         │
            │   │                  │
Collector→─┤   Emitter→───────────┘
(c)         │   (e)
            │
```

### Detailed Breadboard Layout for Single Relay

```
Breadboard View (Side Profile):

5V ────┬─────────[Relay Coil]─────┬────────────────┐
       │                          │                │
       │                      [D4] │ Diode anode    │
       │             (anode ──┘)  │ cathode ──┐    │
       │                          │           │    │
       │                          └───────────┼────┤
       │                                      │    │
       │        Collector ┌─→ To Pump Coil    │    │
       │                  │                   │    │
GPIO ──[1kΩ]──→ Base      │                   │    │
(3.3V)  ┌──────┤          │                   │    │
        └──────┤ 2N2222   │    [Pump               │
               │ NPN      │     Load]             │
               ├──→ Emitter                       │
               │                                  │
GND ───────────┴──────────────────────────────────┤
                                                  │
Load Return ────────────────────────────────────────┘
(Pump Ground)


Transistor Pinout (2N2222):
    Emitter (e) → GND
    Base (b)    → From GPIO (through 1kΩ resistor)
    Collector (c) → Relay Coil
```

---

## 4. Complete System Schematic

```
                    ┌─────────────────────────────────────┐
                    │     External Power Supply           │
                    │ (5V for relays, USB or external)    │
                    └──────────┬──────────────────────────┘
                               │
                    ┌──────────┴──────────┐
                    │                     │
                  +5V                    GND
                    │                     │
                    ├─ Relay 1 Circuit    │
                    │  (Pump 1)           │
                    │                     │
                    ├─ Relay 2 Circuit    │
                    │  (Pump 2)           │
                    │                     │
                    │                     │
    ┌───────────────┬─────────────────────┴──────────────┐
    │               │                                    │
    │           ┌───────────────────────────┐           │
    │           │    ESP32 WROOM DevKit     │           │
    │           │                           │           │
    │           │ 3V3 ───┐                  │           │
    │           │        │                  │           │
    │           │ GND ───┼──────────────────┼──→ GND    │
    │           │        │                  │           │
    │  ┌────────┼─ GPIO 2 (Red LED)        │           │
    │  │        │        │                  │           │
    │  │ ┌──────┼─ GPIO 4 (Green LED)      │           │
    │  │ │      │        │                  │           │
    │  │ └──────┼─ GPIO 5 (Blue LED)       │           │
    │  │        │        │                  │           │
    │  │ ┌──────┼─ GPIO 12 (Relay 1)       │           │
    │  │ │      │        │                  │           │
    │  │ │ ┌────┼─ GPIO 13 (Relay 2)       │           │
    │  │ │ │    │        │                  │           │
    │  │ │ │ ┌──┼─ GPIO 34 (Button Mode)   │           │
    │  │ │ │ │  │        │                  │           │
    │  │ │ │ │ ┌┼─ GPIO 35 (Button Confirm)│           │
    │  │ │ │ │ ││        │                  │           │
    │  │ │ │ │ ││ GND ───┼──────────────────┼──→ GND    │
    │  │ │ │ │ └┴────────┘                  │           │
    │  │ │ │ └───────────────────────────────┘           │
    │  │ │ │                                             │
    │  │ │ │  RGB LED Circuit                            │
    │  │ └──[330Ω]─→ Green LED ──→ GND                  │
    │  │     GND                                          │
    │  │                                                  │
    │  └──[330Ω]─→ Red LED ────→ GND                    │
    │              GND                                    │
    │                                                     │
    └────[330Ω]─→ Blue LED ────→ GND                    │
                  GND                                     │

Legend:
─────  Jumper wire
[Ω]    Resistor (specify value)
→      Connection direction
GND    Common ground (must be shared!)
3V3    3.3V logic supply
```

---

## 5. Breadboard Component Placement Guide

### Row Layout Example (20-row breadboard)

```
┌─────────────────────────────────────────┐
│         + (5V)        - (GND)            │
├─────────┬──────────────────────┬─────────┤
│ Relay 1 │ Relay 2              │ LEDs    │
│ Circuit │ Circuit              │ Circuit │
├─────────┼──────────────────────┼─────────┤
│ Row 1   │ [1N4007-D1]          │         │
│ Row 2   │ [2N2222-Q1]          │ [R-LED] │
│ Row 3   │ [1kΩ resistor]       │ [330Ω]  │
│ Row 4   │ ├─ Relay Coil (+)    │ [330Ω]  │
│ Row 5   │ └─ Relay Coil (-)    │ [330Ω]  │
│ Row 6   │ ├─ Pump Load         │ [GND]   │
│ Row 7   │ └─ Pump Ground       │         │
│ Row 8   │                      │         │
│ Row 9   │                      │ Button  │
│ Row 10  │ Power/Ground Bus     │ Circuit │
│ Row 11  │ +5V ───────────────  │         │
│ Row 12  │ GND ───────────────  │ [Mode]  │
│ Row 13  │                      │ [Conf]  │
│ Row 14  │ ESP32 Connections    │         │
│ Row 15  │ GPIO 12 ─────────→   │         │
│ Row 16  │ GPIO 13 ─────────→   │ ├─[GND] │
│ Row 17  │ GPIO 2 ──────────→   │ └─[GND] │
│ Row 18  │ GPIO 4 ──────────→   │         │
│ Row 19  │ GPIO 5 ──────────→   │         │
│ Row 20  │ GND ───────────────  │         │
└─────────┴──────────────────────┴─────────┘

Actual order depends on your specific breadboard.
Key rule: Group by function (power, relays, LEDs, buttons)
```

---

## 6. Connection Checklist

Use this to verify your breadboard is wired correctly:

### Power Distribution
- [ ] 5V supply positive → Breadboard +5V rail
- [ ] 5V supply ground → Breadboard GND rail (also connected to ESP32 GND)
- [ ] ESP32 GND → Breadboard GND rail (shared ground is critical!)
- [ ] Decoupling capacitor (0.1µF) between 3.3V and GND near ESP32

### RGB LED (Red = GPIO 2, Green = GPIO 4, Blue = GPIO 5)
- [ ] Red LED (+) → [330Ω] → GPIO 2
- [ ] Green LED (+) → [330Ω] → GPIO 4
- [ ] Blue LED (+) → [330Ω] → GPIO 5
- [ ] All LED (-) → GND (common cathode)
- [ ] LEDs light up when GPIO goes HIGH

### Relay 1 Circuit (GPIO 12, Pump 1)
- [ ] GPIO 12 → [1kΩ] → Q1 (2N2222) Base
- [ ] Q1 Collector → Relay 1 Coil (+)
- [ ] Relay 1 Coil (-) → 1N4007 Diode Anode
- [ ] 1N4007 Diode Cathode → GND
- [ ] Q1 Emitter → GND
- [ ] Relay 1 NO (Normally Open) → Pump 1
- [ ] Relay 1 COM (Common) → 5V or Load Supply

### Relay 2 Circuit (GPIO 13, Pump 2)
- [ ] GPIO 13 → [1kΩ] → Q2 (2N2222) Base
- [ ] Q2 Collector → Relay 2 Coil (+)
- [ ] Relay 2 Coil (-) → 1N4007 Diode Anode
- [ ] 1N4007 Diode Cathode → GND
- [ ] Q2 Emitter → GND
- [ ] Relay 2 NO (Normally Open) → Pump 2
- [ ] Relay 2 COM (Common) → 5V or Load Supply

### Button 1 - Mode (GPIO 34)
- [ ] Button one terminal → GPIO 34
- [ ] Button other terminal → GND
- [ ] (Breadboard pull-up NOT needed; ESP32 has internal pull-up)

### Button 2 - Confirm (GPIO 35)
- [ ] Button one terminal → GPIO 35
- [ ] Button other terminal → GND
- [ ] (Breadboard pull-up NOT needed; ESP32 has internal pull-up)

---

## 7. Testing Procedure

### Stage 1: Power and LEDs Only
1. Connect ESP32 and LEDs (no relays yet)
2. Verify each LED lights individually
3. Test RGB color mixing

### Stage 2: Add Buttons (No Relays)
1. Add buttons to GPIO 34 and 35
2. Use serial monitor to verify button presses
3. Watch LED state changes

### Stage 3: Add Relay Circuits
1. Add one relay circuit at a time
2. Test with a test load (LED or motor)
3. Verify relay clicks when GPIO goes HIGH

### Stage 4: Full System Test
1. Add second relay
2. Test complete state machine
3. Verify pump relay operation
4. Connect actual pump load once proven stable

---

## 8. Common Mistakes to Avoid

❌ **Forgetting shared GND between ESP32 and 5V supply**
- Always connect ESP32 GND to breadboard GND rail with 5V supply

❌ **Wrong transistor polarity**
- Collector (c) goes to relay coil
- Emitter (e) goes to GND
- Base (b) receives signal from GPIO

❌ **Missing freewheeling diode on relay**
- Diode MUST be present to protect transistor from voltage spikes
- Anode (striped end) toward relay coil
- Cathode toward GND

❌ **No base resistor on transistor**
- Always use 1kΩ resistor between GPIO and transistor base
- Prevents excessive base current

❌ **LED resistor too high**
- 330Ω is standard for 3.3V GPIO and 2V LED
- Adjust if LED appears dim (try 220Ω or 270Ω)

❌ **Reversed button connections**
- One terminal to GPIO, other to GND
- No external pull-up needed (use internal pull-up)

---

## Component Specifications

### Transistor (NPN BJT)
- Type: NPN BJT
- Models: 2N2222, BC547, 2N3904
- Collector Current (Ic): 500mA typical (relays usually need 50-100mA)
- Minimum DC Gain (hFE): 100-200
- Suitable for 5V relay coils driven from 3.3V logic

### Diode (Freewheeling)
- Type: 1N4007
- Max Current: 1A (typical relay coil draws 50-100mA)
- Reverse Recovery Time: Fast enough for relay switching
- Replace diode if it melts or burns

### Resistors
- LED series: 330Ω, 1/4W, 5% tolerance
- Transistor base: 1kΩ, 1/4W, 5% tolerance
- All standard through-hole types

### Push Buttons
- Type: Momentary push button (normally open)
- Contact Rating: At least 5V at 50mA
- Travel: 2-5mm typical
- Operating force: 50-100g

### Relays
- Voltage: 5V DC coil
- Contact Type: SPDT (Single Pole Double Throw) minimum
- Coil Current: 50-100mA typical
- Contact Rating: 10A @ 125/250V AC suitable for pump motors

