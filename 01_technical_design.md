# Daikin Custom Remote — Technical Design

This document captures implementation choices. For requirements, see [00_specifications.md](00_specifications.md).

---

## 1. IR Protocol

| Item | Value |
|---|---|
| Remote model | ARC466A33 |
| Protocol variant | DAIKIN (base protocol) |
| Library | [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) |
| Class | `IRDaikinAC` (`ir_Daikin.h`) |
| Carrier frequency | 38 kHz |

Each transmission sends 3 frames (35 bytes total). The library handles frame assembly, checksum, and modulation. See [01_IR_protocol_and_mapping.md](01_IR_protocol_and_mapping.md) for full parameter mapping.

IR LED: 940 nm LED + NPN transistor driver (2N2222A or S8050), ~100 mA peak. Recommended GPIO: pin 4 (IRremoteESP8266 default).

---

## 2. Microcontroller

**Preferred: Raspberry Pi Pico (RP2040, genuine)**

| Property | Value |
|---|---|
| Sleep current | ~1–2 mA |
| Flash | 2 MB |
| Power input | Direct Li-Po on VSYS pin (3.0–4.2 V) |
| ADC pins available | ADC0–ADC2, all usable simultaneously |

Rationale: best sleep current of realistic candidates, ample flash, no ADC conflicts, inexpensive genuine board (€4–6 EU distributor).

**Fallback: ESP32 DevKit clone (~€3–5)**
IRremoteESP8266 is natively supported and well tested. Higher sleep current (~10–20 mA) hurts battery life. Use if Pico + IRremoteESP8266 combination fails to validate.

**Ruled out:**

| Board | Reason |
|---|---|
| Arduino Nano (ATmega328P) | No proper sleep mode, ~15–20 mA quiescent |
| Arduino Nano Every (ATmega4809) | Better sleep but no clone advantage over Pico |
| ATtiny85 / Digispark | 8 KB flash — Daikin IR library requires full reimplementation |

**First validation step (before any assembly):** flash a minimal sketch on the Pico using the Arduino-Pico core, include `IRDaikinAC`, send a hardcoded command, confirm the AC responds. If it fails, switch to ESP32.

See [03_microcontroller_choice.md](03_microcontroller_choice.md) for full comparison table.

---

## 3. Input — Rotary Switches

Three selectors, each wired with **two poles**: one for ADC readout (resistor ladder), one for wake-from-sleep detection (GPIO edge on wiper break). All switches must be 2P variants.

| Selector | Positions | Preferred part | Body | Fallback |
|---|---|---|---|---|
| Fan / Power | 6 (OFF + 1–5) | Alpha SR1712F 2P8T (bridged to 6) | 17 mm | — |
| Mode | 4 (Fan/Cool/Heat/Dry) | 8404-3C on hand (3P4T, spare poles unused) | unknown | RS1010 2P4T |
| Temperature | 11 (16–26 °C) | Alpha SR1610 2P12T (bridged to 11) | 16 mm | Lorlin CK1032 2P bridged to 11 (27.5 mm, bulkier) |

**Resistor ladder (pole 1):** 10 kΩ per step, powered from regulated 3.3 V rail. ~300 mV spacing per step for 11-position ladder — comfortable margin above ADC noise floor. Do not power from VSYS (battery voltage varies and would shift all readings).

**Wake detection (pole 2):** all contacts of the second pole shorted to 3.3 V, wiper to GPIO with pull-down. Knob movement breaks contact → falling edge → MCU wakes from sleep.

Pin budget: 3 ADC pins + 3 digital GPIOs for rotary switches. Well within RP2040's 26 GPIOs.

**Sleep is non-negotiable.** Without sleep the MCU draws ~25 mA continuously → ~80 hours on a 2000 mAh cell. The 6-month target requires sleep current (~1–2 mA on RP2040). "Always-on" is not a viable alternative.

**Fallback if 2P switches are unavailable:** RC differentiator (cap + resistor) on the ADC wiper produces a spike on position change, detected via a comparator IC. Spike amplitude (~300 mV) is below GPIO logic threshold so a bare GPIO cannot be used — a comparator with adjustable threshold is required. More complex and costly than the 2P switch approach. See [04_rotary_switch_choice.md](04_rotary_switch_choice.md) for details.

**Grayhill 56SP12 ruled out** — ~€30 per unit, exceeds total BOM budget of €35.

**8404-3C (on hand):** 3P4T — one pole for ADC ladder, one for wake detection, one spare.

**Temperature range with 10-position switch:** 16–25 °C (dropping 26 °C) is acceptable if an 11-position part doesn't fit.

See [04_rotary_switch_choice.md](04_rotary_switch_choice.md) for full comparison and images.

---

## 4. Input — Push Buttons

3–4 momentary buttons for secondary modes. Each wired to a digital GPIO with internal pull-up.

| Button | Function | IR call | Constraint |
|---|---|---|---|
| Powerful | Toggle boost | `setPowerful(!state)` | Mutually exclusive with Econo |
| Econo | Toggle energy saving | `setEcono(!state)` | Mutually exclusive with Powerful |
| Swing | Toggle vertical swing | `setSwingVertical(!state)` | — |
| Resend | Retransmit current state | `send()` | No state change |

Button states (Powerful, Econo, Swing) are the only internal software state — everything else is read directly from the rotary switch positions.

Note: Quiet is a fan speed value in the IR protocol (`kDaikinFanQuiet`), not a boolean flag. It is not exposed here — it would conflict with the fan knob.

---

## 5. Feedback — LED

Minimal feedback, consistent with the stateless interface principle.

**Minimum viable (spec requirement):**
- 1 LED: blinks on IR transmission, confirms battery is live.

**Extended option:**
- WS2812B chain: 5 mode + 5–6 fan + 11 temperature + 3 button state = ~25 LEDs on 1 GPIO.
- This contradicts the stateless principle for buttons (which have internal state) but is optional visual reinforcement.

Decision: start with the single TX LED. Add WS2812B chain only if the single LED feels insufficient during use.

---

## 6. Power

- **Battery:** Li-Po single cell (3.7 V nominal, 3.0–4.2 V range).
- **Charging:** TP4056 module with protection IC (DW01A + FS8205), exposed via USB-C port.
- **Bulk capacitor:** 100 µF on the power rail to absorb the ~100 mA IR LED spike and prevent MCU brownout.
- **Battery sizing:** TBD — driven by 6-month target. With Pico at ~1–2 mA sleep and ~25 mA active (brief pulses only), a 1000–2000 mAh cell is likely sufficient.
- **Low-battery LED:** nice to have.

---

## 7. Send Trigger

The firmware sends a new IR frame on every control change (knob moved, button pressed). No "confirm" step. The Resend button retransmits the current state without changing it.

---

## 8. Open Questions

1. **IRremoteESP8266 on Pico** — must validate before committing to RP2040.
2. **Secondary modes supported by FTXM20N2V1B** — confirm Powerful / Econo / Swing via IR test.
3. **Temperature selector: 11-position 2P part** — confirm enclosure fit with three knobs side by side; source Alpha SR1610 2P12T.
4. **LED scope** — single TX LED or full WS2812B chain?
5. **ADC ladder margins** — verify 11-position ladder on Pico 12-bit ADC on bench.
6. **BOM cost** — hard limit €35 total. Source check on Alpha SR1610 2P and SR1712F 2P (Tayda / Mouser / LCSC).
