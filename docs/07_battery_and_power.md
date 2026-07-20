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

## 6. Charging

The **TP4056 module** (widely available, ~€0.50) includes:
- Constant-current / constant-voltage Li-Po charging (default 1 A, adjustable via RPROG).
- DW01A + FS8205 protection: over-discharge cutoff (~2.5 V), over-charge, over-current.
- Charge current indicator LED (optional, can be left or removed).

Connect USB-C to the module's IN+ / IN−. The module's OUT+ / OUT− feed the board.
No additional protection components needed.
