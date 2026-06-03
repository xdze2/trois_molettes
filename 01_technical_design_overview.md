# Technical Design Overview

A high-level map of the design: the choices made, the ones still open, and the trade-offs behind each. This is the summary — the detail lives in the dedicated docs:

- [00_specifications.md](00_specifications.md) — requirements and the stateless-interface principle
- [01_IR_protocol_and_mapping.md](01_IR_protocol_and_mapping.md) — Daikin IR protocol and control mapping
- [02_BOM_prototype.csv](02_BOM_prototype.csv) — parts and sourcing
- [03_microcontroller_choice.md](03_microcontroller_choice.md) — MCU comparison
- [04_rotary_switch_choice.md](04_rotary_switch_choice.md) — rotary switch part selection
- [05_electronics_circuit.md](05_electronics_circuit.md) — the circuit: readout, wake, IR driver, power

The whole design is pulled in two directions at once: the **stateless interface** (knob position = state, big tangible controls) and the **6-month battery target**. Most open questions are where those two collide.

---

## 1. The shape of the device

Three rotary selectors (Fan/Power, Mode, Temperature) + 3–4 push buttons, an IR LED, one indicator LED, on a sleeping MCU. No screen, no per-position lights — the knobs *are* the display. See [00_specifications.md](00_specifications.md).

| Block | Choice | Status |
|---|---|---|
| MCU | RP2040 (Raspberry Pi Pico) | preferred, pending IR-library validation |
| IR | 940 nm LED + NPN driver, IRremoteESP8266 `IRDaikinAC` | chosen |
| Rotary readout + wake | **per-switch diode encoding on 1P switches (Approach D)** | chosen; bench-validate DORMANT wake |
| Buttons | momentary → GPIO, internal pull-up, wake on edge | chosen |
| Feedback | single TX LED (WS2812B chain dropped) | chosen |
| Power | Li-Po + TP4056, gated 3.3 V rail, deep sleep | chosen; cell size pending |

---

## 2. MCU — RP2040, with a sleep-current caveat

RP2040 is preferred: best realistic sleep current, 2 MB flash, direct Li-Po on VSYS, cheap genuine board. Fallback is an ESP32 DevKit if IRremoteESP8266 won't run on the Pico (Arduino-Pico core) — that compatibility is the **first thing to validate**, before any assembly.

