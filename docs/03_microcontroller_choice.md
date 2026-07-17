# Microcontroller Choice

## 1. Constraints

The device sleeps **~100 % of the time** (a few sub-second wakes per day), so **sleep current ≈ average current ≈ the entire battery budget**. That, plus the two requirements your interface design forces — *wake on ~13 independent GPIO lines* and *hand-build on perfboard from a dev board* — are what actually drive the choice. The IR library, which used to head this list, is now a convenience, not a gate.

| Constraint | Weight | Detail |
|---|---|---|
| **Sleep current** | **hard** | Battery life ≥ 6 months (goal: years) is what makes the remote *usable* — without it it dies and the official remote becomes the fallback, defeating the project. Sleep ≈ average current, so this dominates. Budget against the **dev-board** figure, not the die. |
| **13-pin both-edge wake** | **hard** | The diode-encoding readout ([05 §3.5](05_electronics_circuit.md#35-the-chosen-scheme--per-switch-diode-encoding)) needs ~13 input lines (9 rotary code + 4 buttons) each able to wake the MCU on either edge, from its deepest viable sleep mode. Chips that restrict deep-sleep wake to a few dedicated pins fail here. |
| **Dev board + perfboard only** | **hard** | No PCB routing or fabrication. Build on a **hand-solderable dev board / module** on perfboard. This rules out bare-die solutions (bare RP2040 + LDO, bare WROOM) — so the board, not the die, must hit the sleep target. |
| Power supply | hard | Direct from single Li-Po cell (3.0–4.2 V), via the board's onboard regulator or VBAT. |
| IR library | **soft** | Off-the-shelf Daikin support (IRremoteESP8266) is an "easy-life" bonus, **not** required. The Daikin frame is a documented byte array + checksum + 38 kHz carrier ([01](01_IR_protocol_and_mapping.md)); porting/reimplementing the transmit is accepted in-scope firmware work. The MCU is **not** constrained to library-supported chips. |
| Cost | soft | Within the €35 BOM; clone/budget boards fine for prototype. |
| Form factor | soft | Small is nice; all realistic candidates fit the 80×100 mm panel. |
| Dev friction | soft | No extra programmer required (USB or onboard debugger preferred). |

---

## 2. Candidates

Two axes decide it: **dev-board sleep current** (in the deepest mode that still wakes on 13 GPIO) and **how many GPIO can wake from that mode**. The old front-runner (RP2040) and its fallback (ESP32 DevKit) both fail the first; two cheap ESP32 variants fail the second. The table is ordered by fitness.

| | **nRF52840 — Seeed XIAO** | **STM32L4 — WeAct/Nucleo board** | **ATmega328P — Pro Mini 3.3 V** | RP2040 — Raspberry Pi Pico | ESP32-WROOM — DevKit | ESP32-C3 / C6 — XIAO |
|---|---|---|---|---|---|---|
| Form factor | 21×18 mm | ~20×50 mm | 18×33 mm | 21×51 mm | 28×54 mm | 21×18 mm |
| Flash | 1 MB | 256 KB–1 MB | 32 KB | 2 MB | 4 MB | 4 MB |
| **Dev-board sleep current** | **~1.5–2.4 µA** (System ON, RAM retained) | **~2–4 µA** (STOP2) | **<1 µA** power-down, BOD off | **~1–1.8 mA** (DORMANT, RT6150 SMPS) | mA-class on DevKit (USB-UART + LDO) | ~6–8 µA |
| **Deep-sleep GPIO wake** | **all ~48 pins**, per-pin polarity, LATCH IDs the pin | **any pin, true HW both-edge EXTI** (STOP2) | **all ~20 pins** via PCINT (= both-edge in HW) | any pin, both-edge ✓ | EXT1: 18 RTC pins, **level-only, one global polarity** | **C3: only 6 wake pins · C6: only 8** |
| ≥13-pin wake? | ✓ | ✓ | ✓ | ✓ | ✓ (awkward — mixed idle levels) | ✗ **below 13** |
| Operating voltage | 1.7–3.6 V | 1.7–3.6 V | 1.8–5.5 V | 1.8–5.5 V | 2.3–3.6 V core | 3.0–3.6 V |
| Direct Li-Po feed | Yes (onboard BATT/LDO) | Yes (LDO) | Yes (RAW) | Yes (VSYS) | Yes (VIN) | Yes |
| IR (38 kHz carrier) | PWM peripheral (Nordic *Smart Remote 3* ref design) | timer | timer (IRremote, Daikin weak) | PIO / IRremoteESP8266 (unverified on Pico) | IRremoteESP8266 ✓ | IRremoteESP8266 ✓ |
| Port Daikin frame? | yes — accepted | yes — accepted | yes — accepted | library or port | library | library |
| Extra programmer | No (USB) | No (onboard ST-LINK / USB-DFU) | **Yes — USB-serial adapter** | No | No | No |
| Price | ~€10 | ~€5–8 | ~€3–4 + adapter | €4–6 | €3–5 clone | ~€5 |
| **Verdict** | **best fit** | **excellent** | simplest build, 8-bit | ✗ sleep current | ✗ sleep current (DevKit) | ✗ too few wake pins |

Notes:
- **Dev-board, not die.** The figures above are the *board* numbers, because perfboard-only means we ship a dev board. This is exactly what sinks the RP2040: the die does ~180 µA in DORMANT (already poor), but the Pico *board*'s always-on RT6150 SMPS pulls ~1–1.8 mA. The only way down was a bare-chip PCB — now ruled out by the perfboard constraint. RP2350 / Pico 2 carry the same SMPS class and do **not** improve the board figure.
- **Why ~13-pin wake eliminates the cheap ESP32-C parts** despite their fine ~6 µA sleep: the C3 exposes only 6 RTC-wake GPIO, the C6 only 8 — both below our 13. ESP32 classic EXT1 has enough RTC pins but wakes level-only on a single global polarity, awkward for 13 lines sitting at different idle levels (workable with software re-arm, but no longer attractive once nRF52/STM32 are on the table).
- **Both-edge in deep sleep is mostly emulated, not literal.** nRF52 SENSE, ESP32 EXT1, ATmega INT0/1 are level detectors in their deepest mode; the standard pattern is *sense the opposite of the current level, re-read and re-arm on wake*. Genuine hardware both-edge in a µA mode exists on **STM32L4 STOP2 (EXTI)** and **ATmega PCINT** — a point in their favour but not a deciding one, since the re-arm dance is cheap.

| Ruled out | Reason |
|---|---|
| ATmega4809 — Nano Every | ~31 µA board power-down (much worse than the 328P) and TQFP/board less hand-friendly; no advantage here. |
| ATtiny85 — Digispark | 8 KB flash, ~6 I/O — can't host 13 wake lines or a ported Daikin frame. |
| Bare RP2040 / RP2350 + LDO | ~180 µA achievable but needs a custom QFN PCB — **excluded by the perfboard-only constraint**. |

---

## 3. IR Library Situation — convenience, not a constraint

The Daikin protocol is well-documented (3 frames, 35 bytes, fixed headers, one checksum byte, 38 kHz pulse-distance — see [01](01_IR_protocol_and_mapping.md) and the blafois reverse-engineering). It is **transmit-only** here: build a fixed byte array, compute the checksum, clock it out modulated at 38 kHz. That is a few hundred lines, within the "small, auditable firmware" goal. So an off-the-shelf library is an *accelerator*, not a gate — and the MCU is chosen on power and wake behaviour first, library convenience last.

| Library | Targets | Daikin support | Role here |
|---|---|---|---|
| IRremoteESP8266 | ESP8266, ESP32 | Full (`IRDaikinAC`) | Drop-in *if* we land on ESP32 — but we aren't, on power grounds. Useful as the **reference implementation to port from**. |
| IRremote (Arduino) | AVR, STM32, RP2040, ESP32 | Partial / weak Daikin | Could host the carrier + raw send on AVR/STM32; Daikin frame likely hand-built on top. |
| (hand-port) | nRF52 / STM32 / AVR | — | Build the 35-byte frame + checksum directly; carrier via the MCU's PWM/timer (nRF52840 PWM, STM32 timer, AVR timer). **Accepted approach.** |

**On the nRF52840 specifically:** Nordic's *Smart Remote 3* reference design is itself a low-power IR remote — the PWM peripheral generates the 38 kHz carrier from a RAM buffer with no CPU involvement, which is an ideal match for clocking out a Daikin frame from deep sleep.

---

## 4. Power Supply Notes

All three candidate boards accept direct Li-Po feed (3.0–4.2 V) via their battery/RAW pin and onboard LDO (XIAO BATT pad, STM32 board VIN, Pro Mini RAW). No external regulator needed. **Watch each board's standby leakage on that path** — on µA-class targets a board's own LDO quiescent or a charge-LED can dominate (e.g. the XIAO VBAT path note); bench-measure.

Recommended additions regardless of MCU choice:
- **100 µF bulk capacitor** on the power rail — absorbs the ~100 mA current spike when the IR LED fires, prevents MCU brownout reset
- **TP4056 module with protection IC** (DW01A + FS8205) between battery and MCU — handles over-discharge cutoff and over-current protection

---

## 5. Decision

The reframed constraints (sleep current hard, 13-pin wake hard, dev-board/perfboard-only hard, IR library soft) **invert the previous decision**. RP2040 is out: it can't hit the battery target in a perfboard-able form, and the bare-chip escape hatch is now excluded. The choice is between the three boards that clear both hard power/wake gates.

### Preferred: nRF52840 — Seeed Studio XIAO nRF52840

- **~1.5–2.4 µA board sleep** (System ON, RAM retained) — ~500–1000× below the Pico board, turning "9 weeks, missed" into *years* on a modest cell.
- **All ~48 GPIO wake from sleep**, per-pin polarity, with a LATCH register that tells the ISR which line changed — a clean fit for 13 diode-encoded + button lines at mixed idle levels.
- **Native IR fit** — PWM generates the 38 kHz carrier from RAM (Nordic Smart Remote 3 is exactly this device class).
- Tiny (21×18 mm), USB programming (Arduino / Zephyr), onboard battery management, ~€10.
- **Cost paid:** port the Daikin frame (accepted) and verify the board's battery power path doesn't leak (some XIAO wiring shows ~20 µA via VBAT — fixable; bench-confirm).

### Strong alternative: STM32L4 (WeAct / Nucleo board), STOP2 mode

- **~2–4 µA** in STOP2, the **only candidate with genuine hardware any-pin both-edge EXTI** — no re-arm dance, 13 lines trivial.
- Cheap WeAct boards (~€5–8), onboard ST-LINK or USB-DFU, mature HAL/Arduino-STM32 toolchain.
- Note: STM32 STANDBY/SHUTDOWN (deeper, ~nA) only wakes on ~5 WKUP pins → too few; **use STOP2**, which keeps the full EXTI matrix at low-µA.

### Simplest build: ATmega328P (Pro Mini 3.3 V), power-down + PCINT

- **<1 µA** power-down (BOD disabled), **PCINT = hardware both-edge on all ~20 pins**, DIP/through-hole — the easiest to hand-build and cheapest (~€3–4).
- Costs: needs an external USB-serial adapter to flash; 8-bit with 32 KB flash (ample for a ported Daikin frame but tight if much else grows); must remember to disable BOD or sleep jumps to ~20 µA.

### Ruled out

| Board | Reason |
|---|---|
| RP2040 / RP2350 — Pico, Pico 2 | ~1–1.8 mA board sleep (RT6150 SMPS); bare-chip fix needs a PCB → excluded by perfboard-only. |
| ESP32-WROOM — DevKit | mA-class on a DevKit; bare module (~10 µA) needs custom board, and EXT1 wake is awkward for 13 mixed lines. |
| ESP32-C3 / C6 — XIAO | Great ~6 µA sleep but only 6 / 8 deep-sleep wake pins — **below 13**. |
| ATmega4809 — Nano Every | ~31 µA power-down, no benefit over the 328P. |
| ATtiny85 — Digispark | 8 KB flash, too few I/O for 13 wake lines + ported frame. |

---

## 6. First Prototype Steps

Two things must be proven on the bench before committing the board — and the **order has flipped**: power/wake first, IR second.

**A. Sleep + multi-GPIO wake (the gating risk).** On the chosen board:
1. Put the MCU in its deepest viable sleep (nRF52 System ON / STM32 STOP2 / AVR power-down).
2. Arm ~13 GPIO for both-edge wake (emulated via re-arm where needed); measure board sleep current with a µA meter.
3. Confirm every line wakes the MCU on either edge, including non-zero→non-zero code changes (the diode-encoding case — see [05 §3.5](05_electronics_circuit.md#35-the-chosen-scheme--per-switch-diode-encoding)).
4. Confirm the measured current supports ≥6 months on a realistic cell. This sizes the battery ([05 §6](05_electronics_circuit.md#6-battery-budget)).

**B. IR transmit.** Build and send one Daikin frame (ported, or via a library if the chip has one):
1. Assemble the 35-byte frame for `power on / cool / 22 °C`, compute the checksum, clock it out at 38 kHz through the LED driver.
2. Verify the FTXM20N2V1B responds.

If a board fails **A**, move to the next in the preference order (nRF52840 → STM32L4 → ATmega328P) and repeat — **A** is the decisive test, **B** is portable across all three.
