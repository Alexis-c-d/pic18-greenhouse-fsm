# PIC18 Greenhouse Climate Controller

Finite-state-machine climate controller for a greenhouse, built on a **PIC18F45K22**.

## Features
- Temperature sensing with LM35 (ADC)
- Humidity sensing with SHT21 over I²C
- Dual PWM outputs for fan and heater
- UART interface
- 4×4 keypad input
- LCD status display

## Toolchain
MPLAB X IDE · XC8 · Proteus (simulation)

## Repository layout
```
src/    C source
sim/    Proteus project
docs/   Schematic and state diagram
```
