# TroisMolettes

A physical, tangible IR remote control for a Daikin split AC unit — no screen, no app, no menus. All controls are hardware (rotary switches, push buttons); the **knob positions are the state** — the only feedback is a single LED that flashes on IR transmission.

![Remote front panel](images/remote_front_panel.svg)

![Remote render](images/daikin_remote_web.png)

![Daikin unit](images/daikin_unit_web.png)

## Inspiration

[![Inspiration](images/inspi_web.png)](https://www.wired.com/2015/03/gadget-prototyping-internet-things/?utm_source=Pinterest&utm_medium=organic#slide-3)

## Concept

Three rotary knobs control **fan speed/power**, **mode**, and **temperature**. A **Send** push button transmits the current state via IR; an optional **Swing** toggle handles louvre swing if panel space allows. The physical knob positions *are* the displayed state — there is no per-position indicator, only a single LED that flashes on IR send. A single (or 3× fanned) IR LED transmits to the AC unit.

Target AC unit: Daikin FTXM20N2V1B · Original remote: ARC466A33 · Protocol: `DAIKIN` (frame ported to AVR `IRremote`; `IRDaikinAC` from IRremoteESP8266 used as the reference format)

## Files

| File | Description |
|---|---|
| [00_specifications.md](00_specifications.md) | Full project requirements: controls, feedback, power, enclosure |
| [01_technical_design_overview.md](01_technical_design_overview.md) | Design summary: choices, trade-offs, and the open decisions (readout/wake, sleep current) |
| [01_IR_protocol_and_mapping.md](01_IR_protocol_and_mapping.md) | Daikin IR protocol (frame structure, parameters, library usage, control mapping) |
| [02_BOM_prototype.csv](02_BOM_prototype.csv) | Bill of materials with prices and sourcing notes |
| [03_microcontroller_choice.md](03_microcontroller_choice.md) | MCU comparison: chosen on sleep current + 13-pin wake (nRF52840 / STM32L4 / ATmega328P), decision rationale |
| [04_rotary_switch_choice.md](04_rotary_switch_choice.md) | Rotary switch families compared, part selection, decision matrix |
| [05_electronics_circuit.md](05_electronics_circuit.md) | Circuit design: diode-encoded readout, multi-GPIO wake, IR driver, power, battery budget, sleep/wake sequence |

## Hardware summary

- **MCU:** ATmega328P (Pro Mini 3.3 V) — on hand, sufficient for this design, ported and tested. Sleeps in `SLEEP_MODE_PWR_DOWN`, woken by PCINT both-edge on every code/button line. Sleep floor dominated by the Pro Mini LDO quiescent (~75 µA) — bench-validation pending. (nRF52840 / STM32L4 were evaluated for lower µA-class sleep; see [03_microcontroller_choice.md](03_microcontroller_choice.md))
- **IR:** TSAL6200 940 nm LED (×1 or ×3 fan) + S9013 NPN transistor driver · 38 kHz carrier via the 328P Timer2 (OC2B, pin D3) · Daikin ARC466A33 frame ported from the documented format and validated against the real unit
- **Inputs:** 3 rotary selectors (all 1-pole) + 4 tactile buttons
- **Rotary readout:** each switch diode-encoded into a small binary code on its own GPIO; the code lines double as both-edge wake interrupts (no ADC, no 2-pole, no analog rail) — see [05_electronics_circuit.md](05_electronics_circuit.md#3-readout--wake--design-options)
- **Feedback:** single TX indicator LED (knob positions are the state)
- **Power:** Li-Po 3.7 V · TP4056 USB-C charging module · MCU in `SLEEP_MODE_PWR_DOWN` between transmissions, woken by PCINT edge (6-month → multi-year battery target — bench-validation pending)
- **BOM cost:** < €35
- **Enclosure:** 3D-printed ~80 × 100 × 25 mm, handheld or wall-mounted
