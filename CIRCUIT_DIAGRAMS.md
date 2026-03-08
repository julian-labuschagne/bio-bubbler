# Bio-Bubbler Circuit Diagrams (Visual Rendering Ready)

This file contains circuit diagrams that can be rendered visually.

## System Block Diagram

```mermaid
graph TD
    PSU["5V Power Supply"]
    ESP["ESP32 WROOM"]
    
    subgraph "Output Circuits"
        LED["RGB LED Circuit<br/>3x GPIO + Resistors"]
        REL1["Relay 1 Circuit<br/>GPIO12 → Transistor → 5V Relay"]
        REL2["Relay 2 Circuit<br/>GPIO13 → Transistor → 5V Relay"]
    end
    
    subgraph "Input Circuits"
        BTN1["Mode Button<br/>GPIO34 Active-Low"]
        BTN2["Confirm Button<br/>GPIO35 Active-Low"]
    end
    
    subgraph "Load"
        PUMP1["Pump 1"]
        PUMP2["Pump 2"]
    end
    
    PSU -->|5V| REL1
    PSU -->|5V| REL2
    PSU -->|GND| ESP
    
    ESP -->|GPIO2/4/5| LED
    ESP -->|GPIO12| REL1
    ESP -->|GPIO13| REL2
    ESP -->|GPIO34| BTN1
    ESP -->|GPIO35| BTN2
    
    REL1 -->|Relay Contact| PUMP1
    REL2 -->|Relay Contact| PUMP2
    
    BTN1 -->|Button Press| ESP
    BTN2 -->|Button Press| ESP
    
    style PSU fill:#d4edda
    style ESP fill:#cfe2ff
    style LED fill:#fff3cd
    style REL1 fill:#f8d7da
    style REL2 fill:#f8d7da
    style BTN1 fill:#e7d4f5
    style BTN2 fill:#e7d4f5
    style PUMP1 fill:#d1e7dd
    style PUMP2 fill:#d1e7dd
```

## Relay Control Circuit (Detailed)

```mermaid
graph LR
    GPIO["GPIO 12/13<br/>3.3V Output"]
    RES["1kΩ Resistor<br/>Base Limiting"]
    BASE["Transistor Base"]
    COLL["Transistor Collector"]
    EMIT["Transistor Emitter"]
    
    RELAY_P["Relay Coil<br/>+5V"]
    RELAY_N["Relay Coil<br/>Ground"]
    DIODE_A["Diode<br/>Anode"]
    DIODE_C["Diode<br/>Cathode"]
    PUMP_COIL["Pump Relay<br/>Coil"]
    
    GND1["GND"]
    VCC["5V"]
    GND2["GND"]
    
    GPIO --> RES --> BASE
    VCC --> PUMP_COIL
    PUMP_COIL --> RELAY_N
    RELAY_N --> DIODE_A
    DIODE_A --> DIODE_C
    DIODE_C --> GND2
    
    COLL --> PUMP_COIL
    BASE -.->|NPN Transistor| COLL
    EMIT -.->|NPN Transistor| EMIT
    EMIT --> GND1
    
    style GPIO fill:#cfe2ff
    style RES fill:#fff3cd
    style BASE fill:#e7d4f5
    style COLL fill:#f8d7da
    style EMIT fill:#f8d7da
    style PUMP_COIL fill:#d1e7dd
    style DIODE_A fill:#f8d7da
    style DIODE_C fill:#f8d7da
    style VCC fill:#d4edda
    style GND1 fill:#e2e3e5
    style GND2 fill:#e2e3e5
```

## Button Input Circuit

```mermaid
graph LR
    VCC["3.3V<br/>Internal Pull-Up"]
    RES["10kΩ<br/>Software"]
    GPIO["GPIO 34/35<br/>Input"]
    SWITCH["Push Button"]
    GND["GND"]
    
    VCC --> RES --> GPIO
    GPIO --> SWITCH --> GND
    
    style VCC fill:#d4edda
    style RES fill:#fff3cd
    style GPIO fill:#cfe2ff
    style SWITCH fill:#e7d4f5
    style GND fill:#e2e3e5
```

## RGB LED Circuit

```mermaid
graph LR
    GPIO_R["GPIO 2<br/>Red"]
    GPIO_G["GPIO 4<br/>Green"]
    GPIO_B["GPIO 5<br/>Blue"]
    
    RES_R["330Ω"]
    RES_G["330Ω"]
    RES_B["330Ω"]
    
    LED_R["Red LED"]
    LED_G["Green LED"]
    LED_B["Blue LED"]
    
    GND["GND<br/>Common Cathode"]
    
    GPIO_R --> RES_R --> LED_R --> GND
    GPIO_G --> RES_G --> LED_G --> GND
    GPIO_B --> RES_B --> LED_B --> GND
    
    style GPIO_R fill:#cfe2ff
    style GPIO_G fill:#cfe2ff
    style GPIO_B fill:#cfe2ff
    style RES_R fill:#fff3cd
    style RES_G fill:#fff3cd
    style RES_B fill:#fff3cd
    style LED_R fill:#f8d7da
    style LED_G fill:#d4edda
    style LED_B fill:#d1e7dd
    style GND fill:#e2e3e5
```

## State Machine (Software Logic)

