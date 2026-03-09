# Bio-Bubbler Circuit Connection Reference

Quick wiring reference for the current firmware.

## ESP32 Connection Summary

```
LED:
  GPIO 2  -> Red
  GPIO 4  -> Green
  GPIO 5  -> Blue

Pumps (relay control):
  GPIO 12 -> Pump 1 relay input
  GPIO 13 -> Pump 2 relay input

Buttons (active-low to GND):
  GPIO 32 -> Mode
  GPIO 33 -> Confirm/Select

OLED (SPI, 0.96 inch):
  GPIO 18 -> D0 / SCLK
  GPIO 23 -> D1 / MOSI
  GPIO 16 -> RES
  GPIO 17 -> DC
  GPIO 27 -> CS
  3V3     -> VCC
  GND     -> GND
```

## Buttons

- Wire each button between GPIO and GND.
- Internal pull-ups are enabled in firmware.
- Do not feed 5V directly into GPIO button lines.

## Relay Driver Reminder

For each relay channel:
- GPIO -> 1k resistor -> NPN base
- NPN emitter -> GND
- NPN collector -> relay coil low side
- Relay coil high side -> +5V
- Flyback diode across relay coil

## OLED Reminder

- OLED is 4-wire SPI plus control lines (`CS`, `DC`, `RES`).
- No MISO needed.
- Use 3.3V for OLED VCC unless your specific module explicitly supports 5V logic/power.

## Power and Ground

- Share ground between ESP32 and external 5V relay supply.
- Keep relay power wiring separated from sensitive signal wiring where possible.
- Add local decoupling near ESP32 and OLED.
