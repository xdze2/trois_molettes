# Electronics Circuit — Input Wiring

Rotary switch selection and how the switches and buttons are wired to the
ATmega328P. For power and battery see [07_battery_and_power.md](07_battery_and_power.md);
for the IR transmit driver see [06_IR_LED_wiring.md](06_IR_LED_wiring.md).

---

## 1. Switch selection

Three rotary selectors needed:

| Selector | Positions | Switch |
|---|---|---|
| Fan speed | 8 (Off / 1–5 / Quiet / Auto) | Alpha SR16, 1P8T |
| Mode | 5 (Fan / Cool / Heat / Dry / Auto) | RS1010, 1P |
| Temperature | 8 (1 °C steps, mode-dependent range) | Alpha SR16, 1P8T |

All are plain 1-pole (1P) — read by per-switch diode encoding, see §3 below.
Switches are chosen on body size and detent feel; pole count is not a constraint.

### Shorting vs. non-shorting contacts

**v1: treated as non-shorting (the common variant); firmware debounce covers the
transient.** Rotary switches come in two contact styles:

- **Non-shorting (break-before-make):** the wiper fully leaves the old contact
  before touching the next. Between detents *all* code lines float to the
  pull-down level (all-zero), so a mid-turn read can momentarily see `000` and,
  with natural-binary encoding, any bit that switches late gives a bogus code.
- **Shorting (make-before-break):** the wiper bridges the old and new contacts
  through the transition, so the code lines OR the two positions together rather
  than dropping to zero. No all-open float, but the intermediate read is the
  *bitwise-OR* of the two codes — still not a valid position under natural binary.

