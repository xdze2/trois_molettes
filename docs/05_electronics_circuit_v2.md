# Electronics Circuit v2 — Evolution

Planned changes to the input wiring for v2 (Mouser sourcing, PCB/enclosure milestone).
For the current, built v1 circuit (diode-encoded switches, pin map, wake, sleep
sequence) see [05_electronics_circuit.md](05_electronics_circuit.md). For power see
[07_battery_and_power.md](07_battery_and_power.md); for the IR driver see
[06_IR_LED_wiring.md](06_IR_LED_wiring.md).

---

## 1. Why change the rotary selector

v1 reads plain 1-pole rotary switches through a per-switch diode matrix (29 diodes
total — see [05 §3](05_electronics_circuit.md#3-diode-encoding)). It works and is
validated on the bench, but the diode array is the single biggest thing bloating the
perfboard layout, and it carries a transient-rejection problem (mid-turn codes need
debouncing — see [05 §1](05_electronics_circuit.md#1-switch-selection)).

Sourcing also shifts from AliExpress (v1) to Mouser for v2, which opens up parts that
weren't practical to source individually before — including one-piece absolute
encoders that output a position code directly, no diodes required.

### Common hard requirements (unchanged from v1)

Whichever route, every selector must be:

- **PCB / perfboard mountable on a 2.54 mm grid.** This is the pivotal constraint. If
  a part's pins land on 0.1″ pitch it drops straight into **perfboard** — no custom
  PCB needed. A custom PCB is only forced when a part is *off*-grid. So an on-grid
  coded encoder is doubly attractive: it removes the diode array (the main thing
  bloating the perfboard) *and* stays perfboard-compatible, resolving the overall
  size constraint without a fab order.
- **Real knob shaft** — a 6 mm (¼″) round or D/flatted shaft that accepts a
  **3D-printed knob**. This rules out screwdriver-slot "trimmer"-style coded
  switches (see below, S-8000).
- **Hand-turned detent feel** and adequate rotational life for daily use.

---

## 2. Chosen candidate — Bourns PAC18R absolute encoder

**`PAC18R1-41D19F`** ([datasheet](../datasheets/pac18r.pdf)) — a Bourns *Pro Audio*
18 mm absolute encoder that meets every requirement above in one part:

| Spec | Value |
|---|---|
| Output | **4-bit Gray code**, absolute (one code per detent, held continuously) |
| Positions | **8** (also 12 / 16 variants; no 5/6-pos — see Mode note) |
| Mounting | 5 PC pins, **2.5 mm pitch, through-hole** → drops into perfboard |
| Shaft | 6.34 mm insulated **flatted** shaft, 19.5 mm; 3/8-32 threaded bushing + nut/washer for a 3D-printed knob |
| Feel | "High-class rotational feel," haptic detents; detent torque 350–650 gf·cm |
| Body | 18 mm square, low profile |
| Life | 30 000 cycles min |
| Wiring | 4 code lines + GND — **no diodes, no pull-down rail** |

Part-number decode: `PAC18R1` · `-4` PC-pins-down/threaded bushing · `1` 8 positions ·
`D` detents · `19` 19.5 mm shaft · `F` insulated flatted shaft.

**Why this is a strictly better fit than the diode matrix:**

- **Gray code, verified single-bit steps.** The datasheet 8-position table steps
  `0000 → 0100 → 1100 → …`, exactly one bit per detent. A mid-turn read is therefore
  always an adjacent *valid* position, never a spurious third code — so the whole
  v1 "strobe line vs debounce" fork and shorting/non-shorting analysis
  (see [05 §1](05_electronics_circuit.md#1-switch-selection)) **do not apply** to
  this part. It self-encodes cleanly; there is no wiper-through-diodes topology to
  float to `000`.
- **Perfboard-friendly at 2.5 mm pitch**, so v2 stays on perfboard while *shrinking*
  it (no diode array).
- **Cost:** +1 GPIO per switch — 4 code lines vs. 3 for natural binary, so the pin
  budget (see [05 §2](05_electronics_circuit.md#2-overview)) rises from 9 to 12
  rotary lines (14 → 17 of 23 used). Good trade for deleting 29 diodes and the
  transient logic.

> **Mode needs 5 positions; PAC18R comes in 8 / 12 / 16 only.** Use the **8-position**
> part for Mode too and leave 3 detents unused (firmware ignores codes 5–7, exactly as
> the RS1010 does today) — giving a uniform 3× `PAC18R1-41D19F` BOM — or keep Mode on
> the RS1010 + diodes. Prefer the uniform BOM for price breaks and panel symmetry.

> **Open — confirm before ordering:** live Mouser unit price (Pro Audio parts often
> €6–12; the project target is ≤ €5/unit — if it overshoots, the v1 diode matrix stays
> the cost-optimal choice) and that the 30 000-cycle life is adequate for daily use.

### Note on the moot v1 transient analysis

This contact-style / Gray-code analysis in
[05 §1](05_electronics_circuit.md#1-switch-selection) applies to the bare
Alpha/RS1010 switches. It is **moot for the PAC18R**, whose Gray-coded output has no
wiper-to-diode topology to float.

### Fixing wiring in software carries forward unchanged

v1's approach — indexing each knob's position table (`FAN_POS`, `MODE_POS`,
`TEMP_C`) directly by raw GPIO code, so a diode shift / reversed rotation / pin swap
is absorbed into the row order rather than re-soldering — carries forward to v2
unchanged. Keep it: it's the low-risk way to reconcile whatever the PCB ends up
routing, with no physical wiring convention to chase. See
[05 §3](05_electronics_circuit.md#3-diode-encoding) for the current implementation.

---

## 3. Other coded encoders / switches evaluated

**Grayhill Series 25B** — 1/4″ shaft, 2.54 mm PC pins, Gray/binary output, 8-pos via
45° throw, shorting contacts, 50 000-cycle life — a genuine UI-grade coded switch and
mechanically ideal, but mil-spec priced (~€15–40/unit), well over the €5 target.

**Elma coded switches** — HEX/Gray/BCD, tactile, up to 16 positions. Same premium
class as Grayhill; possibly better EU availability. Not priced.

**Same Sky S-8000** — 3-bit BCO absolute coded switch, 2.54 mm DIP, cheap (~€3–6). But
screwdriver-slot / φ3.6 shaft, 36 mN·m adjustment torque and only 10 000-cycle life: a
set-and-forget address selector, **not** a hand-turned control. It is BCO not Gray, so
it does not even buy the transient-free benefit. See [datasheet](../datasheets/s-8000.pdf).

*Excluded category — incremental/quadrature encoders* (Bourns PEC11, ALPS EC11, ~€1–3):
cheap and nice-feeling, but they output *increments*, not absolute position, and hold
no state through power-down. That breaks "knob position is the state," so they are out
regardless of price.

This is the "UI-grade absolute encoder" branch of the option-C family in
[05 §8](05_electronics_circuit.md#8-alternatives-considered) — proper knob shaft,
haptic detents, Gray-coded output, done with a single one-piece part instead of a
switch + diode matrix.

---

## 4. Open questions

- [ ] Confirm live Mouser unit price for `PAC18R1-41D19F` vs. the ≤ €5/unit target.
- [ ] Decide Mode: 8-position PAC18R with 3 detents unused (uniform BOM) vs. RS1010 + diodes.
- [ ] Confirm three 18 mm bodies + Send button fit the 80×100 mm panel face.
- [ ] Confirm 30 000-cycle life is adequate for expected daily use.

If the PAC18R overshoots the cost target, fall back to the v1 diode matrix — see
[05 §1](05_electronics_circuit.md#1-switch-selection) for the open questions on that
route (SR16 stock/price, panel fit, shared knob cap).

See also [roadmap.md](roadmap.md) for how this fits into the v2 milestone (rotary
selector, PCB-vs-perfboard, IR LED, enclosure).
