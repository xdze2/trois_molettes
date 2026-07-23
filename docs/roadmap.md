# Roadmap

## Prototype v1

v1 is the bench prototype: three subsystems — selector readout + sleep/wake, real
Daikin IR comms, AVR firmware port — each individually proven on real hardware, then
integrated on a wood panel running end-to-end. This doc collects what v1 deliberately
leaves open, so the later builds have one place to look. Nothing here is a
feasibility question; it's all **integration, sourcing, and finish**.

## v1.2 — power & inputs

- Sleep mode: measure `SLEEP_MODE_PWR_DOWN` energy use on the assembled board.
  → [07](07_battery_and_power.md)
- Test the temperature potentiometer.
- Battery: AA with regulator? Measure sleep/active current before committing.
  → [07](07_battery_and_power.md)

## v2 — PCB & enclosure

- PCB with directly-mounted switches — learn KiCad. → [05](05_electronics_circuit.md)
- IR LED: single wide-angle LED instead of the 3× fan. → [06](06_IR_LED_wiring.md)
- Readout transient rejection: confirm switch contact style, then choose strobe/valid
  line (A) vs debounce timing (C). Gray code helps only the shorting case; 8 positions
  on 3 bits leaves no spare "invalid" codeword. → [05 §1, §3](05_electronics_circuit.md)
- Wall-mountable enclosure.
