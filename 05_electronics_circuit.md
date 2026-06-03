# Electronics Circuit

The circuit-level design for the remote. Covers how the rotary switches are read and how they wake the MCU, the IR transmit driver, the power architecture, and the decoupling. For **which switch parts** to use see [04_rotary_switch_choice.md](04_rotary_switch_choice.md); for requirements see [00_specifications.md](00_specifications.md); for high-level implementation choices see [01_technical_design.md](01_technical_design.md).

The whole circuit is shaped by one hard constraint: the **6-month battery target**. The MCU sleeps between transmissions, and every always-on current path (resistor ladders, indicator LEDs, board regulator) is treated as a budget item. See [Battery budget](#battery-budget).

---

## 1. Block overview

```
                 ┌──────────────────────────────────────────┐
   Li-Po ──► TP4056 ──► VSYS ──► [MCU: RP2040] ──► GPIO ──► IR LED driver ──► 940 nm LED
   (3.0–4.2 V)  (charge/protect)        │  ▲
                                        │  │  pole-2 wake edges (3 GPIO, IRQ)
   gated 3.3 V rail ◄── GPIO gate ──────┘  └── ADC0–2 ◄── 3 resistor ladders (pole 1)
        │                                          ▲
        └──────────────────────────────────────────┘  (rail powers the ladders; OFF in sleep)

   buttons ──► 4 GPIO (pull-up, IRQ)        TX indicator LED ──► 1 GPIO (direct drive)
```

Pin budget (RP2040): 3 ADC + 3 wake GPIO + 4 button GPIO + 1 IR + 1 TX LED + 1 rail gate = **12 pins**. Well within 26.

---

## 2. Rotary readout — resistor ladder on ADC

Each selector's **pole 1** is wired as a resistor ladder on a single ADC pin. Equal resistors in series between each position tap; the common wiper goes to the ADC; one end of the chain to the gated 3.3 V rail, the other to GND.

- Mode (4 pos) → 1 ADC pin
- Fan (6 pos) → 1 ADC pin
- Temperature (11 pos) → 1 ADC pin

Total: **3 ADC pins**. On RP2040 (Pico), ADC0–ADC2 are all usable without restriction.

**Ladder resistor value:** 10 kΩ per step is a safe starting point. Voltage spacing is ~300 mV per step for the 11-position ladder (3.3 V ref, 12-bit ADC → ~0.8 mV/LSB). Margin is comfortable — noise is typically a few mV.

**Power the ladder from the regulated, gated 3.3 V rail — not VSYS (raw Li-Po).** A varying supply would shift every ADC reading as the battery drains. Powering it from a *gated* rail (rather than the always-on 3.3 V) is what lets the ladders draw zero current during sleep — see [§5 Power](#5-power) and [Battery budget](#battery-budget).

---

## 3. Wake-from-sleep detection

The MCU sleeps between transmissions and must wake on any knob or button change. Buttons naturally generate a GPIO edge (pull-up + IRQ). Rotary switches on a resistor ladder do **not** — a position change produces a DC voltage shift, not an edge — so they need a dedicated wake mechanism.

### 3.1 Do not rely on the inter-contact float

An obvious approach is to detect the wiper floating between contacts — when the wiper lifts off a contact, a pull-up drives the GPIO high, producing a rising edge. **This is fragile** and rejected:

- **Non-shorting vs shorting switches.** Non-shorting (break-before-make) switches have a guaranteed float period; shorting (make-before-break) switches may have *no* float at all — the wake pulse may never occur.
- **Float duration is unspecified.** Even on non-shorting switches, only the *order* of events is guaranteed, not the duration. A fast detent snap can float for only a few microseconds — too short to reliably trigger a GPIO interrupt given MCU wake latency.

**Conclusion:** detect the voltage change on *landing*, not the float — via a second pole (Option A) or an HPF spike on the ladder wire (Option B).

### 3.2 Option A — 2-pole switch, alternating-contact wake (preferred)

One pole drives the ADC ladder. The **second pole** generates the wake edge.

> **Do not tie all pole-2 contacts to a single level.** If every pole-2 contact is shorted to 3.3 V (or all to GND), the wiper sits at the same level in *every settled position*. The only thing that ever changes the level is the wiper lifting off during the inter-contact float — which is exactly the float method rejected above. "All contacts to 3.3 V" collapses into the float approach and must not be used.

**Alternating-contact pattern.** Wire the pole-2 contacts in an alternating sequence — odd positions to 3.3 V, even positions to GND — wiper to a GPIO configured to interrupt on **both edges** (no fixed pull needed; the contact drives the line in every settled position):

```
pos:      0    1    2    3    4    5
pole-2:  3V3  GND  3V3  GND  3V3  GND
              wiper ──► GPIO (IRQ on both edges)
```

Landing on any adjacent detent flips the settled level → a clean, debounced-by-mechanics edge. The edge appears on the **new** settled contact, not during the float, so it is independent of shorting / non-shorting switch type and of float duration.

- Clean, reliable, no extra components beyond the wiring pattern.
- Wake signal is independent of shorting/non-shorting switch type and float duration.
- Requires **2P variants** for all rotary switches (the 2P requirement is *not* eliminated — the alternating pattern only fixes *how* the second pole is wired).
- Total: 3 ADC pins + 3 wake GPIOs.

**Known corner case:** an instantaneous jump across two detents of the *same* parity (e.g. 1 → 3, both GND) produces no net level change → no edge → no wake. On a detented switch this requires skipping a detent faster than the wiper can register, which is unlikely in normal use. Single-step moves (the common case) always produce an edge. If observed in practice, fall back to Option B.

Note: this wake pole is wired to GND/3.3 V *directly*, independent of the gated ladder rail, so it stays live during sleep to catch the wake edge. Its only current draw is the brief through-path when the wiper bridges during transition — negligible.

### 3.3 Option B — RC differentiator + comparator (fallback)

A high-pass RC filter (cap in series, resistor to GND) on the ADC wiper line differentiates the DC step into a voltage spike on transition:

```
wiper ──┤C├──┬── to comparator
             R
             │
            GND
```

Spike amplitude ≈ voltage step size (~300 mV for 11-position ladder) — **below the RP2040 GPIO logic threshold (~1.0–1.6 V)**. A comparator with adjustable threshold is required to detect it reliably.

- Works on 1P switches — no 2P requirement.
- Robust to shorting/non-shorting switch type — fires on the voltage step at landing, not on the float.
- Adds a comparator IC (e.g. LM393 dual, one per two switches) plus RC passives.
- Detects transitions only, not settled position — acceptable for wake.
- Mechanical bounce during slow rotation may produce multiple spikes — also acceptable.
- More complex and likely more expensive than Option A.

**Use only if 2P switches are unavailable or exceed BOM budget.**

### 3.4 Option C — shift register (74HC165) with periodic wake

Instead of the ADC ladder, each switch position is wired to one input of a daisy-chained **74HC165** (parallel-in, serial-out shift register). The MCU clocks through all inputs in a tight serial read cycle: 3 pins total (CLK, LATCH, DATA) regardless of input count.

```
switch positions ──► [74HC165] ──┐
                                  ├── DATA → MCU (1 pin)
switch positions ──► [74HC165] ──┘
                    CLK, LATCH ←── MCU (2 pins)
```

- Purely digital — no ADC, no threshold tuning, no resistor ladder.
- Scales freely: add more switches, same 3-pin interface.
- Per-position wiring is simpler: one wire per position to GND, pull-ups on the 165.

**Wake-from-sleep problem:** the shift register is passive — the MCU must be awake to initiate a read cycle, so it cannot detect a position change while sleeping. A separate wake signal is still required (the 2P requirement is not eliminated), or the design must fall back to **periodic wake** (e.g. every 2–5 s to poll), which raises average sleep current and cuts battery life.

**Verdict:** a cleaner digital design, worth considering if ADC noise or ladder calibration becomes a problem in practice. It does not help the wake problem. Combine with Option A (alternating-contact 2P wake), or accept the battery penalty of periodic polling.

---

## 4. IR transmit driver

| Item | Value |
|---|---|
| Emitter | 940 nm IR LED (TSAL6400 / TSAL6200) |
| Driver | NPN transistor (2N2222A or S8050), MCU GPIO → base via resistor |
| Peak current | ~100 mA |
| Series resistor | ~100 Ω (LED current limit) |
| GPIO | pin 4 (IRremoteESP8266 default) |
| Carrier | 38 kHz, generated by the library |

The MCU GPIO cannot source ~100 mA directly, so the LED is driven through an NPN transistor (LED + series resistor on the collector, GPIO → base resistor → base, emitter to GND). The ~100 mA pulse is the largest transient in the system and is the reason for the bulk capacitor in [§5](#5-power). The library handles frame assembly, checksum, and 38 kHz modulation — see [01_IR_protocol_and_mapping.md](01_IR_protocol_and_mapping.md).

---

## 5. Power

- **Battery:** Li-Po single cell (3.7 V nominal, 3.0–4.2 V range).
- **Charging / protection:** TP4056 module with protection IC (DW01A + FS8205), exposed via USB-C port. Handles over-discharge cutoff and over-current.
- **MCU supply:** Li-Po feeds VSYS directly (RP2040 onboard regulator). 1.8–5.5 V input range covers the full Li-Po range.
- **Gated 3.3 V ladder rail:** the resistor ladders are powered from a **switchable** 3.3 V rail — a separate LDO (MCP1700 / HT7333) or load switch enabled by a GPIO via a P-MOSFET high-side switch (e.g. AO3401). The MCU enables it on wake, reads the ladders, then disables it before sleeping. The Pico's onboard 3.3 V cannot be gated, so the ladders must *not* be powered from it. This gating is what removes the ladders' ~90 µA from the sleep budget.
- **Bulk capacitor:** 100 µF on the power rail to absorb the ~100 mA IR LED pulse and prevent MCU brownout reset.
- **Decoupling:** 100 nF ceramic at the MCU supply pins.
- **Battery sizing:** TBD — driven by the 6-month target and the *measured* sleep current. Size the cell only after bench measurement (see below).
- **Low-battery LED:** nice to have — monitor VSYS via a divider into a spare ADC, light an LED below threshold.

---

## 6. Battery budget

The 6-month target is the hardest constraint in the project and it drives the whole power architecture. Sleep current alone does **not** close it — the supporting circuitry matters as much as the MCU.

**Sleep current is not "~1–2 mA" on a stock Pico board.** A bare RP2040 in DORMANT can reach ~180 µA, but the Raspberry Pi Pico *board* carries an always-on RT6150 buck-boost SMPS plus other support circuitry; measured deep-sleep for a stock Pico board is typically **~1.3 mA**. Budget against the board number, not the datasheet die number.

**The resistor ladders draw continuous current if always powered.** An 11-position ladder at 10 kΩ/step is ~110 kΩ end-to-end → ~30 µA at 3.3 V; three ladders ≈ **~90 µA continuous**, and they force the 3.3 V rail to stay enabled during sleep.

Rough budget at 1.3 mA average: a 2000 mAh cell → ~1500 h ≈ **~9 weeks**, well short of 6 months. To close the gap the architecture must:

1. **Gate the ladder supply.** Power the three ladders from the GPIO-switched 3.3 V rail ([§5](#5-power)), enabled only while awake. This is only possible *because* wake comes from the digital pole-2 edge, not from the ladder — the two halves of the design are coupled. Gated off, the ladders' ~90 µA disappears during sleep.
2. **Minimise board-level standby.** Either accept the stock Pico's ~1.3 mA (and size the cell / target accordingly), or move to a bare RP2040/RP2350 + an efficient external LDO and drop the SMPS overhead. This is the single biggest lever on the 6-month target.
3. **Keep active time tiny.** Each wake is sub-second and infrequent, so the average is dominated by sleep current — which is why (1) and (2) matter most.

This is an open hardware-validation item, not a solved problem — the achievable sleep current must be measured on the bench before the cell can be sized and the 6-month claim confirmed.

---

## 7. Sleep / wake sequence

```
sleep (3.3 V ladder rail OFF, MCU dormant)
  │  pole-2 alternating-contact edge  ──or──  button GPIO edge
  ▼
wake
  enable 3.3 V ladder rail
  wait for rail + ladder to settle
  read 3 ADC ladders → map to positions
  debounce: wait for knob to stop moving (settle window),
            re-read until two consecutive reads agree
  if state changed (or RESEND pressed): build frame, send IR, flash TX LED
  disable 3.3 V ladder rail
  ▼
sleep
```

The debounce / settle step also handles two software-solvable issues:

- **Float-time read glitch.** On a non-shorting switch the ladder pin is momentarily open during the inter-detent float; waiting for two consecutive agreeing reads avoids latching an indeterminate value.
- **Knob sweep.** Sweeping a knob across several detents would otherwise fire one full 3-frame transmission per intermediate position. Waiting for the knob to settle (~300–500 ms of stability) before sending collapses a sweep into a single send.

Both are firmware concerns, not hardware blockers.

---

## 8. Open questions

- [ ] Measure stock Pico board sleep current on the bench; decide whether a bare RP2040/RP2350 + external LDO is required to hit 6 months.
- [ ] Verify ADC voltage spacing for the 11-position ladder on bench (3.3 V ref, 12-bit, equal 10 kΩ resistors).
- [ ] Confirm the gated-rail design (LDO + high-side switch vs integrated load switch) and that the wake pole stays live with the ladder rail off.
- [ ] Confirm 2P switch availability so Option A holds; otherwise fall back to Option B (comparator).