**The caveat that shapes everything downstream:** the ~1–2 mA figure is the die; a *stock Pico board* sleeps at ~1.3 mA because of its always-on SMPS. That alone gives only ~9 weeks on a 2000 mAh cell. Hitting 6 months likely needs a **bare RP2040/RP2350 + efficient LDO**, plus gating every always-on current path. See [03_microcontroller_choice.md](03_microcontroller_choice.md) and [Battery budget](05_electronics_circuit.md#6-battery-budget).

---

## 3. Rotary readout + wake — decided: per-switch diode encoding

This coupled two requirements that look separate but aren't:

1. **Read absolute position** of each selector (stateless principle — must survive sleep and battery removal).
2. **Wake the MCU** when a knob moves (battery — the MCU is asleep and a knob is not an interrupt source by default).

**Decision: each rotary switch is a plain 1-pole part, diode-encoded into a small binary code on its own GPIO; those same code lines, armed for both-edge interrupts, provide the wake.** Buttons are direct GPIO. No ADC, no analog ladder, no 2-pole switches, no comparator.

This is the same trick a coded rotary switch uses, wired by hand onto a **big, tactile knob** — which is what the interface wants and the off-the-shelf coded parts (tiny, screwdriver-slot) don't give.

| Selector | Positions | Bits → GPIO |
|---|---|---|
| Fan / Power | 6 | 3 |
| Mode | 4 | 2 |
| Temperature | 11 | 4 |

Plus 4 button GPIO ≈ **13 input pins**, all interrupt-armed, of the RP2040's 26.

### Why this, over the alternatives considered

- **Readout and wake come from the same lines.** Any knob move flips ≥1 code bit → a both-edge interrupt on that line wakes the MCU. (A single OR'd wake pin would miss non-zero→non-zero moves — must be *per-line* edges.) So no separate wake pole is needed, which is what **removes the 2-pole requirement** and lets every switch be chosen on **feel and size alone**.
- **Rejected:** resistor-ladder + 2P (needs scarce 2P ≥10-pos parts, a gated analog rail, ADC calibration); ladder + comparator (keeps the ladder *and* adds an IC); off-the-shelf coded switch (right circuit, wrong feel). All recorded in [05_electronics_circuit.md §3](05_electronics_circuit.md#3-readout--wake--design-options).
- **Cost:** ~19 small-signal diodes for the temperature switch (fewer for Fan/Mode), loose 1N4148/BAV99 — no dedicated "diode array" part exists for this. Lays out cleanly on perfboard (one rail per code line).

### Still to validate on the bench

The wake depends on **DORMANT-mode GPIO wake reached from the Arduino-Pico core** (which IRremoteESP8266 needs). RP2040 supports wake from any GPIO, but the Arduino-core path to `dormant_wake` must be proven — same bench session as the IR-library check. If it's painful, it bears on the MCU choice. See [05 §8](05_electronics_circuit.md#8-open-questions).

---

## 4. Push buttons

3–4 momentary buttons → GPIO with internal pull-up, each an interrupt/wake source. Powerful and Econo are mutually exclusive; Resend just retransmits. The button toggle states (Powerful / Econo / Swing) are the **only** internal software state — they have no physical position to read back, unlike the knobs. See mapping in [01_IR_protocol_and_mapping.md §5.5](01_IR_protocol_and_mapping.md).

---

## 5. Feedback — single TX LED

One indicator LED, flashes on IR send, confirms the device is alive. The 25-LED WS2812B chain was dropped: ~15 mA quiescent kills the battery target, and it's redundant with "knob position = state." Firm decision. Drive detail in [05_electronics_circuit.md](05_electronics_circuit.md).

---

## 6. Power

Li-Po single cell, TP4056 USB-C charge/protect module, 100 µF bulk cap for the IR pulse. No analog rail to gate — the digital encoding's only standing current is a few µA of switch leakage, kept small with weak pull-downs. The dominant sleep term is **board-level standby** (~1.3 mA on a stock Pico), so cell size is left **TBD until that's measured** — the measurement, not a guess, sizes the battery and decides stock-board vs bare-chip. Detail and budget in [05_electronics_circuit.md §5–6](05_electronics_circuit.md#5-power).

---

## 7. Send behaviour

Send a fresh IR frame on every control change — no confirm step. A settle-window debounce collapses a multi-detent knob sweep into a single send and avoids reading a switch mid-transition. Resend retransmits current state unchanged. Sequence in [05_electronics_circuit.md §7](05_electronics_circuit.md#7-sleep--wake-sequence).

---

## 8. Open questions (the decisions that still gate the build)

1. **IRremoteESP8266 + DORMANT wake on Pico (Arduino core)** — validate together, first; the diode-encoded wake depends on reaching `dormant_wake` from the Arduino core. Determines RP2040 vs ESP32.
2. **Achievable sleep current** — bench-measure the chosen board in DORMANT; decides bare-chip vs stock board, and the cell size. The biggest lever on the 6-month target.
3. **Secondary modes** — confirm Powerful / Econo / Swing are accepted by the FTXM20N2V1B over IR (sets the final button count).
4. **Switch choice on feel** — pick the three 1P switches on detent quality and body size (no pole constraint now); confirm shaft/knob fit.
5. **Enclosure fit** — three knobs (big bodies) side by side in the 80×100 mm panel.
6. **BOM ≤ €35** — source-check the final 1P switch set + diodes on Tayda / Mouser / LCSC.

(Resolved: readout = per-switch diode encoding on 1P switches. Mode = 4 positions. Feedback = single TX LED. IR = `IRDaikinAC` / DAIKIN.)
