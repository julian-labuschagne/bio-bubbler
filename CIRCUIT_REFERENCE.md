# Circuit Connection Reference

Quick visual reference for breadboard layout before detailed schematics.

## ESP32 WROOM Pin Connections

```
ESP32 WROOM Pinout (relevant pins):
┌─────────────────────────────────┐
│  GND  ├┤ GPIO 34 (Button Mode)  │
│  3V3  ├┤ GPIO 35 (Button Confirm)│
│       ├┤ GPIO 2  (LED Red)      │
│       ├┤ GPIO 4  (LED Green)    │
│       ├┤ GPIO 5  (LED Blue)     │
│  5V   ├┤ GPIO 12 (Relay 1)      │
│  GND  ├┤ GPIO 13 (Relay 2)      │
└─────────────────────────────────┘
```

## RGB LED Connection (Common Cathode)

```
               ┌─ R LED (3.3V, ~330Ω resistor) ─→ GPIO 2
               │
RGB LED Module ┼─ G LED (3.3V, ~330Ω resistor) ─→ GPIO 4
               │
               ├─ B LED (3.3V, ~330Ω resistor) ─→ GPIO 5
               │
               └─ Common Cathode ─→ GND
```

## Button Connections (Active-Low with Internal Pull-Up)

```
Button 1 (Mode):
    ┌──────────────────┐
    │   Push Button    │
    └────┬─────────┬───┘
         │         │
        GND      GPIO 34
                (pull-up enabled in code)

Button 2 (Confirm):
    ┌──────────────────┐
    │   Push Button    │
    └────┬─────────┬───┘
         │         │
        GND      GPIO 35
                (pull-up enabled in code)
```

## Relay Control with NPN Transistor (for each relay)

```
                    +5V ───────────────┐
                                       │
                                    ① │
                                    [Relay Coil]
                                    ② │
                                       ├─────────┐
                                    ① ↓         │
                    GPIO ──[1kΩ]──|   NPN      │
                                  |   (2N2222) │
                                  ├─────→ Pump/Load
                                  │         
                              [Diode ↑ 1N4007]
                                  │         
                                  └─ GND ────┘

Legend:
① = Relay Coil
[Diode] = Freewheeling diode (prevents voltage spikes)
NPN = Transistor (collector to relay, emitter to GND)
1kΩ = Base limiting resistor
```

## Power Distribution on Breadboard

```
ESP32 GND  ──┬─→ Breadboard GND Rail
             │
             ├─→ Button GND connections
             │
             ├─→ LED Common Cathode
             │
             └─→ Transistor Emitters (relay control)

5V Supply ─────→ Breadboard 5V Rail
             │
             ├─→ Relay Coils (power)
             │
             └─→ Status LED (through resistors to GPIO)

3.3V Supply ──→ Breadboard 3.3V Rail (if needed)
             │
             └─→ Level shifting (if required)
```

## Component Checklist

- [ ] 2× Push buttons (momentary, active-low when pressed)
- [ ] 1× Common cathode RGB LED or 3× individual LEDs
- [ ] 3× ~330Ω resistors (for RGB LED current limiting)
- [ ] 2× 5V relays (suitable for pump loads)
- [ ] 2× NPN transistors (2N2222, 2N3904, or similar)
- [ ] 2× ~1kΩ resistors (transistor base limiting)
- [ ] 2× 1N4007 diodes (freewheeling/flyback diodes)
- [ ] Breadboard with power rails
- [ ] 5V power supply (sufficient for relay coils + logic)
- [ ] Jumper wires (multiple colors recommended)
- [ ] Decoupling capacitor (~100µF or 0.1µF near ESP32)

## Next Steps

1. **Get detailed wiring instructions** from the prompt in `BREADBOARD_SETUP_PROMPT.md`
2. **Build on breadboard** following the schematic
3. **Test without relays first** (LEDs and buttons only)
4. **Add relay circuits** once basic inputs/outputs work
5. **Test relay operation** with test load (LED or motor)
6. **Connect actual pumps** once proven stable

## Important Safety Notes

- Always verify **GND connections** between ESP32 and 5V supply
- Freewheeling diodes protect transistors from relay voltage spikes
- Base resistors protect transistor from excessive base current
- Test with multimeter before connecting actual pump loads
- Keep relay coil voltage separate from logic circuits
