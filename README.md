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
| [03_microcontroller_choice.md](03_microcontroller_choice.md) | MCU comparison (nRF52840 / STM32L4 / ATmega328P) on sleep current + multi-pin wake; ATmega328P chosen, rationale |
| [04_rotary_switch_choice.md](04_rotary_switch_choice.md) | Rotary switch families compared, part selection, decision matrix |
| [05_electronics_circuit.md](05_electronics_circuit.md) | Input wiring: diode-encoded readout, multi-GPIO PCINT wake, pin map, sleep/wake sequence |
| [06_IR_LED_wiring.md](06_IR_LED_wiring.md) | IR emitter: TSAL6200 + S9013 driver, single/3× wiring, bulk cap, wide-angle vs range, mounting strategies |
| [07_battery_and_power.md](07_battery_and_power.md) | Power architecture: Li-Po + TP4056, sleep-current budget, Pro Mini battery mods, charging |
| [10_software_architecture.md](10_software_architecture.md) | Firmware layers: portable Daikin frame builder, AVR HAL (Timer2/sleep), Linux mock |

## Bench logs (build journal)

The [howtos/](howtos/) directory is the running record of bringing each subsystem up
on real hardware — including the dead-ends, false positives, and fixes. This is where
the three milestones (selector readout + sleep/wake, real Daikin IR comms, AVR port)
were actually proven.

| Log | What it covers |
|---|---|
| [01_arduino_setup.md](howtos/01_arduino_setup.md) | Pro Mini / Nano (ATmega328PB) first flash: board config, CH340 wiring, 8 MHz upload speed |
| [02_serial_debug.md](howtos/02_serial_debug.md) | Garbled-serial root cause (CKDIV8 fuse → 4 MHz), workaround, and permanent ISP bootloader fix |
| [03_rs1010_readout.md](howtos/03_rs1010_readout.md) | Diode-encoded rotary readout: GPIO config, inter-detent glitch, 10 ms debounce, PCINT sleep+wake |
| [04_ir_receiver_signal.md](howtos/04_ir_receiver_signal.md) | TSOP38238 + scope: confirming the 3-frame Daikin structure and pulse-distance bit timing |
| [05_ir_modulation_test.md](howtos/05_ir_modulation_test.md) | IR LED loopback + carrier check; the false-positive trap where a mis-wired transistor still "passed" |
| [06_ir_rx_dump.md](howtos/06_ir_rx_dump.md) | Capturing real ARC466A33 frames with the ATmega (PCINT delta buffer); checked-in reference dump |
| [07_verify_frame_against_capture.md](howtos/07_verify_frame_against_capture.md) | Diffing `daikin_build_frame()` vs the capture — found 5 wrong fixed bytes breaking the checksums |
| [08_daikin_fan_toggle.md](howtos/08_daikin_fan_toggle.md) | **Working end-to-end:** full frame over IR, AC beeps & changes fan speed; gap-timing + long-delay fixes |

## Hardware summary

- **MCU:** ATmega328P (Pro Mini 3.3 V) — on hand, sufficient for this design, ported and tested. Sleeps in `SLEEP_MODE_PWR_DOWN`, woken by PCINT both-edge on every code/button line. Sleep floor dominated by the Pro Mini LDO quiescent (~75 µA) — bench-validation pending. (nRF52840 / STM32L4 were evaluated for lower µA-class sleep; see [03_microcontroller_choice.md](03_microcontroller_choice.md))
- **IR:** TSAL6200 940 nm LED (×1 or ×3 fan) + S9013 NPN transistor driver · 38 kHz carrier via the 328P Timer2 (OC2B, pin D3) · Daikin ARC466A33 frame ported from the documented format and validated against the real unit
- **Inputs:** 3 rotary selectors (all 1-pole) + 4 tactile buttons
- **Rotary readout:** each switch diode-encoded into a small binary code on its own GPIO; the code lines double as both-edge wake interrupts (no ADC, no 2-pole, no analog rail) — see [05_electronics_circuit.md](05_electronics_circuit.md#3-readout--wake--design-options)
- **Feedback:** single TX indicator LED (knob positions are the state)
- **Power:** Li-Po 3.7 V · TP4056 USB-C charging module · MCU in `SLEEP_MODE_PWR_DOWN` between transmissions, woken by PCINT edge (6-month → multi-year battery target — bench-validation pending)
- **BOM cost:** < €35
- **Enclosure:** 3D-printed ~80 × 100 × 25 mm, handheld or wall-mounted
