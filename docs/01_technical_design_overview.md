# Technical Design Overview

A high-level map of the design: the choices made, the ones still open, and the trade-offs behind each. This is the summary — the detail lives in the dedicated docs:

- [00_specifications.md](00_specifications.md) — requirements and the stateless-interface principle
- [01_IR_protocol_and_mapping.md](01_IR_protocol_and_mapping.md) — Daikin IR protocol and control mapping
- [02_BOM_prototype.csv](02_BOM_prototype.csv) — parts and sourcing
- [03_microcontroller_choice.md](03_microcontroller_choice.md) — MCU comparison
- [05_electronics_circuit.md](05_electronics_circuit.md) — rotary switch selection and the circuit: readout, wake, IR driver, power

The whole design is pulled in two directions at once: the **stateless interface** (knob position = state, big tangible controls) and the **6-month battery target**. Most open questions are where those two collide.

---

## 1. The shape of the device

Three rotary selectors (Fan/Power, Mode, Temperature) + a Send button (+ optional Swing toggle), an IR LED, one indicator LED, on a sleeping MCU. No screen, no per-position lights — the knobs *are* the display. See [00_specifications.md](00_specifications.md).

| Block | Choice | Status |
|---|---|---|
| MCU | **ATmega328P (Pro Mini 3.3 V)** | locked — ported and tested; bench-validate sleep current |
| IR | TSAL6200 940 nm LED (×1 or ×3) + S9013 driver; **ported Daikin frame** | locked — validated against the real unit |
| Rotary readout + wake | **per-switch diode encoding on 1P switches** | locked; deep-sleep PCINT wake validated on the RS1010 |
| Buttons | momentary → GPIO, internal pull-up, wake on edge | locked |
| Feedback | single TX LED (WS2812B chain dropped) | locked |
| Power | Li-Po + TP4056, weak switch pull-downs, deep sleep | chosen; cell size pending bench measurement |

---

## 2. MCU — chosen on sleep current and wake, not on the IR library

The device sleeps ~100% of the time, so sleep current ≈ average current ≈ the whole battery budget, and the diode-encoded readout needs ~12 GPIO that can wake the MCU on both edges. Those two constraints, plus a perfboard-only build rule (no PCB fabrication), drove the choice.

**Locked: ATmega328P (Pro Mini 3.3 V).** On hand, simplest to hand-build, and it clears both gates: `SLEEP_MODE_PWR_DOWN` with PCINT both-edge wake on every code/button line (genuinely per-pin, including non-zero→non-zero code changes), and a documented AVR IR path via Timer2. The board sleep floor is dominated by the Pro Mini's onboard LDO quiescent (~75 µA) rather than the chip itself (~0.1 µA power-down) — mitigated by removing the power LED and optionally swapping the LDO (see [07_battery_and_power.md §4](07_battery_and_power.md)).

The lower-µA candidates — nRF52840 (Seeed XIAO, ~1.5–2.4 µA board sleep) and STM32L4 STOP2 (~2–4 µA) — remain the upgrade path if the LDO floor proves too high for the battery target. Full comparison in [03_microcontroller_choice.md](03_microcontroller_choice.md); budget in [07_battery_and_power.md](07_battery_and_power.md).

---

## 3. Rotary readout + wake — decided: per-switch diode encoding

This coupled two requirements that look separate but aren't:

1. **Read absolute position** of each selector (stateless principle — must survive sleep and battery removal).
2. **Wake the MCU** when a knob moves (battery — the MCU is asleep and a knob is not an interrupt source by default).

**Decision: each rotary switch is a plain 1-pole part, diode-encoded into a small binary code on its own GPIO; those same code lines, armed for both-edge interrupts, provide the wake.** Buttons are direct GPIO. No ADC, no analog ladder, no 2-pole switches, no comparator.

This is the same trick a coded rotary switch uses, wired by hand onto a **big, tactile knob** — which is what the interface wants and the off-the-shelf coded parts (tiny, screwdriver-slot) don't give.

| Selector | Positions | Bits → GPIO |
|---|---|---|
| Fan / Power | 8 (SR16) | 3 |
| Mode | 5 (RS1010) | 3 |
| Temperature | 8 (SR16) | 3 |

Plus Send (+ optional Swing) button GPIO ≈ **10–11 input pins**, all interrupt-armed — well within the 328P's pin count (see the full pinout in [05_electronics_circuit.md §1](05_electronics_circuit.md)).

