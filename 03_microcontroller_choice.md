# Microcontroller Choice

## 1. Constraints

| Constraint | Detail |
|---|---|
| IR library | Must support Daikin protocol without rewriting from scratch |
| Power supply | Direct from single Li-Po cell (3.0–4.2 V) |
| Form factor | Small as possible |
| Power consumption | Low sleep current — battery-powered handheld device |
| Cost | Clone/budget boards acceptable for prototype |
| Dev friction | No extra programmer required |

---

## 2. Candidates

| | RP2040 — Raspberry Pi Pico | ATmega328P — Arduino Nano (clone) | ATmega4809 — Arduino Nano Every | ESP32-WROOM — ESP32 DevKit (clone) | ATtiny85 — Digispark |
|---|---|---|---|---|---|
| Form factor | 21×51 mm | 18×45 mm | 18×45 mm | 28×54 mm | 25×19 mm |
| Flash | 2 MB | 32 KB | 48 KB | 4 MB | 8 KB |
| Sleep current | ~1–2 mA die / **~1.3 mA stock board** | ~15–20 mA (no proper sleep) | ~3 mA | ~10–20 mA | ~0.1 mA |
| Operating voltage | 1.8–5.5 V | 1.8–5.5 V | 1.8–5.5 V | 2.3–3.6 V core | 2.7–5.5 V |
| Onboard LDO | Yes (VSYS pin) | Yes (RAW pin) | Yes | Yes (VIN pin) | No |
| Direct Li-Po feed | Yes ✓ | Yes ✓ | Yes ✓ | Yes ✓ | Risky |
| IR library | IRremoteESP8266 — **unverified on Pico** | IRremote ✓ (flash tight) | IRremote ✓ | IRremoteESP8266 ✓ | DIY only |
| Extra programmer | No | No | No | No | No (USB built-in) |
| Clone available | Yes (~€2–4) | Yes (~€2–4) | Not commonly cloned | Yes (~€3–5) | Yes (~€3–5) |
| Genuine price | €4–6 | €20–25 | €9–12 | €8–12 | €5–8 |
| Recommended source | EU distributor (genuine) | AliExpress clone | Genuine (no clone advantage) | AliExpress clone | AliExpress clone |

---

## 3. IR Library Situation

The IR library is the hard constraint — the Daikin protocol is complex (3 frames, 35 bytes, checksum) and not worth reimplementing from scratch.

| Library | Targets | Daikin support | Notes |
|---|---|---|---|
| IRremoteESP8266 | ESP8266, ESP32 | Full (`IRDaikinAC`) | Does not officially target Pico/AVR |
| IRremote | AVR, STM32, Pico, ESP32 | Partial | Daikin support less complete; may need validation |

**Key unknown:** does IRremoteESP8266 compile and run correctly on the Pico using the Arduino-Pico core? This must be validated early — it determines whether the Pico is viable at all.

---

## 4. Power Supply Notes

All dev board candidates accept direct Li-Po feed (3.0–4.2 V) on their RAW/VSYS/VIN pin via their onboard LDO. No external regulator needed.

Recommended additions regardless of MCU choice:
- **100 µF bulk capacitor** on the power rail — absorbs the ~100 mA current spike when the IR LED fires, prevents MCU brownout reset
- **TP4056 module with protection IC** (DW01A + FS8205) between battery and MCU — handles over-discharge cutoff and over-current protection

---

## 5. Decision

### Preferred: RP2040 — Raspberry Pi Pico (genuine)

- Best sleep current of the realistic candidates
- Ample flash (2 MB) — no library size concerns
- Direct Li-Po on VSYS pin
- Genuine board is already cheap (€4–6 from EU distributor, 2-day shipping)
- Clone has almost no price advantage — buy genuine

**Caveat on sleep current:** the ~1–2 mA figure is the die. A *stock Pico board* sleeps at ~1.3 mA because of its always-on RT6150 SMPS, which alone gives only ~9 weeks on a 2000 mAh cell — short of the 6-month target. Reaching 6 months likely requires a **bare RP2040 (or RP2350) + efficient external LDO** rather than the stock board, plus gating the resistor-ladder rail off during sleep. Validate on the bench before committing the board form. See [Battery budget](05_electronics_circuit.md#6-battery-budget).

**Blocker to verify first:** IRremoteESP8266 on Pico with Arduino-Pico core. If it fails, fall back to ESP32 DevKit clone.

### Fallback: ESP32-WROOM — ESP32 DevKit (clone)

- IRremoteESP8266 works natively and is well tested
- Higher sleep current (~10–20 mA) — worse battery life
- Larger board
- WiFi available as bonus (could sync state from BRP069B41 adapter if desired later)
- Clone ~€3–5 from AliExpress

### Ruled out

| Board | Reason |
|---|---|
| ATmega328P — Arduino Nano | No proper sleep mode, ~15–20 mA quiescent — unacceptable for battery device |
| ATmega4809 — Arduino Nano Every | Better sleep but no clone, genuine price not justified over Pico |
| ATtiny85 — Digispark | 8 KB flash, no IR library — would require full Daikin protocol reimplementation |

---

## 6. First Prototype Step

Before any hardware assembly, validate the IR library on the chosen MCU:

1. Flash a minimal sketch on the Pico using Arduino-Pico core
2. Include IRremoteESP8266 / `IRDaikinAC`
3. Send a hardcoded `setPower(true)`, `setMode(kDaikinCool)`, `setTemp(22)`, `send()`
4. Verify the AC unit responds

If step 4 fails or the library does not compile, switch to ESP32 DevKit and repeat.
