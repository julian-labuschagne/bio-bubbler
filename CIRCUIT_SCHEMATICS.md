# Bio-Bubbler Detailed Circuit Schematics

ASCII schematics for current hardware mapping.

---

## 1) RGB LED (Common Cathode)

```
GPIO2  --[330R]--> RED anode
GPIO4  --[330R]--> GREEN anode
GPIO5  --[330R]--> BLUE anode
RGB cathode ----------------------> GND
```

---

## 2) Buttons (Active-Low)

```
Mode button:
GPIO32 ----o/ o---- GND

Confirm/Select button:
GPIO33 ----o/ o---- GND
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
ESP32 GPIO18  ----> OLED D0 (SCLK)
ESP32 GPIO23  ----> OLED D1 (MOSI)
ESP32 GPIO16  ----> OLED RES
ESP32 GPIO17  ----> OLED DC
ESP32 GPIO27  ----> OLED CS
ESP32 3V3     ----> OLED VCC
ESP32 GND     ----> OLED GND
```

---

## 5) Full Pin Checklist

- [ ] GPIO 2 wired to LED Red resistor
- [ ] GPIO 4 wired to LED Green resistor
- [ ] GPIO 5 wired to LED Blue resistor
- [ ] GPIO 12 wired to Pump 1 relay driver
- [ ] GPIO 13 wired to Pump 2 relay driver
- [ ] GPIO 32 wired to Mode button (to GND)
- [ ] GPIO 33 wired to Confirm button (to GND)
- [ ] GPIO 18/23/16/17/27 wired to OLED D0/D1/RES/DC/CS
- [ ] OLED VCC to 3V3 and OLED GND to GND
- [ ] ESP32 GND tied to external 5V relay supply GND

---

## 6) Safety Notes

- Keep relay load wiring isolated from logic traces.
- Verify transistor pinout for your exact package before powering.
- Always keep grounds common between ESP32 and relay supply.
- If OLED stays blank, verify SPI pin order (`D0` clock, `D1` data), and RES/DC/CS wiring.
