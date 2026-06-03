# Electronics Circuit

The circuit-level design for the remote. Covers how the rotary switches are read and how they wake the MCU, the IR transmit driver, the power architecture, and the decoupling. For **which switch parts** to use see [04_rotary_switch_choice.md](04_rotary_switch_choice.md); for requirements see [00_specifications.md](00_specifications.md); for high-level choices and trade-offs see [01_technical_design_overview.md](01_technical_design_overview.md).

The whole circuit is shaped by one hard constraint: the **6-month battery target**. The MCU sleeps between transmissions, and every always-on current path (resistor ladders, indicator LEDs, board regulator) is treated as a budget item. See [Battery budget](#6-battery-budget).

---

## 1. Block overview

**Chosen readout (§3.6):** all three rotary switches are **1-pole, diode-encoded** onto their own GPIO; buttons are direct. Every input line is both-edge-interrupt-armed for wake. No ADC, no analog ladder, no gated rail.

```
   Li-Po ──► TP4056 ──► VSYS ──► [MCU: RP2040] ──► GPIO ──► IR LED driver ──► 940 nm LED
   (3.0–4.2 V) (charge/protect)      │
                                     ├── 3 GPIO ◄── Fan code (b2..b0)   ┐
                                     ├── 2 GPIO ◄── Mode code (b1..b0)  ├ diode-encoded 1P
                                     ├── 4 GPIO ◄── Temp code (b3..b0)  ┘ (all both-edge IRQ = wake)
                                     ├── 4 GPIO ◄── buttons (PWR/ECO/SWING/RESEND, pull-up, IRQ)
                                     └── GPIO ──► TX indicator LED (direct)
```

Pin budget: 3 + 2 + 4 (rotary) + 4 buttons + 1 IR + 1 TX = **15 pins** of 26. The rotary code lines double as the wake interrupts, so no separate wake pins. Margin is comfortable for the optional low-battery sense or extra buttons.

---

## 2. Reading a rotary switch — the two coupled problems

Each selector must answer two things, and they are easy to conflate:

1. **Absolute position** — which detent is selected, readable on demand and after the MCU has been asleep (the stateless principle).
2. **Wake** — a GPIO edge when the knob moves, because the MCU sleeps and a plain switch is not an interrupt source.

A switch landing on a new contact is a *DC level/voltage change*, not an edge, so wake never comes for free from a naive readout — it must be engineered. §3 records four approaches considered; the chosen one (**per-switch diode encoding**, Approach D) reads position as a small binary code on a few GPIO and takes wake from those same lines via both-edge interrupts, on plain **1-pole** switches.

The buttons, by contrast, are trivial: each goes to a GPIO with internal pull-up and is its own wake/interrupt source.

---

## 3. Readout + wake — design options

**Decision up front:** the chosen scheme is **per-switch diode encoding (Approach D)** for all three rotary switches — jump to [§3.5](#35-the-chosen-scheme--per-switch-diode-encoding) for the design and [§3.6](#36-decision--approach-d-for-all-three-rotary-switches) for why. The other approaches (A–C) are recorded below as the alternatives considered.

Four approaches, A–D. A and B encode position as an **analog voltage** (resistor ladder, 1 ADC pin); C and D encode it **digitally** (binary code, several GPIO). The digital ones get wake from the same lines that read the code and **drop the 2-pole requirement**. §3.1 first explains why the obvious "wake on the float" idea is rejected, since that reasoning underlies all four.

### 3.1 Do not rely on the inter-contact float

An obvious approach is to detect the wiper floating between contacts — when the wiper lifts off a contact, a pull-up drives the GPIO high, producing a rising edge. **This is fragile** and rejected:

- **Non-shorting vs shorting switches.** Non-shorting (break-before-make) switches have a guaranteed float period; shorting (make-before-break) switches may have *no* float at all — the wake pulse may never occur.
- **Float duration is unspecified.** Even on non-shorting switches, only the *order* of events is guaranteed, not the duration. A fast detent snap can float for only a few microseconds — too short to reliably trigger a GPIO interrupt given MCU wake latency.

**Conclusion:** detect the *landed* state, not the float — via a settled second-pole level (A), an HPF spike (B), or settled digital code lines (C/D, the chosen scheme). All four read what the switch *settled on*, never the transient float.

### 3.2 Option A — resistor ladder + 2-pole alternating-contact wake *(considered)*

**Readout (pole 1):** the position contacts are wired as a resistor ladder on a single ADC pin — equal resistors in series between each tap, common wiper to the ADC, chain ends to the gated 3.3 V rail and GND.

- Mode (4) → 1 ADC · Fan (6) → 1 ADC · Temperature (11) → 1 ADC. **3 ADC pins** total; RP2040 ADC0–ADC2 are all usable.
- 10 kΩ per step is a safe value: ~300 mV spacing per step on the 11-position ladder (3.3 V ref, 12-bit → ~0.8 mV/LSB) — comfortable above noise.
- **Power the ladder from the gated 3.3 V rail, not VSYS.** A varying supply shifts every reading; gating the rail is also what lets the ladder draw zero current in sleep (see [§5](#5-power), [§6](#6-battery-budget)).

**Wake (pole 2):** the **second pole** generates the wake edge.

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
- Requires a **2P switch** for each selector using this approach (the alternating pattern fixes *how* the second pole is wired, but a second pole is still needed). The digital options C/D avoid this.
- Pins: 1 ADC + 1 wake GPIO per ladder selector.

**Known corner case:** an instantaneous jump across two detents of the *same* parity (e.g. 1 → 3, both GND) produces no net level change → no edge → no wake. On a detented switch this requires skipping a detent faster than the wiper can register, which is unlikely in normal use. Single-step moves (the common case) always produce an edge. If observed in practice, fall back to Option B.

Note: this wake pole is wired to GND/3.3 V *directly*, independent of the gated ladder rail, so it stays live during sleep to catch the wake edge. Its only current draw is the brief through-path when the wiper bridges during transition — negligible.

**The catch — sourcing.** This needs a **2P switch with ≥ the position count**. For Fan (6) and Mode (4) that's easy and cheap. For **temperature (≥10), 2P parts are scarce, bulky, and pricier** (Alpha SR1610 2P12T — confirm stock; Lorlin CK1032 2P — 27.5 mm, bulky). If that constraint bites, the digital options (C/D) drop the 2P requirement entirely. See [04_rotary_switch_choice.md](04_rotary_switch_choice.md).

### 3.3 Option B — RC differentiator + comparator *(considered, rejected)*

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
- More complex than Option A, and the digital options (C/D) also free the switch to 1P without analog tuning.

**Mostly superseded** by the digital options for the 1P case: B keeps the analog ladder (so it still needs a gated rail and ADC calibration) and adds a comparator to boot. Prefer D unless you specifically want to keep an analog readout.

### 3.4 Option C — off-the-shelf coded rotary switch *(considered)*

A **coded rotary switch** outputs its position directly as a binary code (BCD for 10 positions, hex for 16) on 4 lines plus a common — 5 connections regardless of position count. It is electrically the cleanest answer: **absolute digital position, no ADC, no calibration.**

```
common ──┘ (to +3.3 V)
position ──► 4 code lines  b3 b2 b1 b0  ──► 4 GPIO (pull-down, both-edge IRQ)
```

- **Position** = read the 4 GPIOs as a nibble, map to °C. (Use **real code**, not complementary, so the mapping is trivial.)
- **Wake** = any move flips ≥1 code bit. Put the 4 code lines on **both-edge GPIO interrupts** — at least one always sees an edge on any position change. See [§3.5 wake](#wake--both-edge-interrupts-on-every-line) for why a single OR'd wake pin is *not* enough.
- **No 2P requirement, no comparator.** The encoding is built into the switch.
- 16-position parts (e.g. Nidec SH-7000, ALPS EC05 — on LCSC) bridge to 11 with an end-stop, or map 11–15 to "26 °C".

**The catch — feel.** These are *configuration* switches (DIP-adjacent), aimed at address/setpoint setting: tiny 7×7 mm bodies, often a **screwdriver slot or stub shaft**, ~16 fine detents. That is the wrong tactile quality for a deliberate, hand-turned interface. Usable **only if** a knob-friendly shafted variant exists at the right size and price — otherwise the encoding idea is better realised by wiring it yourself (Option D).

### 3.5 The chosen scheme — per-switch diode encoding

A coded switch is just a 1P-N-position switch wired in a binary pattern. So each knob is a **big, satisfying 1P switch** chosen on feel and size, with the encoding provided by a handful of diodes on its own pins. Each switch is **independent** — its own code lines, its own GPIO — so the firmware reads three separate small codes and combines them; nothing is shared, nothing is scanned.

**Per-switch diode encoding.** Put +3.3 V on the wiper. From each position contact, run a diode to each code line that should be HIGH for that position; pull-downs on the code lines. Example for the temperature switch (4 bits):

```
wiper = +3.3 V (gated)
 pos 0 (0000)                                  b3 b2 b1 b0
 pos 1 (0001)  ──►|──────────────────────────────────► b0
 pos 2 (0010)  ──►|─────────────────────────► b1
 pos 3 (0011)  ──►|───────────────────────────────►b1,b0
 ...                                            │  │  │  │
 pos10 (1010)  ──►|──►b3 ──►|──►b1              ▼  ▼  ▼  ▼
                                       pull-downs → 4 GPIO (both-edge IRQ)
```

Same pattern for Fan (3 lines) and Mode (2 lines) on their own GPIO. Diodes prevent bus-tied contacts from shorting positions together; count = total set-bits across the used codes (~19 for Temp codes 0–10, fewer for the smaller switches).

- **Parts:** loose small-signal diodes — **1N4148** (THT) or **BAV99/BAV70** (SOT-23, 2 per package). There is *no* dedicated "diode encoder" product (searching "diode array" returns photodiode/ESD parts); a coded switch [§3.4](#34-option-c--off-the-shelf-coded-rotary-switch-considered) *is* the integrated version of exactly this. So it's hand-wired loose diodes — which lays out cleanly on perfboard: one rail per code line, diodes from contacts to rails.
- **Position** = read each switch's GPIO group as a small nibble. Works on **any 1P switch** — Alpha SR1610 1P, Lorlin CK1032 1P, Grayhill 56 1P12T — chosen on body size and detent feel.
- **No shared bus.** Each switch on its own lines avoids the superposition problem — three switches sharing one bus would OR their codes together and become unreadable without scanning, and scanning needs the MCU awake (no help for wake). Independent lines keep both readout and wake trivial. (A shared scanned matrix only pays off at many more switches than three.)

#### Wake — both-edge interrupts on every line

The MCU sleeps in **DORMANT** (lowest power). Before sleeping, arm **every input line — all rotary code lines and all button lines — for both-edge interrupts**, then go dormant. Any knob move or button press toggles ≥1 line → edge → wake. The ISR doesn't decode anything: it only needs to know *something changed*, then the awake MCU reads all switches, sends, and sleeps again.

- **Any move always produces an edge.** A single-step rotary move flips ≥1 code bit; the inter-detent float (all that switch's lines briefly 0) only *adds* edges, so even same-direction code changes are covered. A button is a clean 1-line edge.
- **Why not OR all lines to one wake pin:** OR gives a *level*, and a move between two non-zero codes can leave it unchanged → missed wake. Per-line edges avoid this. (Same trap as the rejected "all contacts to one level" wake — in digital form.)
- **No extra hardware.** The per-pin both-edge interrupt *is* a per-bit change detector, for free. (A hardware latch+XOR change-detector — 74HC175 + 74HC86 → one wake pin — would also work but adds 2 ICs and quiescent sleep current against the 6-month budget; kept only as a fallback if per-line wake misbehaves on the bench.)

**RP2040 caveat — validate the DORMANT path.** Every RP2040 GPIO supports interrupts and can wake the chip from DORMANT (via `dormant_wake`), with no "only certain pins" restriction. **But** that path lives in the pico-sdk; reaching it cleanly from the **Arduino-Pico core** (which the IR library needs) is fiddly and must be proven on the bench — same validation bucket as the IRremoteESP8266 check. If DORMANT-from-Arduino proves painful, it bears on the MCU choice. See [§8](#8-open-questions).

**Float glitch (readout).** All of a non-shorting switch's code lines momentarily read 0 during the inter-detent float (a false "position 0"). It *helps* wake but must not be sampled as a position — handled by the debounce: wait for two consecutive agreeing reads (see [§7](#7-sleep--wake-sequence)).

### 3.6 Decision — Approach D for all three rotary switches

**Chosen: per-switch independent diode encoding (Approach D)** for Fan, Mode, and Temperature alike. Each rotary switch is a plain **1-pole** part, chosen on body size and detent feel, with its contacts diode-encoded onto its own small set of GPIO. Buttons stay direct (one GPIO each). Everything is read statically and woken by both-edge interrupts.

Why D over the others:

- **A (ladder + 2P)** — works, but needs 2P switches (the temperature sourcing pain), a gated analog rail, ADC calibration, and continuous ladder current. Rejected once D removed the 2P constraint.
- **B (ladder + comparator)** — frees the switch to 1P but keeps the analog ladder *and* adds a comparator. Strictly worse than D.
- **C (off-the-shelf coded switch)** — electrically identical to D but the compact coded parts feel wrong (screwdriver-slot, tiny). D is C built on a big tactile knob.

**Why uniform (all three D), not mixed:** one readout pattern across all knobs means one firmware path, one wiring idiom on the perfboard, no analog rail at all. The pin cost is modest:

| Selector | Positions | Bits | GPIO |
|---|---|---|---|
| Fan / Power | 6 | 3 | 3 |
| Mode | 4 | 2 | 2 |
| Temperature | 11 | 4 | 4 |
| **Rotary total** | | | **9** |
| Buttons (Powerful / Econo / Swing / Resend) | — | — | 4 (direct) |

≈ **13 input GPIO**, all both-edge-interrupt-armed for wake, well within the RP2040's 26. No ADC, no ladder rail, no comparator, no 2P switches. See [§3.5](#35-the-chosen-scheme--per-switch-diode-encoding) for the wiring and [04_rotary_switch_choice.md](04_rotary_switch_choice.md) for part selection (all three are now 1P, chosen on feel).

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
- **Switch-encoding current path:** a diode-encoded switch with its wiper at +3.3 V draws continuous current through the closed contact → diodes → pull-down resistors (~33 µA per high line at 100 kΩ; a few tens of µA per switch). **The wiper must stay powered during sleep** — that's how the settled code is held on the lines for the wake interrupt to see an edge. So you *cannot* gate the wiper supply (gating it zeroes all lines and kills wake — the same coupling trap as elsewhere in this design). Instead keep the leakage small with **weak pull-downs** (220 kΩ–1 MΩ; the GPIO inputs are high-impedance, so weak is fine), bringing it to single-digit µA per switch — a handful of µA total across three switches, negligible against the ~1.3 mA board floor. **No gating IC, no analog rail.** Bench-measure to confirm the chosen pull-down value.
- **Bulk capacitor:** 100 µF on the power rail to absorb the ~100 mA IR LED pulse and prevent MCU brownout reset.
- **Decoupling:** 100 nF ceramic at the MCU supply pins.
- **Battery sizing:** TBD — driven by the 6-month target and the *measured* sleep current. Size the cell only after bench measurement (see below).
- **Low-battery LED:** nice to have — monitor VSYS via a divider into a spare ADC, light an LED below threshold.

---

## 6. Battery budget

The 6-month target is the hardest constraint in the project and it drives the whole power architecture. Sleep current alone does **not** close it — the supporting circuitry matters as much as the MCU.

**Sleep current is not "~1–2 mA" on a stock Pico board.** A bare RP2040 in DORMANT can reach ~180 µA, but the Raspberry Pi Pico *board* carries an always-on RT6150 buck-boost SMPS plus other support circuitry; measured deep-sleep for a stock Pico board is typically **~1.3 mA**. Budget against the board number, not the datasheet die number.

**The switch-encoding lines draw a little continuous current** (wiper held at 3.3 V through closed contacts to pull-downs). With **weak pull-downs** this is single-digit µA per switch — a handful of µA total, negligible against the board floor. The wiper can't be gated off (that would kill wake — see [§5](#5-power)), so weak pull-downs, not gating, are the lever here. This is far smaller than the ~90 µA the rejected resistor-ladder approach would have cost, and needs no gating IC.

Rough budget at 1.3 mA average: a 2000 mAh cell → ~1500 h ≈ **~9 weeks**, well short of 6 months. The dominant term by far is **board-level standby**, not the switches. To close the gap:

1. **Minimise board-level standby.** Either accept the stock Pico's ~1.3 mA (and size the cell / target accordingly), or move to a bare RP2040/RP2350 + an efficient external LDO and drop the SMPS overhead. **This is the single biggest lever** on the 6-month target — everything else (switch leakage, active time) is small by comparison.
2. **Keep the switch leakage small** with weak pull-downs (above) — a few µA, easily done.
3. **Keep active time tiny.** Each wake is sub-second and infrequent, so the average is dominated by sleep current — which is why (1) matters most.

This is an open hardware-validation item, not a solved problem — the achievable sleep current must be measured on the bench before the cell can be sized and the 6-month claim confirmed.

---

## 7. Sleep / wake sequence

```
sleep (MCU DORMANT; all code + button lines armed for both-edge IRQ;
       switch wipers held at 3.3 V via weak pull-downs)
  │  any rotary code-line edge  ──or──  button GPIO edge
  ▼
wake
  read the three switch code groups (Fan 3b, Mode 2b, Temp 4b) → map to positions
  read button states
  debounce: wait for knobs to stop moving (settle window),
            re-read until two consecutive reads agree
  if state changed (or RESEND pressed): build frame, send IR, flash TX LED
  re-arm both-edge IRQs
  ▼
sleep
```

The debounce / settle step also handles two software-solvable issues:

- **Float-time read glitch.** On a non-shorting switch all of that switch's code lines momentarily read 0 during the inter-detent float; waiting for two consecutive agreeing reads avoids latching the false "position 0".
- **Knob sweep.** Sweeping a knob across several detents would otherwise fire one full 3-frame transmission per intermediate position. Waiting for the knob to settle (~300–500 ms of stability) before sending collapses a sweep into a single send.

Both are firmware concerns, not hardware blockers.

---

## 8. Open questions

Readout is decided (Approach D, all three switches). Remaining items:

- [ ] **DORMANT wake from the Arduino-Pico core.** Confirm the pico-sdk `dormant_wake` path is reachable and reliable from the Arduino core (which IRremoteESP8266 needs), with multiple GPIO armed for both-edge wake. If painful, it bears on the MCU choice. Same bench session as the IR-library check.
- [ ] **Diode-encode + wake bench test.** Wire one 1P12T (Temp) with its ~19 diodes; verify the nibble reads correctly and that a both-edge interrupt on the code lines wakes on every move, including non-zero→non-zero (the float forces an edge — confirm it).
- [ ] **Pull-down value.** Measure leakage vs. noise immunity to pick the weak pull-down (220 kΩ–1 MΩ) that keeps switch current to a few µA without flaky reads.
- [ ] **Sleep current.** Measure the chosen board in DORMANT; decide stock Pico vs bare RP2040/RP2350 + LDO to hit 6 months. Dominant term — see [§6](#6-battery-budget).
- [ ] **Switch sourcing on feel.** Pick the three 1P switches on detent quality and body size (no pole constraint now); confirm shaft/knob fit. See [04_rotary_switch_choice.md](04_rotary_switch_choice.md).
