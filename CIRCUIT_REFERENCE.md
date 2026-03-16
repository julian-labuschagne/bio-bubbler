# Bio-Bubbler Circuit Connection Reference

Quick wiring reference for the current firmware.

## ESP32-C3 Super Mini Connection Summary

```
LED:
  GPIO 0  -> Red   (R1)
  GPIO 1  -> Green  (R2)
  GPIO 2  -> Blue   (R3)

Pumps (relay control):
  GPIO 20 -> Pump 1 relay input (IN1)
  GPIO 21 -> Pump 2 relay input (IN2)

Buttons (active-low to GND):
  GPIO 3  -> Mode    (SW1 / BTN_RED)
  GPIO 10 -> Confirm/Select (SW2 / BTN_GREEN)

OLED (SPI, 0.96 inch):
  GPIO 5  -> D0 / SCLK
  GPIO 4  -> D1 / MOSI
  GPIO 8  -> RES
  GPIO 7  -> DC
  GPIO 6  -> CS
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
