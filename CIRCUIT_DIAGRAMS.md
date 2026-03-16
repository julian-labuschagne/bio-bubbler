# Bio-Bubbler Circuit Diagrams (Visual Rendering Ready)

This file contains Mermaid diagrams aligned with the current firmware.

## System Block Diagram

```mermaid
graph TD
    PSU["5V Relay Supply"]
    ESP["ESP32-C3 Super Mini"]
    OLED["0.96in SPI OLED"]

    subgraph Outputs
        LED["RGB LED\nGPIO 0/1/2"]
        REL1["Pump 1 Relay\nGPIO 20"]
        REL2["Pump 2 Relay\nGPIO 21"]
    end

    subgraph Inputs
        BTN_MODE["Mode Button\nGPIO 3 active-low"]
        BTN_CONFIRM["Confirm/Select Button\nGPIO 10 active-low"]
    end

    PSU --> REL1
    PSU --> REL2
    PSU -->|Shared GND| ESP

    ESP --> LED
    ESP --> REL1
    ESP --> REL2
    ESP --> BTN_MODE
    ESP --> BTN_CONFIRM

    ESP -->|SCLK GPIO5| OLED
    ESP -->|MOSI GPIO4| OLED
    ESP -->|RES GPIO8| OLED
    ESP -->|DC GPIO7| OLED
    ESP -->|CS GPIO6| OLED
    ESP -->|3V3/GND| OLED
```

## OLED SPI Wiring

```mermaid
graph LR
    ESP["ESP32-C3"]
    OLED["OLED"]

    ESP -->|GPIO5| D0["D0/SCLK"]
    ESP -->|GPIO4| D1["D1/MOSI"]
    ESP -->|GPIO8| RES["RES"]
    ESP -->|GPIO7| DC["DC"]
    ESP -->|GPIO6| CS["CS"]
    ESP -->|3V3| VCC["VCC"]
    ESP -->|GND| GND["GND"]

    D0 --> OLED
    D1 --> OLED
    RES --> OLED
    DC --> OLED
    CS --> OLED
    VCC --> OLED
    GND --> OLED
```

## State Machine (Current Behavior)

```mermaid
stateDiagram-v2
    [*] --> IDLE

    IDLE --> PENDING_PULSE: Mode
    PENDING_PULSE --> PENDING_CONTINUOUS: Mode
    PENDING_CONTINUOUS --> IDLE: Mode

    PENDING_PULSE --> PULSE: Confirm
    PENDING_CONTINUOUS --> CONTINUOUS: Confirm

    PULSE --> PULSE: Confirm starts pulse cycle
    PULSE --> PENDING_CONTINUOUS: Mode

    CONTINUOUS --> CONTINUOUS: Confirm toggle stop/start
    CONTINUOUS --> IDLE: Mode

    note right of IDLE
      Confirm in IDLE (with no pending mode)
      toggles OLED Wi-Fi info page
    end note
```

## OLED UI Mapping

```mermaid
graph TD
    IDLE["IDLE"] --> OLED_IDLE["Idle"]
    IDLE --> OLED_WIFI["SSID/PASS/IP page\n(toggle with Confirm)"]

    PENDING_PULSE["Pending Pulse"] --> OLED_PULSE_PENDING["Tap (flashing)"]
    PENDING_CONT["Pending Continuous"] --> OLED_CONT_PENDING["Brew (flashing)"]

    PULSE["Pulse"] --> OLED_TAP["Tap"]
    PULSE --> OLED_POURING["Pouring during active pulse"]

    CONT["Continuous"] --> OLED_BREW["Brewing"]
    CONT --> OLED_TIMER["Timer: DDdHHhMMm or HHhMMm"]
```

## Component Values

| Item | Value/Type |
|---|---|
| LED resistors | 330 ohm |
| Relay base resistors | 1k ohm |
| Relay transistor | NPN (2N2222 / 2N3904 / BC547) |
| Relay flyback diode | 1N4007 |
| Button wiring | GPIO to GND, internal pull-up |
| OLED interface | SPI (D0, D1, RES, DC, CS) |