Either way, natural-binary encoding produces transient false codes mid-turn, which
is why the firmware debounces (§6). The Alpha SR16 and RS1010 are treated as
non-shorting here; the §6 wake note relies on the all-lines-float edge, which only a
non-shorting switch guarantees. If a shorting variant is substituted, wake still
works (OR'd codes still change ≥1 line on a move) but the extra float edges
disappear.

**Gray code does *not* fix the non-shorting case.** It is tempting to think a
Gray-coded switch removes the transient regardless of contact style — it does not.
Gray code only helps two of the three transient sources:

- **Contact skew** (bits switching at slightly different instants): fixed — a
  single-step Gray move flips only one line, so there is no skew to observe.
- **Shorting bridge** (make-before-make): fixed — the intermediate read is the
  bitwise-OR of two adjacent Gray codes, which for a single-bit-apart pair is just
  one of the two valid neighbours, never a spurious third value.
- **Non-shorting float** (break-before-make): **not fixed.** When the wiper leaves
  contact, *all* code lines fall to the pull-down level → `000`, no matter how the
  detents are encoded. The all-open gap is a mechanical event, not an encoding
  artifact. Worse, `000` is a *valid position code* (Off — see §3 Fan table), so
  every non-shorting transition momentarily reads "Off." Only time-stability
  filtering (§6) rejects it.

> **v2 decision.** Confirm the part's contact style **before ordering** for the PCB
> build. For a **shorting** switch, Gray code (§3) removes the transient and lets the
> settle loop shrink to a one-shot read. For a **non-shorting** switch (the v1
> assumption), the float-to-`000` transient survives any encoding, so the real fork
> is *strobe line vs debounce timing* — see the A/C options in §3.

### Fan speed — Alpha SR16 1P8T

![Alpha SR16](../images/rotary_switch/sr16_web.png)

8 positions: Off / 1 / 2 / 3 / 4 / 5 / Quiet / Auto.

16 mm body, PCB through-hole, 6 mm D-shaft, 30° indexing. Widely stocked
(Tayda, Mouser). Good detent feel, common in DIY audio/synth.

### Mode — RS1010 1P

![RS1010](../images/rotary_switch/rs1010_web.png)

5 positions: Fan / Cool / Heat / Dry / Auto.

Compact PCB through-hole, M7×0.75 bushing (~12.5 mm), 6 mm shaft, 30° indexing.
The RS1010 goes up to 6 positions in 1P — 5 fits cleanly.

### Temperature — Alpha SR16 1P8T

Same part as Fan speed. 8 positions, 1 °C steps. Firmware applies a
mode-dependent offset to map the 8 positions to the correct temperature range
(see [00_specifications.md §4.3](00_specifications.md)).

### Alternative considered — two RS1010 for temperature

Two RS1010 switches (5×5 = 25 positions) would give 1 °C resolution across a
*wider* range (the single SR16 covers 8 °C per mode at 1 °C steps). Rejected:
doubles the switch count, complicates panel layout, and an 8-position 1 °C-step
range covers the practical daily-use band adequately for this application.

### Other switches evaluated

**Grayhill 56** — same compact footprint as RS1010, up to 12 positions 1P.
Good alternative for Mode or Temperature if RS1010 stock is short.

**Alpha SR1712F** — 17 mm body, up to 8 positions. Similar feel to SR16,
slightly larger. Usable substitute.

**Alpha SR2510 / SR10010F** — 25 mm body, up to 12 positions. Overkill and bulky.

**Lorlin CK1060 / CK1032** — 27.5 mm body. Very tactile, cheap, proven, but too
large for the 80×100 mm panel with three knobs.

**NKK MR-K112** — ultra-compact, up to 12 positions. Premium / industrial pricing,
overkill for this application.

**8404-3C (on hand)** — 3P4T. Only 4 positions; one short for Mode (5 needed).

### Open questions

- [ ] Confirm SR16 1P8T stock and price on Tayda / Mouser.
- [ ] Confirm three knobs (two SR16 + one RS1010) fit the 80×100 mm panel face with the Send button (and optional swing toggle).
- [ ] Confirm 6 mm D-shaft on both SR16 and RS1010 accepts the same knob cap.

---

## 2. Overview

Three 1-pole rotary switches, diode-encoded onto independent GPIO groups.
One push button (Resend) and one optional toggle (Swing), direct to GPIO.
All lines are both-edge interrupt sources for deep-sleep wake.

```
ATmega328P
  PB2 PB3 PB4  ◄──  Fan speed  (SR16, 8 pos, 3 code lines)
  PC0 PC1 PC2  ◄──  Mode       (RS1010, 5 pos, 3 code lines)
  PD4 PD5 PD6  ◄──  Temp       (SR16, 8 pos, 3 code lines)
  PD2          ◄──  Resend button      (ext. pull-down, active-high — bench
                                         wiring; see §5)
  PD7          ◄──  Swing toggle (opt) (pull-up, active-low)
  PD3 (OC2B)  ──►  IR LED driver  ← validated end-to-end against the real unit
  PB5          ──►  TX LED (IR flash, visible)
  PC3          ──►  feedback LED (visible, always-on or status)
```

Pin total: 9 rotary + 2 buttons + 1 IR + 2 LEDs = **14 of 23 usable GPIO**.

### Pin assignment (with Arduino Pro Mini labels)

AVR pin names are primary — the Timer2/OC2B (IR carrier) and INT0/PCINT (wake)
reasoning depends on them. The Arduino column is the silkscreen label to solder
to on the Pro Mini board.

| Function | AVR pin | Arduino | Dir | Notes |
|---|---|---|---|---|
| Fan speed b0 | PB2 | D10 | in | |
| Fan speed b1 | PB3 | D11 | in | MOSI — shared with ICSP during flash |
| Fan speed b2 | PB4 | D12 | in | MISO — shared with ICSP during flash |
| Mode b0 | PC0 | A0 | in | |
| Mode b1 | PC1 | A1 | in | |
| Mode b2 | PC2 | A2 | in | |
| Temp b0 | PD4 | D4 | in | |
| Temp b1 | PD5 | D5 | in | |
| Temp b2 | PD6 | D6 | in | |
| Resend button | PD2 | D2 | in | INT0 — external interrupt for wake; ext. pull-down, active-high (see §5) |
| Swing toggle | PD7 | D7 | in | optional |
| IR LED driver | PD3 | D3 | out | OC2B (Timer2) — **fixed**, validated |
| TX LED | PB5 | D13 | out | SCK — also the on-board LED on most Pro Minis |
| Feedback LED | PC3 | A3 | out | |

**Power / non-GPIO connections** (see [07_battery_and_power.md](07_battery_and_power.md)):

| Net | Pro Mini pin | From | Notes |
|---|---|---|---|
| Battery + | RAW | TP4056 OUT+ | cell voltage 3.0–4.2 V → on-board LDO → 3.3 V rail |
| Ground | GND | TP4056 OUT− | common ground |
| 3.3 V rail | VCC | on-board LDO out | feeds MCU + switch wipers + IR driver |

The TP4056 module charges the cell from USB-C and provides DW01A over-discharge /
over-current protection; its OUT+ / OUT− feed the Pro Mini's RAW pin (onboard LDO
path). Remove the Pro Mini power LED for battery use — see
[07_battery_and_power.md §3–4](07_battery_and_power.md).