### Why this, over the alternatives considered

- **Readout and wake come from the same lines.** Any knob move flips ≥1 code bit → a both-edge interrupt on that line wakes the MCU (a single OR'd wake pin would miss non-zero→non-zero moves — must be per-line edges). No separate wake pole is needed, which removes the 2-pole requirement and lets every switch be chosen on feel and size alone.
- **Rejected:** resistor-ladder + 2P (needs scarce 2P ≥10-pos parts, a gated analog rail, ADC calibration); ladder + comparator (keeps the ladder *and* adds an IC); off-the-shelf coded switch (right circuit, wrong feel). Detail in [05_electronics_circuit.md §3](05_electronics_circuit.md#3-readout--wake--design-options).
- **Cost:** ~29 small-signal diodes total (1N4148/BAV99), one rail per code line — lays out cleanly on perfboard.

### Still to validate on the bench

PCINT both-edge wake from `SLEEP_MODE_PWR_DOWN` is **validated on the RS1010** (10 ms settle window, see [howtos/03_rs1010_readout.md](../howtos/03_rs1010_readout.md)) — every line wakes on either edge, including non-zero→non-zero code changes. What remains is the **sleep-current measurement** on the assembled board (power LED removed) to fix the cell size.

---

## 4. Buttons

**Send** — momentary button → GPIO with internal pull-up, an interrupt/wake source. It is the only action that transmits IR; it changes no setting (the knobs hold the state). An optional **Swing** toggle (if panel space allows) is the only control with internal software state — it has no absolute physical position to read back, unlike the knobs. See mapping in [01_IR_protocol_and_mapping.md](01_IR_protocol_and_mapping.md).

---

## 5. Feedback — single TX LED

One indicator LED, flashes on IR send, confirms the device is alive. The 25-LED WS2812B chain was dropped: ~15 mA quiescent kills the battery target, and it's redundant with "knob position = state." Drive detail in [05_electronics_circuit.md](05_electronics_circuit.md).

---

## 6. Power

Li-Po single cell, TP4056 USB-C charge/protect module, 220 µF bulk cap for the IR pulse. No analog rail to gate — the digital encoding's only standing current is a few µA of switch leakage, kept small with weak pull-downs. The Pro Mini's LDO quiescent (~75 µA) is the dominant sleep term, not the switch leakage; the 6-month target is reachable on a small cell after removing the power LED, with room to push toward "years" by swapping the LDO. Cell size is TBD until sleep current is measured on the assembled board. Detail and budget in [07_battery_and_power.md](07_battery_and_power.md).

---

## 7. Send behaviour

Send a fresh IR frame on every control change — no confirm step. A settle-window debounce collapses a multi-detent knob sweep into a single send and avoids reading a switch mid-transition. Resend retransmits current state unchanged. Sequence in [05_electronics_circuit.md §7](05_electronics_circuit.md#7-sleep--wake-sequence).

---

## 8. Open questions (the decisions that still gate the build)

With the three subsystems individually proven (selector readout + sleep/wake, real Daikin IR comms, AVR port), the remaining questions are about **integration and finish**, not feasibility:

1. **Sleep current measurement** — measure `SLEEP_MODE_PWR_DOWN` current on the assembled board with the power LED removed. Biggest lever on the 6-month target; decides cell size and whether to swap the LDO.
2. **Swing support** — confirm the FTXM20N2V1B accepts a swing toggle over IR (decides whether the optional Swing control ships).
3. **IR LED mounting** — part is fixed (TSAL6200); open is the mount: 3× fan vs. aimable friction-gimbal head (see [06_IR_LED_wiring.md](06_IR_LED_wiring.md)).
4. **Knob / shaft fit** — confirm the SR16 and RS1010 6 mm shafts accept the same knob cap; confirm detent feel.
5. **Panel fit** — two SR16 + one RS1010 + Send (+ optional Swing) on the 80×100 mm face.
6. **BOM ≤ €35** — source-check the final switch set + diodes on Tayda / Mouser / LCSC.

(Resolved: MCU = ATmega328P (Pro Mini 3.3 V), ported and tested. Readout = per-switch diode encoding on 1P switches; PCINT both-edge wake validated. Positions = Fan 8 / Mode 5 / Temp 8. Feedback = single TX LED. IR = DAIKIN frame ported to AVR (`IRDaikinAC` as reference), validated against the real unit.)
