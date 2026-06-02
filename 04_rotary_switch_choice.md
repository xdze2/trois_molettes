# Rotary Switch Choice

Three rotary selectors are needed: **Mode** (5 pos), **Fan speed** (5 pos), **Temperature** (10–11 pos).

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

## Readout method: resistor ladder on ADC

All selectors wired as a resistor ladder on a single ADC pin. Equal resistors in series between each position output; common pin to ADC; one end to 3.3 V, other end to GND.

- Mode (5 pos) → 1 ADC pin
- Fan (5 pos) → 1 ADC pin
- Temperature (10–11 pos) → 1 ADC pin

Total: **3 ADC pins**. On RP2040 (Pico), ADC0–ADC2 are all usable without restriction.

Ladder resistor value: 10 kΩ per step is a safe starting point. Spacing between voltage levels grows smaller with more positions — verify margins for the 11-position ladder with the MCU's 12-bit ADC.

---

## Decision matrix

| Selector | Positions needed | Candidate | Body size | Decision |
|---|---|---|---|---|
| Mode | 5 | RS1010 1P6T (bridged to 5) | compact | **preferred** |
| Fan speed | 5 | RS1010 1P6T (bridged to 5) | compact | **preferred** |
| Temperature | 10–11 | Grayhill 56SP12 (bridged to 10–11) | compact | **preferred** |
| Temperature | 10–11 | NKK MR-K112 | ultra-compact | alternative |
| Temperature | 10–11 | Lorlin CK1032 bridged to 11 | 27.5 mm | fallback |
| Mode | 4 | 8404-3C (on hand) | unknown | only if Dry dropped |

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

- [ ] Confirm enclosure footprint allows three RS1010-size bushings plus a Grayhill 56 (same size) — should fit in 80×100 mm.
- [ ] Verify ADC voltage spacing for 11-position ladder on RP2040 (3.3 V ref, 12-bit → ~3 mV per LSB, ~300 mV per step with equal 10 kΩ resistors — needs simulation or bench test).
- [ ] Source check: RS1010 availability on TME / Mouser / LCSC for Mode and Fan selectors.
- [ ] Confirm shaft length and knob compatibility (6 mm D-shaft vs round shaft).