![GPIO and power assignments on the Arduino Pro Mini](../images/circuit/gpios.png)

The IR driver is fixed at PD3 (OC2B) — the carrier is generated by Timer2 toggling
OC2B, validated end-to-end against the real unit
([sketches/daikin_fan_toggle](../sketches/daikin_fan_toggle)). All other assignments
are free and were arranged around it (Resend on PD2/INT0, Temp lines on PD4–PD6).

---

## 3. Diode encoding

Each 1-pole rotary switch has its wiper connected to **+3.3 V**.
From each position contact, small-signal diodes route to the code lines that should
read HIGH for that position. The code lines have **1 MΩ pull-downs to GND** — the
GPIO inputs are configured as plain `INPUT` (high-Z, no internal pull-up).

Reading the GPIO group as a binary nibble gives the switch position directly.

All three switches use **3 code lines** (natural binary, positions 0–N).
`position = read_gpio_group()` directly — no lookup, no offset in hardware.
The temperature range offset (heating vs cooling) is applied in firmware based on the Mode knob.

#### Natural binary vs. Gray code

**v1 uses natural binary** for readability — the diode pattern *is* the position
number — and relies on the firmware debounce (§6) instead of the encoding to reject
transients. Adjacent positions can therefore differ in more than one bit: Fan 3→4
goes `011`→`100`, flipping all three lines at once. If those lines don't switch in
the same instant (contact skew, or the momentary all-open float of a non-shorting
switch — see §1), the decoder can briefly read a bogus in-between code (7, 5, 1…).
That transient is exactly what the §6 debounce settle-loop filters out.

> **v2 decision — how to reject the mid-turn transient.** **Gray code** (reflected
> binary) makes **consecutive positions differ by exactly one bit**, so on a
> *shorting* switch a mid-transition read is always the old or new position — never a
> spurious third value — and the multi-read settle window (§6) could shrink to a
> one-shot read plus a short hold-off (firmware Gray→binary decode:
> `binary = gray ^ (gray >> 1) ^ (gray >> 2)` for 3 bits). **But Gray code does
> nothing for a non-shorting switch** (§1): the wiper's all-open gap floats every code
> line to `000` regardless of encoding, and `000` is the valid Off code. Because the
> SR16 knobs use **all 8 codewords** (8 positions on 3 bits), there is **no spare
> pattern left to reserve for "float / invalid."** So the real fork is:
>
> - **Option A — add a per-switch "valid" strobe line.** A 4th code line driven HIGH
>   at *every* detent (one diode from all N contacts to it). Read `0xxx` → wiper is
>   between detents → ignore; `1xxx` → on a detent, low 3 bits are the position (Off =
>   `1000`). Detects float *instantaneously and encoding-independently* — the strobe
>   goes low exactly when all position lines float. Lets the settle loop drop to a
>   one-shot even on a non-shorting switch. **Cost:** +1 GPIO per switch (+3 → 17/23
>   used), +1 diode per position (one from every contact to the strobe rail:
>   Fan +8, Mode +5, Temp +8 = **+21 diodes**, 29 → 50), +3 PCINT lines to arm.
> - **Option C — keep 3 bits, reject float by timing (status quo).** The float→`000`
>   is transient, so the §6 settle loop rejects it: `000` only survives the 10 ms
>   window if the knob is genuinely parked at Off. **Cost:** none in hardware, already
>   validated; but permanently tied to the multi-read settle window, and trusts that
>   no mechanical position dwells in float long enough to read as stable.
>
> (Reserving `000` as invalid and shifting positions up — "Option B" — is a dead end
> for the SR16 knobs: 8 positions need all 8 codewords, leaving none spare. It only
> fits ≤7-position knobs like Mode.)
>
> Recommendation: keep **C** for the perfboard prototype (validated, zero cost); adopt
> **A** on the PCB only if bench testing shows float dwell is marginal at 10 ms or you
> want to drop the settle loop. Gray code is worth wiring only if the ordered part
> turns out to be *shorting*. Decide alongside the §1 contact-style confirmation.

### Fan speed — SR16 1P8T (8 positions, 3 bits)

| Position | b2 | b1 | b0 | Meaning |
|---|---|---|---|---|
| 0 | 0 | 0 | 0 | Off |
| 1 | 0 | 0 | 1 | Speed 1 |
| 2 | 0 | 1 | 0 | Speed 2 |
| 3 | 0 | 1 | 1 | Speed 3 |
| 4 | 1 | 0 | 0 | Speed 4 |
| 5 | 1 | 0 | 1 | Speed 5 |
| 6 | 1 | 1 | 0 | Quiet |
| 7 | 1 | 1 | 1 | Auto |

