# Battery and Power

Power supply, charging, sleep current budget.
For IR transmit driver see [06_IR_LED_wiring.md](06_IR_LED_wiring.md);
for switch wiring and pull-down leakage see [05_electronics_circuit.md](05_electronics_circuit.md).

---

## 1. Power architecture

```
Li-Po cell (3.7 V nominal, 3.0–4.2 V range)
  └── TP4056 module (charge + DW01A protection)
        └── VBAT ──► ATmega328P Pro Mini 3.3 V (onboard LDO)
                        └── 3.3 V rail ──► switch wipers (diode encoding)
                                       ──► IR LED driver (via transistor, §2 bulk cap)
```

- **TP4056 module** handles charge from USB-C, over-discharge cutoff (DW01A), and over-current protection. No separate components needed.
- **Pro Mini LDO** (MIC5205 or equivalent) regulates battery to 3.3 V for the MCU and switch wipers. Its quiescent current (~50–100 µA) is the dominant sleep term on this board — see §3.
- The switch wiper rail is not gated: the settled code must stay on the lines during sleep for PCINT wake to work (see [05 §3](05_electronics_circuit.md)).

---

## 2. Cell sizing

Target: **6 months** on a single charge (goal: years).

Because the device sleeps nearly 100% of the time, average current ≈ sleep current.
Active time (IR transmit, ~200 ms at ~100 mA peak) at typical usage is well under 1 µA averaged.

| Avg current | Life on 400 mAh | Life on 1000 mAh |
|---|---|---|
| 50 µA | ~11 months | ~2.3 years |
| 100 µA | ~6 months | ~1.1 years |
| 200 µA | ~3 months | ~7 months |

Size the cell only after measuring actual sleep current (§3). A small flat Li-Po
(e.g. 400–600 mAh, ~5–6 mm thick) fits the 25 mm enclosure depth constraint.

---

## 3. Sleep current budget

**What draws current in sleep:**

| Source | Typical | Notes |
|---|---|---|
| Pro Mini LDO quiescent | ~50–100 µA | Dominant term. MIC5205 ~75 µA typ. |
| ATmega328P power-down | ~0.1–0.4 µA | Negligible |
| Pro Mini power LED | ~1 mA | **Remove before use** — dominant if left in |
| Diode-encoding pull-downs (1 MΩ) | ~3–7 µA | 1–2 HIGH lines per switch at ~3.3 µA each |

The LDO quiescent current dominates. The Pro Mini isn't ideal for battery use — the
LDO and always-on power LED together waste ~1.1 mA. After desoldering the LED, the
LDO quiescent (~75 µA) is the real floor, and at 100 µA total (LDO + pull-down
leakage) a 600 mAh cell gives ~8 months.

**To-do:** measure actual sleep current on the bench with the power LED removed and
the MCU in `SLEEP_MODE_PWR_DOWN`. This fixes the cell size.

---

## 4. Pro Mini modifications for battery use

- **Desolder the power LED** (D1, on-board, connected directly to VCC). It draws ~1 mA continuously and dominates the sleep budget.
- **Do not use the RAW pin** to power the board from battery — it goes through a second regulator stage with higher drop. Use the **VCC pin** directly from the TP4056 output if you bypass the onboard LDO, or connect battery to RAW and accept the LDO quiescent cost.
- Optional: **replace the LDO** with a lower-quiescent part (e.g. MCP1700, 1.6 µA Iq) to bring the sleep floor down from ~75 µA to a few µA. Not required for the 6-month target but opens "years" range on a small cell.

---

## 5. Bulk capacitor (IR pulse stabilisation)

The IR LED driver pulls ~100–300 mA for ~600 µs per mark. The battery internal
resistance (and LDO load transient) can sag the rail enough to brownout the MCU.

Place a **220 µF electrolytic** physically close to the transistor collector / VCC node.
It supplies the transient locally without requiring a large battery.

See [06_IR_LED_wiring.md §Bulk Capacitor](06_IR_LED_wiring.md) for sizing details.

---

## 6. Alkaline (AAA / AA) as an alternative to Li-Po

The Li-Po + TP4056 stack is convenient (USB-C recharge, integrated protection) but
adds a charge IC, a soft-pouch cell that's awkward to mount, and a chemistry that's
overkill for a device drawing ~100 µA. Because sleep dominates and years-long life
is plausible even at sub-mA, **primary alkaline cells are worth considering** — a
user swaps them once a year or two rather than plugging in a cable.

