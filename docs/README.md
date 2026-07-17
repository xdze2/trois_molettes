# Design docs

The design notes for the TroisMolettes AC remote — requirements, part choices, and the
circuit/firmware design. Start with the [technical design overview](01_technical_design_overview.md)
for the high-level map; the rest go deep on one subsystem each. For the hardware bring-up
bench logs (what was actually proven on real hardware), see [`../howtos/`](../howtos/).

| File                                                              | Description                                                                                                       |
| ----------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| [00_specifications.md](00_specifications.md)                       | Full project requirements: controls, feedback, power, enclosure                                                  |
| [01_technical_design_overview.md](01_technical_design_overview.md) | Design summary: choices, trade-offs, and the open decisions (readout/wake, sleep current)                        |
| [01_IR_protocol_and_mapping.md](01_IR_protocol_and_mapping.md)     | Daikin IR protocol (frame structure, parameters, library usage, control mapping)                                 |
| [02_BOM_prototype.csv](02_BOM_prototype.csv)                       | Bill of materials with prices and sourcing notes                                                                 |
| [03_microcontroller_choice.md](03_microcontroller_choice.md)       | MCU comparison (nRF52840 / STM32L4 / ATmega328P) on sleep current + multi-pin wake; ATmega328P chosen, rationale |
| [04_rotary_switch_choice.md](04_rotary_switch_choice.md)           | Rotary switch families compared, part selection, decision matrix                                                 |
| [05_electronics_circuit.md](05_electronics_circuit.md)             | Input wiring: diode-encoded readout, multi-GPIO PCINT wake, pin map, sleep/wake sequence                         |
| [06_IR_LED_wiring.md](06_IR_LED_wiring.md)                         | IR emitter: TSAL6200 + S9013 driver, single/3× wiring, bulk cap, wide-angle vs range, mounting strategies        |
| [07_battery_and_power.md](07_battery_and_power.md)                 | Power architecture: Li-Po + TP4056, sleep-current budget, Pro Mini battery mods, charging                        |
| [10_software_architecture.md](10_software_architecture.md)         | Firmware layers: portable Daikin frame builder, AVR HAL (Timer2/sleep), Linux mock                               |
| [11_serial_remote_app.md](11_serial_remote_app.md)                 | Python Textual TUI soft front-panel over serial → ATmega → IR: protocol spec, app architecture (planned)         |
