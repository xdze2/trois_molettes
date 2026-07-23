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

## v2 — switches, board & enclosure

Sourcing shifts from AliExpress (v1) to Mouser. The pivotal decision is the rotary
selector.

- **Rotary selector — coded absolute encoder vs. plain switch + diodes.** Leading
  candidate is the **Bourns PAC18R absolute encoder** (`PAC18R1-41D19F`): 4-bit Gray
  code, 8 positions, 2.5 mm-pitch THT, real flatted knob shaft, haptic detents. It
  deletes the 29-diode readout array and, being Gray-coded, **retires the transient-
  rejection question entirely** (no strobe/debounce fork). Open: confirm live Mouser
  price vs the ≤ €5/unit target; if it overshoots, fall back to the v1 plain-switch +
  diode matrix. → [05_electronics_circuit_v2.md](05_electronics_circuit_v2.md)
- **PCB vs. perfboard.** A custom PCB is only *forced* if a part is off the 2.54 mm
  grid. Because the PAC18R is on-grid *and* removes the diode array (the main thing
  bloating the perfboard), v2 may stay on **perfboard** — no KiCad/fab order. Learn
  KiCad only if the layout still demands it. → [05_electronics_circuit_v2.md](05_electronics_circuit_v2.md)
- IR LED: single wide-angle LED instead of the 3× fan. → [06](06_IR_LED_wiring.md)
- Wall-mountable enclosure.
