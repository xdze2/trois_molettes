# Rotary Switch Choice

Three rotary selectors needed:

| Selector | Positions | Switch |
|---|---|---|
| Fan speed | 8 (Off / 1–5 / Quiet / Auto) | Alpha SR16, 1P8T |
| Mode | 5 (Fan / Cool / Heat / Dry / Auto) | RS1010, 1P |
| Temperature | 8 (2 °C steps, mode-dependent range) | Alpha SR16, 1P8T |

All are **plain 1-pole (1P)** — read by per-switch diode encoding, see
[05_electronics_circuit.md](05_electronics_circuit.md). Switches are chosen
purely on body size and detent feel; pole count is not a constraint.

---

## Fan speed — Alpha SR16 1P8T

![Alpha SR16](images/rotary_switch/sr16_web.png)

8 positions: Off / 1 / 2 / 3 / 4 / 5 / Quiet / Auto.

16 mm body, PCB through-hole, 6 mm D-shaft, 30° indexing. Widely stocked
(Tayda, Mouser). Good detent feel, common in DIY audio/synth.

Code lines: **3 bits** (natural binary, positions 0–7).

## Mode — RS1010 1P

![RS1010](images/rotary_switch/rs1010_web.png)

5 positions: Fan / Cool / Heat / Dry / Auto.

Compact PCB through-hole, M7×0.75 bushing (~12.5 mm), 6 mm shaft, 30° indexing.
The RS1010 goes up to 6 positions in 1P — 5 fits cleanly.

Code lines: **3 bits** (positions 0–4, one code unused).

## Temperature — Alpha SR16 1P8T

Same part as Fan speed. 8 positions, 2 °C steps.

Firmware applies a mode-dependent offset to map the 8 positions to the
correct temperature range (see [00_specifications.md §4.3](00_specifications.md)).

Code lines: **3 bits** (natural binary, positions 0–7).

---

## GPIO summary

| Selector | Bits | Pins |
|---|---|---|
| Fan speed (SR16) | 3 | PB2 PB3 PB4 |
| Mode (RS1010) | 3 | PC0 PC1 PC2 |
| Temperature (SR16) | 3 | PD2 PD3 PD4 |
| Send button | 1 | PD5 |
| Swing toggle (opt.) | 1 | PD6 |
| IR TX | 1 | PD7 (OC2A) |
| TX LED (IR flash) | 1 | PB5 |
| Feedback LED | 1 | PC3 |
| **Total** | | **14 of 23 usable** |

---

## Alternative considered — two RS1010 for temperature

Two RS1010 switches (5×5 = 25 positions) would give 1 °C resolution across a
wider range. Rejected: doubles the switch count, complicates panel layout, and
2 °C steps cover the practical daily-use range adequately for this application.

---

## Other switches evaluated

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

---

## Open questions

- [ ] Confirm SR16 1P8T stock and price on Tayda / Mouser.
- [ ] Confirm three knobs (two SR16 + one RS1010) fit the 80×100 mm panel face with the Send button (and optional swing toggle).
- [ ] Confirm 6 mm D-shaft on both SR16 and RS1010 accepts the same knob cap.
