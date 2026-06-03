# Rotary Switch Choice

Three rotary selectors are needed: **Mode** (4 pos: Fan/Cool/Heat/Dry), **Fan speed** (6 pos: OFF + 1–5), **Temperature** (11 pos: 16–26 °C, or 10 pos: 16–25 °C fallback).

**All three switches are plain 1-pole (1P)** — the chosen readout is per-switch diode encoding (Approach D, see [05_electronics_circuit.md §3](05_electronics_circuit.md#3-readout--wake--design-options)): each switch's contacts are diode-encoded into a small binary code, and those code lines double as the both-edge wake interrupt. No 2-pole switches, no resistor ladders, no ADC. **Switches are therefore chosen purely on body size and detent feel** — the position count just sets how many code lines (Fan 3, Mode 2, Temp 4).

## Switch families considered

### RS1010 — reference form factor

![RS1010](images/rotary_switch/rs1010_web.png)

PCB through-hole mount, M7×0.75 bushing (~12.5 mm), 6 mm shaft, 30° indexing, compact body.

| Variant | Config | Max positions |
|---|---|---|
| RS1010 | 1P | 2 – 6 |
| RS1010 | 2P | 2 – 4 |

**Tops out at 6 positions.** Fine for Mode (5 pos) and Fan (5–6 pos). Not enough for Temperature (need 10–11).

Electrical rating: 0.1 A / 16 V DC — more than adequate for resistor-ladder ADC readout.

### Grayhill 56 series

![Grayhill 56](images/rotary_switch/Grayhill_56_web.png)

Same compact footprint as RS1010 (~12.7 mm bushing), available up to 12 positions (1P12T), well-documented, PCB through-hole. Good drop-in candidate for the temperature selector.

### NKK MR-K112

![NKK MR-K112](images/rotary_switch/NKK_MR-K112_web.png)

Ultra-compact, ~13 mm, up to 12 positions (1P or 2P), 2.54 mm PCB pitch. Slightly smaller than RS1010. Less common but very clean form factor.

> **Overkill & expensive.** Premium part aimed at industrial/medical applications. [Datasheet](https://www.nkkswitches.com/pdf/MRlogicLevel.pdf)

### Alpha series — [official product list](https://www.taiwanalpha.com/en/products/8?cat=59)

### Alpha SR1610

![Alpha SR16](images/rotary_switch/sr16_web.png)

16 mm body, PCB through-hole, 6 mm D-shaft, 30° indexing. Available 1P or 2P, up to 12 positions. Widely stocked (Tayda, Mouser). Common in DIY audio/synth projects. Noticeably bigger than RS1010.

### Alpha SR1712F

![Alpha SR1712F](images/rotary_switch/SR1712F-0108_alpha_web.png)

17 mm body, PCB through-hole, D-shaft, 30° indexing. Available 1P or 2P, up to 8 positions. Rated 0.3 A / 16 V DC, non-shorting contacts, 10 000 cycle lifetime. Sufficient for Mode (5 pos) and Fan (5 pos), not enough for Temperature (10–11 pos). [Datasheet](https://www.mouser.com/datasheet/2/13/SR1712F-1815245.pdf)

### Alpha SR2510 / SR10010F

![Alpha SR10010F](images/rotary_switch/sr10010F_Alpha_web.png)

25 mm body, PCB through-hole, up to 12 positions. Larger footprint — same family but noticeably bulkier. Overkill for Mode/Fan; covers Temperature but at the cost of panel space.

### Lorlin CK1060 / CK1032

![Lorlin CK1060](images/rotary_switch/Lorlin_ck1060_web.png)

27.5 mm body — much larger. Already in BOM as fallback. Position-limiting washer allows bridging a 1P12T to any count ≤ 12. Proven and cheap but bulky for a compact enclosure.

### 8404-3C (on hand)

3P4T (3 circuits, 4 positions). Only 4 positions — covers OFF / Fan / Cool / Heat, dropping Dry. The 3 independent circuits are unused in this design (resistor-ladder uses only one pole). Usable if Dry mode is dropped.

---

## Readout & wake — see the circuit doc

How the switches are read and woken — the chosen per-switch diode encoding (Approach D), and the rejected alternatives (ladder + 2P, ladder + comparator, off-the-shelf coded switch) — is covered in [05_electronics_circuit.md §3](05_electronics_circuit.md#3-readout--wake--design-options). The conclusion that matters for switch *selection*: **all three are 1P**, chosen on size and feel.

---

## Decision matrix

All three are **1P**, diode-encoded (Approach D). Choose on **body size and detent feel**; any spare poles on an on-hand part are simply unused. Code-line count per switch: Fan 3, Mode 2, Temp 4.

| Selector | Positions | Candidate (1P) | Body size | Notes |
|---|---|---|---|---|
| Mode | 4 | 8404-3C (on hand) | unknown | on hand; 4 pos = Fan/Cool/Heat/Dry exactly. Spare poles unused. |
| Mode | 4 | RS1010 1P | compact | alternative |
| Fan speed | 6 | Alpha SR1712F 1P8T (bridged to 6) | 17 mm | preferred — good feel, common |
| Temperature | 11 | Alpha SR1610 1P12T (bridged to 11) | 16 mm | preferred — widely stocked |
| Temperature | 11 | Lorlin CK1032 1P (bridged to 11) | 27.5 mm | alternative — bigger, very tactile |
| Temperature | 11 | Grayhill 56 1P12T | compact | alternative — compact but pricier |
| Temperature | 11 | Coded rotary switch (Nidec SH-7000 / ALPS EC05) | 7 mm | ruled out — too small / screwdriver feel (Approach C) |

### Temperature range with 10 positions

The RS1010 family (and Grayhill 56 bridged to 10) covers 10 steps. Two practical mappings:

| Mapping | Range | Missing |
|---|---|---|
| 16–25 °C | 10 steps × 1 °C | 26 °C |
| 18–27 °C | 10 steps × 1 °C | 16–17 °C and 26 °C |

**16–25 °C is the realistic daily-use range** for a European home. Dropping 26 °C is acceptable.

With a 12-position switch bridged to 11: full 16–26 °C coverage is possible.

---

## Open questions

- [ ] Confirm enclosure footprint allows three knobs (16–17 mm body) side by side in 80×100 mm panel.
- [ ] Source and price check on Tayda / Mouser / LCSC: three **1P** switches (Alpha SR1712F 1P for Fan; Alpha SR1610 1P12T / Lorlin CK1032 1P / Grayhill 56 1P12T for Temp) + small-signal diodes.
- [ ] Pick all three on **detent feel and body size** (unconstrained by poles); confirm shaft + knob compatibility (6 mm D-shaft vs round).

(Circuit-level open questions — ADC margins, gated rail, sleep current — are in [05_electronics_circuit.md](05_electronics_circuit.md).)