### Mode — RS1010 1P (5 positions, 3 bits)

| Position | b2 | b1 | b0 | Mode |
|---|---|---|---|---|
| 0 | 0 | 0 | 0 | Fan |
| 1 | 0 | 0 | 1 | Cool |
| 2 | 0 | 1 | 0 | Heat |
| 3 | 0 | 1 | 1 | Dry |
| 4 | 1 | 0 | 0 | Auto |

Positions 5–7 are unused (switch has no detent there).

### Temperature — SR16 1P8T (8 positions, 3 bits)

The raw code (`b2 b1 b0`) is the natural-binary reading of the three lines. The
temp knob is wired **reversed** — raw 0 is the rightmost detent = warmest — so the
temperature counts *down* with raw code. The firmware absorbs this in a per-mode
lookup table (`TEMP_C[isHeat][raw]`), 1 °C steps, matching
[00_specifications.md §4.3](00_specifications.md):

| Raw code | b2 | b1 | b0 | Detent | Cooling (°C) | Heating (°C) |
|---|---|---|---|---|---|---|
| 0 | 0 | 0 | 0 | rightmost | 31 | 21 |
| 1 | 0 | 0 | 1 | | 30 | 20 |
| 2 | 0 | 1 | 0 | | 29 | 19 |
| 3 | 0 | 1 | 1 | | 28 | 18 |
| 4 | 1 | 0 | 0 | | 27 | 17 |
| 5 | 1 | 0 | 1 | | 26 | 16 |
| 6 | 1 | 1 | 0 | | 25 | 15 |
| 7 | 1 | 1 | 1 | leftmost | 24 | 14 |

Firmware: `temp_C = TEMP_C[mode == HEAT ? 1 : 0][raw]` (see `applyTempPos()`).

This raw-code-indexed table is also how every knob's wiring is corrected in
software: each per-knob table (`FAN_POS`, `MODE_POS`, `TEMP_C`) is indexed directly
by the raw GPIO reading, so any diode shift / reversed rotation / pin swap is
absorbed into the row order — a knob wired "wrong" is fixed by editing one table,
not by re-soldering.

> **v2 decision — fix wiring in software.** Keep this. Indexing each knob table by
> raw code is the low-risk way to reconcile whatever the PCB ends up routing, with
> no physical wiring convention to chase. The firmware already works this way; v2
> carries it forward.

### Wiring diagram

![RS1010 diode encoding wiring](../images/circuit/rs1010_wiring_2.png)

### Diode count

A diode is placed for each set bit in each position code:

| Switch | Positions used | Total diodes |
|---|---|---|
| Fan speed (SR16) | 8 | 12 |
| Mode (RS1010) | 5 | 5 |
| Temperature (SR16) | 8 | 12 |
| **Total** | | **29** |

(One diode per set bit, summed over each switch's position codes: Mode positions
0–4 have codes 000, 001, 010, 011, 100 → 0+1+1+2+1 = **5** diodes. The netlist
generator `schematics/circuit.py` derives these counts from the same tables.)

Use **1N4148** (THT) or **BAV99 / BAV70** (SOT-23, 2 per package).
Lay out as one rail per code line, diodes from contacts to rails — clean on perfboard.

### Why each switch needs its own lines

If three switches shared a bus, their codes would OR together and become unreadable
without active scanning (MCU awake, mux selected). Independent lines keep readout
and wake trivial: the MCU can read the full state at any time after waking, without context.

---

## 4. Pull-down value and sleep leakage

The wiper is held at +3.3 V continuously, including during sleep — the settled code
must stay on the lines so the wake interrupt can see an edge on the next move. Gating
the wiper supply would zero all lines and kill wake.

The pull-downs therefore draw current whenever a code line is HIGH. With 1 MΩ:
~3.3 µA per HIGH line, typically 1–2 lines HIGH per switch → a few µA total.

Do not use `INPUT_PULLUP`: the chip's ~20–50 kΩ internal pull-up fights the
external pull-down (50× stronger), lines would read HIGH always, and ~165 µA/line
would flow.

The sleep floor is dominated by the Pro Mini's onboard LDO quiescent current
(~75 µA), not the pull-down leakage — so 1 MΩ is comfortably below the floor and a
smaller value would still be acceptable. Bench-validate that reads stay reliable at
1 MΩ before treating it as settled. See [07_battery_and_power.md §3](07_battery_and_power.md)
for the full leakage budget.