### Capacity vs. Li-Po

| Cell | Nominal V | Usable capacity | Notes |
|---|---|---|---|
| AAA alkaline | 1.5 V (1.6 → 0.9 V) | ~1000–1200 mAh | Small; 2–3 fit a slim enclosure |
| AA alkaline | 1.5 V (1.6 → 0.9 V) | ~2000–2800 mAh | 2–3× the AAA energy, but Ø14.5 × 50 mm |
| 9 V (6LR61) | 9 V (9 → 6 V) | ~550 mAh | Single cell + snap, but low mAh and wastes V in regulation |
| Li-Po (§2) | 3.7 V (4.2 → 3.0 V) | 400–1000 mAh | Rechargeable; needs TP4056 |

Even AAA alkaline holds **more usable energy than a small Li-Po**: at 100 µA total
sleep current, a single AAA (~1100 mAh) already lasts ~1.2 years, and AA pushes past
2–3 years. Both easily meet the 6-month target with margin.

### Voltage: the real design question

The MCU rail is 3.3 V. Alkaline cells give 1.5 V each, sagging toward 0.9 V as they
deplete, which changes how they must feed the board:

- **1 × AAA/AA (0.9–1.6 V):** requires a **boost converter** up to 3.3 V. Adds a
  part and its quiescent draw (pick a low-Iq boost, e.g. TPS61200-class ~µA range),
  but gives the fewest cells and simplest holder.
- **2 × AAA/AA (1.8–3.2 V):** feeds the ATmega328P **directly, no regulator** — the
  328P runs 1.8–5.5 V and stays valid across the whole discharge curve. This is the
  most efficient option: no LDO/boost quiescent loss at all, dropping the sleep floor
  to essentially the MCU + pull-down leakage (~4 µA). The catch: the IR-LED forward
  drop and the diode-encoded readout thresholds must both be checked at the low end
  (~1.8 V) — see [06_IR_LED_wiring.md](06_IR_LED_wiring.md) and
  [05_electronics_circuit.md](05_electronics_circuit.md).
- **3 × AAA/AA (2.7–4.8 V):** back to needing a **3.3 V regulator** (buck or LDO with
  low dropout), spending quiescent current again — no advantage over 2-cell unless the
  extra headroom is needed for the IR forward voltage.
- **9 V (6LR61):** possible but the weakest option. It needs a **step-down regulator**
  (buck; an LDO would burn ~3.3/9 ≈ 63% of the energy as heat), and a 9 V alkaline
  only holds ~550 mAh — so even at buck efficiency its effective 3.3 V energy is on the
  order of a *single AAA*, in a far bulkier cell. Its only merit is the single-cell
  snap connector. Not recommended unless a 9 V snap is desirable for some other reason.

**Recommended:** 2 × AAA driving the 328P directly (bypassing the Pro Mini LDO
entirely — see §4 on not using RAW). This removes the dominant ~75 µA LDO term and
the TP4056, at the cost of a battery holder and losing USB-C recharge. It only works
if the IR driver and readout are validated down to ~1.8 V; if the IR stage needs more
headroom, prefer 3 × AAA + a low-dropout 3.3 V regulator, or keep the boost/Li-Po path.

### Trade-offs

- **For:** no charge circuit, no soft-pouch mounting problem, higher usable energy,
  and the 2-cell path removes regulator quiescent entirely.
- **Against:** no recharge (swap cells), a battery holder eats enclosure volume
  (AA especially — the 50 mm length fights the slim-panel goal, so **AAA is preferred**),
  and alkaline self-discharge / leakage over multi-year storage is a real risk (lithium
  primary AAA — Energizer L92 — avoids leakage and adds capacity if that matters).

This stays open until bench sleep-current is measured (§3): the LDO-vs-direct-drive
decision only pays off once we know the real floor.

---

## 7. Charging

The **TP4056 module** (widely available, ~€0.50) includes:
- Constant-current / constant-voltage Li-Po charging (default 1 A, adjustable via RPROG).
- DW01A + FS8205 protection: over-discharge cutoff (~2.5 V), over-charge, over-current.
- Charge current indicator LED (optional, can be left or removed).

Connect USB-C to the module's IN+ / IN−. The module's OUT+ / OUT− feed the board.
No additional protection components needed.
