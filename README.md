# PIC18 Greenhouse Climate Controller FSM

Finite-state-machine climate controller for a greenhouse, built on **PIC18F45K22** (XC8, bare-metal).

---

## Overview
This project implements a 5-state FSM that regulates temperature and humidity inside a greenhouse:
- **Temperature sensing** – LM35 via internal ADC (AN0)
- **Humidity sensing** (optional) – SHT21 via I²C
- **Actuators** – PWM-driven fan (CCP2) and heater (CCP1)
- **User interface** – 2×16 LCD + 4×4 matrix keypad for setpoint adjustment
- **Telemetry** – UART reports (CSV-ready)

| State | Behaviour |
|-------|-----------|
| `IDLE` | Waiting for valid min/max temperature config |
| `MONITOR` | Normal operation – sensors in range |
| `HEATING` | Temperature < `temp_min` – proportional PWM on heater |
| `COOLING` | Temperature > `temp_max` – proportional PWM on fan |
| `ALARM` | Temperature > `temp_max+5` or < `temp_min-5` – max cooling + alert |

---

## Hardware
| Pin | Peripheral | Function |
|-----|------------|----------|
| RA0/AN0 | ADC | LM35 temperature sensor |
| RC1/CCP2 | PWM | Fan (ventilation) |
| RC2/CCP1 | PWM | Heater |
| RC3/RC4 | I²C | SHT21 humidity sensor (optional) |
| RB0–RB5 | GPIO | LCD 2×16 (4-bit mode) |
| RD0–RD7 | GPIO | 4×4 Matrix keypad |

---

## Repository Layout
```
pic18-greenhouse-fsm/
├── src/
│   └── main.c          # Complete FSM + drivers (ADC, PWM, I²C, UART, LCD, Keypad)
├── sim/                # Proteus simulation files (to be added)
├── docs/
│   └── EVALUACION FINAL.pdf  # Original project report (Spanish)
└── README.md
```

---

## Building & Simulating
1. Open `src/main.c` in **MPLAB X IDE** with **XC8** compiler.
2. Target device: **PIC18F45K22**.
3. Configure oscillator to **8 MHz INTIO**.
4. Build → `main.hex`.
5. Load into **Proteus** schematic (see `sim/`) for simulation.

---

## Key Formulas (from project report)
- **LM35 → °C**: `T = (ADC × 5.0 / 1024) / 0.01`
- **SHT21 RH%**: `RH = -6 + 125 × (raw / 65536)`
- **PWM duty (10-bit)**: `CCPRxL = duty >> 2; CCPxCONbits.DCxB = duty & 0x03`

---

## UART Telemetry Format
```
T:23.5,H:45.2,S:1,Min:18.0,Max:28.0
```
- `T` – Temperature (°C)
- `H` – Humidity (%)
- `S` – State (0=IDLE, 1=MONITOR, 2=HEATING, 3=COOLING, 4=ALARM)
- `Min`/`Max` – Configured thresholds

---

## Extracted Project Documentation
The original Spanish report ([`EVALUACION FINAL.pdf`](docs/EVALUACION%20FINAL.pdf)) describes:
- 5-state FSM with hysteresis (±5 °C alarm band)
- Proportional PWM control for smooth actuation
- Keypad-driven setpoint adjustment (keys A=min, B=max, C=confirm, D=cancel)
- 10 ms Timer0 tick for non-blocking delays
- Optional SHT21 humidity sensor with I²C polling

---

## License
MIT – feel free to use for learning or as a base for your own greenhouse controller.