---

## 5. Buttons

**Resend** (PD2 / INT0) — push button, retransmits the current knob state
unchanged (no stored state to diverge from physical position — see
[00_specifications.md](00_specifications.md)).
**Swing** (PD7) — on/off toggle, optional.

The two buttons use different wiring conventions on the bench:

**Resend** follows the rotary-switch convention (§3): the button connects the GPIO
to +3.3 V when pressed, with an external pull-down to GND at rest. The pin is
plain `INPUT` (high-Z, no internal pull-up) — pressed = HIGH, released = LOW.

```
+3.3V ── button ──┬── GPIO (plain INPUT)
                   └── pull-down ── GND
                  (reads LOW at rest, HIGH when pressed)
```

Do not use `INPUT_PULLUP` here for the same reason as §4: the internal
pull-up would fight the external pull-down.

**Swing** uses the internal-pull-up scheme instead: it connects between the GPIO
and GND, with the pin in `INPUT_PULLUP` — pressed/on = LOW, released/off = HIGH.

```
GPIO (INPUT_PULLUP) ──┬── toggle ── GND
                      └── (reads HIGH at rest, LOW when on)
```

No external resistor needed for Swing. Each line is armed for both-edge
interrupt for wake.

---

## 6. Wake — both-edge interrupts on every line

The ATmega328P uses **PCINT** (pin-change interrupt) for deep-sleep wake from
`SLEEP_MODE_PWR_DOWN`. PCINT fires on any logic-level change on a masked pin —
it is genuinely both-edge per pin.

ARM every code line and every button line before sleeping:

```c
// Example for Fan lines on PORTB (PB2/PB3/PB4 = PCINT2/3/4)
PCICR  |= _BV(PCIE0);
PCMSK0 |= _BV(PCINT2) | _BV(PCINT3) | _BV(PCINT4);
```

On any knob move or button press, ≥1 line changes → PCINT fires → MCU wakes.
The ISR does nothing except return; the main loop re-reads and debounces.

On a non-shorting switch, the inter-detent float (all lines momentarily 0) produces
extra edges — that only helps, it never prevents wake.

### Debounce

After waking, wait for the code to settle before reading:

```c
uint8_t readDebouncedCode() {
    uint8_t prev = readCodeOnce();
    for (uint8_t i = 0; i < MAX_SETTLE_TRIES; i++) {
        delay(SETTLE_MS);  // 10 ms validated on the RS1010
        uint8_t cur = readCodeOnce();
        if (cur == prev) return cur;
        prev = cur;
    }
    return prev;
}
```

The 10 ms settle window is validated on the RS1010 — see [howtos/03_rs1010_readout.md](../howtos/03_rs1010_readout.md).

---

## 7. Sleep / wake sequence

```
sleep  (SLEEP_MODE_PWR_DOWN, all lines PCINT-armed)
  │
  │  any code-line edge  ──or──  Resend / Swing edge
  ▼
wake
  read Fan (3b), Mode (3b), Temp (3b) with debounce
  map temp: temp_C = TEMP_C[mode == HEAT ? 1 : 0][raw]  (1 °C steps, raw reversed)
  read Swing toggle state
  if any knob position changed, or Resend pressed:
      build Daikin frame, transmit IR, flash TX LED
  ▼
sleep
```

The device transmits whenever a knob position changes (knob position *is* the
state — see [00_specifications.md](00_specifications.md)), and on an explicit
Resend press, which retransmits the current state unchanged — e.g. to recover
from a signal the AC unit missed.

---

## 8. Alternatives considered

**A — resistor ladder + 2-pole alternating-contact wake.** Each switch position maps to
an ADC voltage via a resistor chain. A second pole with alternating GND/3.3 V contacts
provides the wake edge. Works, but requires 2-pole switches (scarce for 11 positions),
a gated analog rail, and ADC calibration.

**B — RC differentiator + comparator.** A high-pass filter on the ADC wiper line
differentiates the position-change step into a voltage spike for wake detection. Frees
the switch to 1-pole but keeps the analog ladder and adds a comparator IC. Strictly
more complex than the chosen scheme.

**C — off-the-shelf coded rotary switch.** Position output directly as a 4-bit binary
code, built into the switch. Electrically identical to the chosen scheme but commercial
coded switches have screwdriver-slot shafts and no tactile detent feel — wrong for
a hand-turned interface. The chosen scheme is option C built on a big 1-pole switch.