```mermaid
stateDiagram-v2
    [*] --> IDLE
    
    IDLE -->|Mode Button| PULSE_PENDING: LED Flashing RED
    IDLE -->|Confirm| IDLE: No action
    
    PULSE_PENDING -->|Mode Button| CONTINUOUS_PENDING: LED Flashing BLUE
    PULSE_PENDING -->|Confirm| PULSE: LED Solid RED
    
    CONTINUOUS_PENDING -->|Mode Button| IDLE: Back to safe
    CONTINUOUS_PENDING -->|Confirm| CONTINUOUS: LED Solid BLUE
    
    PULSE -->|Confirm (30s)| PULSE_RUNNING: Pumps ON
    PULSE -->|Mode Button| PULSE_PENDING: Change mind
    
    PULSE_RUNNING -->|Timer (30s)| PULSE: Pumps OFF
    
    CONTINUOUS -->|Confirm Toggle| CONTINUOUS_STOPPED: Emergency STOP
    CONTINUOUS -->|Mode Button| CONTINUOUS_PENDING: Change mode
    CONTINUOUS_STOPPED -->|Confirm Toggle| CONTINUOUS: Emergency START
    
    style IDLE fill:#d4edda
    style PULSE_PENDING fill:#fff3cd
    style CONTINUOUS_PENDING fill:#fff3cd
    style PULSE fill:#f8d7da
    style PULSE_RUNNING fill:#f8d7da
    style CONTINUOUS fill:#cfe2ff
    style CONTINUOUS_STOPPED fill:#f8d7da
```

## Breadboard Power Distribution

```mermaid
graph TD
    PSU["5V DC Supply"]
    GND["Ground Reference"]
    
    PSU -->|+5V| BUS_5V["Breadboard +5V Rail"]
    GND -->|GND| BUS_GND["Breadboard GND Rail"]
    
    BUS_5V -->|Relay Coils| RELAY["Relay 1 & 2 Circuits"]
    BUS_GND --> RELAY
    
    ESP["ESP32"]
    ESP -->|GND| BUS_GND
    ESP -->|3.3V| DECAP["0.1µF Decap"]
    DECAP --> BUS_GND
    
    BUS_GND -->|Logic Ground| BUTTONS["Button Circuits"]
    BUS_GND -->|LED Ground| LEDS["LED Circuits"]
    
    style PSU fill:#d4edda
    style GND fill:#e2e3e5
    style BUS_5V fill:#fff3cd
    style BUS_GND fill:#e2e3e5
    style ESP fill:#cfe2ff
    style RELAY fill:#f8d7da
    style BUTTONS fill:#e7d4f5
    style LEDS fill:#fff3cd
    style DECAP fill:#d1e7dd
```

## Pin Assignment Summary

```mermaid
graph LR
    ESP["ESP32 WROOM"]
    
    ESP -->|GPIO 2| LED_R["🔴 Red LED<br/>+330Ω"]
    ESP -->|GPIO 4| LED_G["🟢 Green LED<br/>+330Ω"]
    ESP -->|GPIO 5| LED_B["🔵 Blue LED<br/>+330Ω"]
    
    ESP -->|GPIO 12| REL1["Relay 1 Control<br/>→ NPN → Pump 1"]
    ESP -->|GPIO 13| REL2["Relay 2 Control<br/>→ NPN → Pump 2"]
    
    ESP -->|GPIO 34| BTN1["MODE Button<br/>Pull-Up"]
    ESP -->|GPIO 35| BTN2["CONFIRM Button<br/>Pull-Up"]
    
    ESP -->|GND| GND["Ground Rail"]
    
    style ESP fill:#cfe2ff
    style LED_R fill:#ffcccc
    style LED_G fill:#ccffcc
    style LED_B fill:#ccccff
    style REL1 fill:#ffdddd
    style REL2 fill:#ffdddd
    style BTN1 fill:#eeddff
    style BTN2 fill:#eeddff
    style GND fill:#dddddd
```

---

## Component Values at a Glance

| Component | Value | Purpose |
|-----------|-------|---------|
| LED Resistor | 330Ω | Current limiting for RGB LEDs (3.3V GPIO) |
| Transistor Base Resistor | 1kΩ | Base current limiting (3.3V GPIO → transistor) |
| Button Pull-Up | Internal | Software pull-up on GPIO 34/35 (no external needed) |
| Debounce Capacitor | 0.1µF (optional) | Hardware debounce on buttons |
| Decoupling Capacitor | 0.1µF | Power supply stability near ESP32 |
| Relay Coil Voltage | 5V DC | Standard relay operating voltage |
| Freewheeling Diode | 1N4007 | Relay transient protection |
| Transistor Type | NPN BJT (2N2222/BC547) | Relay driver (500mA capable) |

---

## Safety Margins

✓ **GPIO Current:** Each GPIO pin rated 40mA max
  - RGB LED at 330Ω: ~4mA each, well within limits
  - Transistor base at 1kΩ: ~3.3mA, within limits

✓ **Transistor Rating:** BC547/2N2222 rated 500mA collector current
  - Typical relay coil: 50-100mA, very safe margin

✓ **Button Inputs:** GPIO 34/35 with internal pull-ups
  - Safe for momentary button presses
  - No external pull-up needed

✓ **Diode Rating:** 1N4007 rated 1A
  - Relay coil collapse current: <500mA, very safe margin

✓ **Power Distribution:** Shared GND between ESP32 and 5V supply
  - Prevents noise and ensures proper logic levels

