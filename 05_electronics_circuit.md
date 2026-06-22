# Electronics Circuit

The circuit-level design for the remote. Covers how the rotary switches are read and how they wake the MCU, the IR transmit driver, the power architecture, and the decoupling. For **which switch parts** to use see [04_rotary_switch_choice.md](04_rotary_switch_choice.md); for requirements see [00_specifications.md](00_specifications.md); for high-level choices and trade-offs see [01_technical_design_overview.md](01_technical_design_overview.md).

The whole circuit is shaped by one hard constraint: the **6-month battery target**. The MCU sleeps between transmissions, and every always-on current path (resistor ladders, indicator LEDs, board regulator) is treated as a budget item. See [Battery budget](#6-battery-budget).

---

## 1. Block overview

**Chosen readout (§3.6):** all three rotary switches are **1-pole, diode-encoded** onto their own GPIO; buttons are direct. Every input line is both-edge-interrupt-armed for wake. No ADC, no analog ladder, no gated rail.

```
   Li-Po ──► TP4056 ──► VBAT ──► [MCU: nRF52840] ──► GPIO ──► IR LED driver ──► 940 nm LED
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

![RS1010 diode encoding wiring](images/circuit/rs1010_wiring_2.png)

- **Parts:** loose small-signal diodes — **1N4148** (THT) or **BAV99/BAV70** (SOT-23, 2 per package). There is *no* dedicated "diode encoder" product (searching "diode array" returns photodiode/ESD parts); a coded switch [§3.4](#34-option-c--off-the-shelf-coded-rotary-switch-considered) *is* the integrated version of exactly this. So it's hand-wired loose diodes — which lays out cleanly on perfboard: one rail per code line, diodes from contacts to rails.
- **Position** = read each switch's GPIO group as a small nibble. Works on **any 1P switch** — Alpha SR1610 1P, Lorlin CK1032 1P, Grayhill 56 1P12T — chosen on body size and detent feel.
- **No shared bus.** Each switch on its own lines avoids the superposition problem — three switches sharing one bus would OR their codes together and become unreadable without scanning, and scanning needs the MCU awake (no help for wake). Independent lines keep both readout and wake trivial. (A shared scanned matrix only pays off at many more switches than three.)

#### Wake — both-edge interrupts on every line

The MCU sleeps in its **deepest viable mode** (nRF52840 System ON with RAM retained, ~1.5–2.4 µA; STM32L4 STOP2; AVR power-down). Before sleeping, arm **every input line — all rotary code lines and all button lines — for wake on both edges**, then sleep. Any knob move or button press toggles ≥1 line → edge → wake. The ISR doesn't decode anything: it only needs to know *something changed*, then the awake MCU reads all switches, sends, and sleeps again.

- **Any move always produces an edge.** A single-step rotary move flips ≥1 code bit; the inter-detent float (all that switch's lines briefly 0) only *adds* edges, so even same-direction code changes are covered. A button is a clean 1-line edge.
- **Why not OR all lines to one wake pin:** OR gives a *level*, and a move between two non-zero codes can leave it unchanged → missed wake. Per-line edges avoid this. (Same trap as the rejected "all contacts to one level" wake — in digital form.)
- **No extra hardware.** The per-pin change detection *is* a per-bit change detector, for free. (A hardware latch+XOR change-detector — 74HC175 + 74HC86 → one wake pin — would also work but adds 2 ICs and quiescent sleep current against the 6-month budget; kept only as a fallback if per-line wake misbehaves on the bench.)
- **"Both edges" is usually emulated in deep sleep, and that's fine.** On the preferred parts the deepest mode senses a *level*, not a literal edge (nRF52 SENSE per-pin level + LATCH; AVR INT level), so the firmware arms each line for the opposite of its current level and re-arms on wake. STM32L4 STOP2 is the exception — true hardware both-edge EXTI on any pin. The re-arm step is a few instructions in the already-running wake handler, so it costs nothing measurable. (ATmega PCINT is also genuine hardware both-edge.)

**MCU caveat — validate deep-sleep multi-pin wake.** All three candidate parts can wake on ≥13 GPIO from a µA-class mode (nRF52840 all ~48 pins via SENSE/LATCH; STM32L4 any pin via STOP2 EXTI; ATmega328P all pins via PCINT). This is the **first** bench test (§8) — measure sleep current *and* confirm every line wakes on both edges, including non-zero→non-zero code changes. If the preferred board disappoints, fall back down the order (nRF52840 → STM32L4 → ATmega328P).

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

≈ **13 input GPIO**, all both-edge-interrupt-armed for wake, well within any candidate board's pin count (nRF52840 XIAO exposes ~11 on the castellations but the module has many more; STM32L4 and ATmega328P have ample). No ADC, no ladder rail, no comparator, no 2P switches. See [§3.5](#35-the-chosen-scheme--per-switch-diode-encoding) for the wiring and [04_rotary_switch_choice.md](04_rotary_switch_choice.md) for part selection (all three are now 1P, chosen on feel).

---

## 4. IR transmit driver

| Item | Value |
|---|---|
| Emitter | 940 nm IR LED (TSAL6400 / TSAL6200) |
| Driver | NPN transistor (2N2222A or S8050), MCU GPIO → base via resistor |
| Peak current | ~100 mA |
| Series resistor | ~100 Ω (LED current limit) |
| GPIO | one carrier-capable output (nRF52840 PWM pin / STM32 timer pin / AVR OCx) |
| Carrier | 38 kHz, generated by the MCU's PWM/timer peripheral |

The MCU GPIO cannot source ~100 mA directly, so the LED is driven through an NPN transistor (LED + series resistor on the collector, GPIO → base resistor → base, emitter to GND). The ~100 mA pulse is the largest transient in the system and is the reason for the bulk capacitor in [§5](#5-power). The 38 kHz carrier comes from the MCU's PWM/timer (on nRF52840, the PWM peripheral can clock the whole modulated frame out of a RAM buffer); frame assembly + checksum are firmware, ported from the documented Daikin format — see [01_IR_protocol_and_mapping.md](01_IR_protocol_and_mapping.md).

---

## 5. Power

- **Battery:** Li-Po single cell (3.7 V nominal, 3.0–4.2 V range).
- **Charging / protection:** TP4056 module with protection IC (DW01A + FS8205), exposed via USB-C port. Handles over-discharge cutoff and over-current.
- **MCU supply:** Li-Po feeds the board's battery input (nRF52840 XIAO has an onboard LDO + BATT pad; STM32L4 / AVR boards via their LDO). The nRF52840 core runs 1.7–3.6 V, covering the full Li-Po range.
- **Switch-encoding current path:** a diode-encoded switch with its wiper at +3.3 V draws continuous current through the closed contact → diodes → pull-down resistors. **The wiper must stay powered during sleep** — that's how the settled code is held on the lines for the wake interrupt to see an edge. So you *cannot* gate the wiper supply (gating it zeroes all lines and kills wake — the same coupling trap as elsewhere in this design). Instead keep the leakage small with **weak pull-downs**. **Now that the board floor is µA-class (≈1.5–2.4 µA on the nRF52840), this leakage is no longer "negligible by comparison" — it can dominate, so it must be designed down, not hand-waved.** At 100 kΩ a high line leaks ~33 µA — *far* too much here; at 1 MΩ it is ~3.3 µA/line, and only the lines that happen to be HIGH in the current code draw at all (~1–2 high lines per switch typically). So use **high-value pull-downs (1 MΩ, the GPIO inputs are high-impedance so this is fine)** to bring total switch leakage to a few µA across all three. **No gating IC, no analog rail.** Bench-measure leakage vs. read reliability to fix the value — it is now a first-order term in the battery budget, not a rounding error.
- **Bulk capacitor:** 100 µF on the power rail to absorb the ~100 mA IR LED pulse and prevent MCU brownout reset.
- **Decoupling:** 100 nF ceramic at the MCU supply pins.
- **Battery sizing:** TBD — driven by the 6-month target and the *measured* sleep current. Size the cell only after bench measurement (see below).
- **Low-battery LED:** nice to have — monitor battery voltage via a divider into a spare ADC, light an LED below threshold. (Use a high-value divider, gated or weak, so it doesn't itself become a µA-budget item.)

---

## 6. Battery budget

The 6-month target (goal: years) is the hardest constraint, and the MCU choice ([03](03_microcontroller_choice.md)) is what makes it reachable. Because the device sleeps ~100 % of the time, **sleep current ≈ average current ≈ the whole budget**, so the budget is built from two µA-class terms, not one mA-class one.

**What the target demands.** On a 2000 mAh cell:

| Avg current | Life |
|---|---|
| ~460 µA | 6 months |
| ~230 µA | 1 year |
| ~115 µA | 2 years |

So the design point is **tens of µA**, and the chosen board must sleep there — which is exactly why the RP2040 (≈1–1.8 mA board → ~9 weeks) was dropped and the nRF52840 XIAO (≈1.5–2.4 µA) chosen; see [03](03_microcontroller_choice.md). Budget against the **board** figure, measured on the bench, not the die datasheet.

**Two terms, now the same order of magnitude:**

1. **Board standby** — nRF52840 XIAO ≈ **1.5–2.4 µA** (System ON, RAM retained). The single biggest term historically; now genuinely small.
2. **Switch-encoding leakage** — wiper at 3.3 V through closed contacts → diodes → pull-downs (see [§5](#5-power)). With **1 MΩ pull-downs**, only the HIGH lines of the current code draw, at ~3.3 µA each → a few µA across all three switches. **At a µA-class board floor this is no longer negligible — it can equal or exceed board standby**, so the pull-down value is a real budget knob (and it cannot be gated, or wake dies). This is the term to bench-tune.

Active time is a non-term: each wake is sub-second and infrequent, so even at tens of mA active the duty-cycle contribution is well under a µA averaged. The average is set entirely by (1) + (2).

**Rough budget at, say, 6 µA total (board + leakage):** a 2000 mAh cell → ~330 000 h, i.e. **multi-year** — comfortably past 6 months and into the "years" goal, with margin for a smaller cell to suit the 25 mm enclosure depth. Even a pessimistic 20 µA (loose pull-downs, board power-path leakage) still clears a year on 2000 mAh.

This is still an open **hardware-validation** item: the achievable board sleep *and* the switch leakage must both be measured before the cell is sized and the claim confirmed. The leverage has simply moved — from "can the MCU even get low enough" (yes, by choosing the right one) to "keep the switch leakage in check" (pull-down value) and "verify the board's battery path doesn't leak" (e.g. the XIAO VBAT note in [03](03_microcontroller_choice.md)).

---

## 7. Sleep / wake sequence

```
sleep (MCU deepest viable mode — nRF52 System ON / STM32 STOP2 / AVR power-down;
       all code + button lines armed for both-edge wake (level-sense + re-arm where needed);
       switch wipers held at 3.3 V via weak pull-downs)
  │  any rotary code-line edge  ──or──  button GPIO edge
  ▼
wake
  read the three switch code groups (Fan 3b, Mode 2b, Temp 4b) → map to positions
  read button states
  debounce: wait for knobs to stop moving (settle window),
            re-read until two consecutive reads agree
  if state changed (or RESEND pressed): build frame, send IR, flash TX LED
  re-arm wake (set each line's sense to the opposite of its now-settled level)
  ▼
sleep
```

The debounce / settle step also handles two software-solvable issues:

- **Float-time read glitch.** On a non-shorting switch all of that switch's code lines momentarily read 0 during the inter-detent float; waiting for two consecutive agreeing reads avoids latching the false "position 0".
- **Knob sweep.** Sweeping a knob across several detents would otherwise fire one full 3-frame transmission per intermediate position. Waiting for the knob to settle (~300–500 ms of stability) before sending collapses a sweep into a single send.

Both are firmware concerns, not hardware blockers.

---

## 8. Open questions

Readout is decided (Approach D, all three switches); MCU is preferred (nRF52840 XIAO) but bench-gated. Remaining items, in priority order:

- [ ] **Sleep current + 13-pin wake on the chosen board (the gating test).** On the nRF52840 XIAO: deep-sleep (System ON, RAM retained), arm ~13 GPIO for both-edge wake (per-pin SENSE level + re-arm), measure board sleep current with a µA meter, and confirm every line wakes on either edge — including non-zero→non-zero code changes. Also confirm the board's battery power path doesn't leak (XIAO VBAT note, [03](03_microcontroller_choice.md)). If it disappoints, fall back nRF52840 → STM32L4 (STOP2) → ATmega328P. This decides the MCU and sizes the cell.
- [ ] **XIAO pin count.** The XIAO nRF52840 breaks out ~11 GPIO on its castellations; this design needs ~13 + IR + TX LED. Either use a board/module that exposes more nRF52840 pins, reduce the line count (e.g. 10-position temp switch → 4 bits unchanged; fold RESEND into a long-press), or solder to extra module pads. Confirm a workable pin map before committing the XIAO specifically.
- [ ] **Daikin IR transmit.** Port the 35-byte frame + checksum, clock it out at 38 kHz via the MCU PWM/timer through the driver, confirm the FTXM20N2V1B responds. Portable across all three candidate MCUs, so it follows the power test.
- [ ] **Diode-encode + wake bench test.** Wire one 1P12T (Temp) with its ~19 diodes; verify the nibble reads correctly and that both-edge wake fires on every move, including non-zero→non-zero (the float forces an edge — confirm it).
- [ ] **Pull-down value.** Now a first-order budget term, not a rounding error. Measure leakage vs. read reliability to pick the value (lean toward 1 MΩ) that keeps switch current to a few µA without flaky reads. See [§6](#6-battery-budget).
- [ ] **Switch sourcing on feel.** Pick the three 1P switches on detent quality and body size (no pole constraint now); confirm shaft/knob fit. See [04_rotary_switch_choice.md](04_rotary_switch_choice.md).
