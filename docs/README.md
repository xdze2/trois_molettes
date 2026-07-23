# Design docs

Requirements, part choices, and circuit/firmware design for the TroisMolettes AC
remote. There is no separate overview doc: start with
[00_specifications.md](00_specifications.md) for the requirements and the
stateless-interface principle, then each numbered file goes deep on one subsystem
(the `A*` files are technical annexes — reference, not reading order). Full file
list: see the table in the [root README](../README.md#files). Bench logs (what's
proven on real hardware) are in [`../howtos/`](../howtos/).

## The tension that shapes every decision

The whole design is pulled in two directions at once: the **stateless interface**
(knob position = state, big tangible controls) and the **6-month battery target**.
Most of the open questions below are where those two collide. Two decisions carry
the most weight because they span several subsystems:

- **MCU chosen on sleep current + multi-pin wake, not the IR library.** The device
  sleeps ~100% of the time, so sleep current ≈ average current ≈ the battery budget;
  and the readout needs ~12 GPIO that can wake on both edges. Daikin IR is a
  hand-portable byte array, so library support was *last*, not first. → [03](03_microcontroller_choice.md), [07](07_battery_and_power.md)
- **Readout and wake are the same lines.** Each 1-pole rotary switch is diode-encoded
  into a small binary code; those same code lines, armed for both-edge PCINT, provide
  the wake — so no separate wake pole, and switches are chosen on feel and size alone. → [05](05_electronics_circuit.md)

## Open questions (what still gates the build)

All three subsystems are individually proven on the bench (selector readout +
sleep/wake, real Daikin IR comms, AVR port), so the remaining questions are about
**integration and finish**, not feasibility:

1. **Sleep-current measurement** — measure `SLEEP_MODE_PWR_DOWN` on the assembled
   board with the power LED removed. Biggest lever on the 6-month target; decides
   cell size and whether to swap the Pro Mini LDO. → [07](07_battery_and_power.md)
2. **Swing support** — confirm the FTXM20N2V1B accepts a swing toggle over IR
   (decides whether the optional Swing control ships). → [A1](A1_IR_protocol_and_mapping.md)
3. **IR LED mounting** — part fixed (TSAL6200); open is the mount: 3× fan vs.
   aimable friction-gimbal head. → [06](06_IR_LED_wiring.md)
4. **Knob / shaft fit** — confirm SR16 and RS1010 6 mm shafts accept the same knob
   cap; confirm detent feel.
5. **Panel fit** — two SR16 + one RS1010 + Send (+ optional Swing) on the 80×100 mm face.
6. **BOM ≤ €35** — source-check the final switch set + diodes on Tayda / Mouser / LCSC. → [02](02_BOM_prototype.csv)
