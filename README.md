# TroisMolettes

A physical, tangible IR remote control for a Daikin split AC unit — no screen, no app, no menus. All controls are hardware (rotary switches, push buttons); the **knob positions are the state** — the only feedback is a single LED that flashes on IR transmission.

![Remote front panel](images/remote_front_panel.svg)

![Remote render](images/daikin_remote_web.png)

![Daikin unit](images/daikin_unit_web.png)

## Inspiration

[![Inspiration](images/inspi_web.png)](https://www.wired.com/2015/03/gadget-prototyping-internet-things/?utm_source=Pinterest&utm_medium=organic#slide-3)

## Concept

Three rotary knobs control **fan speed/power**, **mode**, and **temperature**. Four push buttons handle secondary modes (Powerful, Econo, Swing, Resend). The physical knob positions *are* the displayed state — there is no per-position indicator, only a single LED that flashes on IR send. A single IR LED transmits to the AC unit.

Target AC unit: Daikin FTXM20N2V1B · Original remote: ARC466A33 · Protocol: `DAIKIN` via IRremoteESP8266

## Files

| File | Description |
|---|---|
| [00_specifications.md](00_specifications.md) | Full project requirements: controls, feedback, power, enclosure |
| [01_technical_design_overview.md](01_technical_design_overview.md) | Design summary: choices, trade-offs, and the open decisions (readout/wake, sleep current) |
| [01_IR_protocol_and_mapping.md](01_IR_protocol_and_mapping.md) | Daikin IR protocol (frame structure, parameters, library usage, control mapping) |
| [02_BOM_prototype.csv](02_BOM_prototype.csv) | Bill of materials with prices and sourcing notes |
| [03_microcontroller_choice.md](03_microcontroller_choice.md) | MCU comparison: Pico vs ESP32 vs Arduino, decision rationale |
| [04_rotary_switch_choice.md](04_rotary_switch_choice.md) | Rotary switch families compared, part selection, decision matrix |
| [05_electronics_circuit.md](05_electronics_circuit.md) | Circuit design: ADC ladder readout, wake detection, IR driver, gated power rail, battery budget, sleep/wake sequence |

## Hardware summary

- **MCU:** Raspberry Pi Pico (RP2040) — preferred for low sleep current and 2 MB flash (stock board sleeps ~1.3 mA; a bare RP2040 + LDO may be needed to hit the 6-month target)
- **IR:** 940 nm LED + NPN transistor driver · IRremoteESP8266 library · `IRDaikinAC` class
- **Inputs:** 3 rotary selectors (all 1-pole) + 4 tactile buttons
- **Rotary readout:** each switch diode-encoded into a small binary code on its own GPIO; the code lines double as both-edge wake interrupts (no ADC, no 2-pole, no analog rail) — see [05_electronics_circuit.md](05_electronics_circuit.md#3-readout--wake--design-options)
- **Feedback:** single TX indicator LED (knob positions are the state)
- **Power:** Li-Po 3.7 V · TP4056 USB-C charging module · MCU DORMANT between transmissions, woken by GPIO edge (6-month battery target — bench-validation pending)
- **BOM cost:** < €35
- **Enclosure:** 3D-printed ~80 × 100 × 25 mm, handheld or wall-mounted
