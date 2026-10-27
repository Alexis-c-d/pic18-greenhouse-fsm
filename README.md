# PIC18 Greenhouse Climate Controller

A complete climate-control FSM for a greenhouse, featuring temperature & humidity sensing, PWM-controlled fan/heater, LCD UI, and UART telemetry.

## What recruiters love
- **Five-state FSM** with hysteresis and alarm handling.
- **Modular drivers** for ADC, I2C, PWM, and UART.
- **Real-world documentation** in `docs/EVALUACION FINAL.pdf`.

## Quick demo
```bash
git clone https://github.com/Alexis-c-d/pic18-greenhouse-fsm.git
cd pic18-greenhouse-fsm/src
# Compile main.c, load onto PIC18, watch UART output.
```

🗂️ **Docs** - full project report in `docs/EVALUACION FINAL.pdf`.

---

## Repository layout
```
pic18-greenhouse-fsm/
├─ src/        # src/main.c - FSM + drivers (ADC, PWM, I2C, UART, LCD)
├─ sim/        # Proteus simulation files
└─ docs/       # EVALUACION FINAL.pdf (project report)
```

## License
MIT - feel free to use for learning or as a base for your own greenhouse controller.
