# Bio-Bubbler Detailed Circuit Schematics

ASCII schematics for current hardware mapping.

---

## 1) RGB LED (Common Cathode)

```
GPIO0  --[330R]--> RED anode   (R1)
GPIO1  --[330R]--> GREEN anode (R2)
GPIO2  --[330R]--> BLUE anode  (R3)
RGB cathode ----------------------> GND
```

---

## 2) Buttons (Active-Low)

```
Mode button (SW1 / BTN_RED):
GPIO3  ----o/ o---- GND

Confirm/Select button (SW2 / BTN_GREEN):
GPIO10 ----o/ o---- GND
```

Notes:
- Internal pull-ups are enabled in firmware.
- No external pull-up is required for the default setup.

---

## 3) Relay Driver (repeat per pump)

```
ESP32 GPIOx --[1k]--> B  NPN transistor
                        C ---- relay coil ---- +5V
                        E -------------------- GND

Flyback diode across relay coil:
  cathode to +5V side, anode to transistor/coil low side
```

Recommended parts:
- NPN: 2N2222 / 2N3904 / BC547
- Diode: 1N4007
- Base resistor: 1k

---

## 4) OLED SPI (0.96 inch)

```
ESP32-C3 GPIO5  ----> OLED D0 (SCLK)
ESP32-C3 GPIO4  ----> OLED D1 (MOSI)
ESP32-C3 GPIO8  ----> OLED RES
ESP32-C3 GPIO7  ----> OLED DC
ESP32-C3 GPIO6  ----> OLED CS
ESP32-C3 3V3    ----> OLED VCC
ESP32-C3 GND    ----> OLED GND
```

---

## 5) Full Pin Checklist

- [ ] GPIO 0 wired to LED Red resistor (R1)
- [ ] GPIO 1 wired to LED Green resistor (R2)
- [ ] GPIO 2 wired to LED Blue resistor (R3)
- [ ] GPIO 20 wired to Pump 1 relay driver (IN1)
- [ ] GPIO 21 wired to Pump 2 relay driver (IN2)
- [ ] GPIO 3 wired to Mode button / BTN_RED (to GND)
- [ ] GPIO 10 wired to Confirm button / BTN_GREEN (to GND)
- [ ] GPIO 5/4/8/7/6 wired to OLED D0/D1/RES/DC/CS
- [ ] OLED VCC to 3V3 and OLED GND to GND
- [ ] ESP32 GND tied to external 5V relay supply GND

---

## 6) Safety Notes

- Keep relay load wiring isolated from logic traces.
- Verify transistor pinout for your exact package before powering.
- Always keep grounds common between ESP32 and relay supply.
- If OLED stays blank, verify SPI pin order (`D0` clock, `D1` data), and RES/DC/CS wiring.
