# Daikin Custom Remote — Project Specifications

## 1. Overview

Build a physical, tangible IR remote control for a Daikin split AC unit. The interface must be "no brain needed": all controls are hardware (rotary switches, buttons), with LED feedback for state. No screen. No complex menus.

## 2. Target Hardware (AC Unit)

| Item | Value |
|---|---|
| Indoor unit | FTXM20N2V1B (2020/06) |
| WiFi adapter | BRP069B41 |
| Firmware | 1.14.68 |
| Protocol regime | Legacy REST (`/aircon/get_control_info`) |
| Original remote | ARC466A33 |
| IR variant | To confirm against IRremoteESP8266 supported list for ARC466A33 |

## 3. Controls & Functions

### 3.1 Primary controls (always accessible)

| Function | Range | Control type | Notes |
|---|---|---|---|
| Power + Mode | OFF / Fan / Cool / Heat / Dry | Rotary selector, 5 positions | One knob covers both on/off and mode |
| Fan speed | 1 / 2 / 3 / 4 / 5 | Rotary selector, 5 positions | |
| Target temperature | 16–26 °C | Rotary selector or encoder | 1 °C steps preferred (11 positions); 2 °C acceptable fallback |

### 3.2 Secondary modes (push buttons)

| Function | Notes |
|---|---|
| Powerful | Momentary or latching push button |
| Night / Quiet | Momentary or latching push button |
| Econo | Momentary or latching push button |

Timer / daily scheduling is **out of scope** for v1 (deemed overkill for the "no brain needed" goal).

### 3.3 Resend

Pressing the temperature knob (if encoder with push) or a dedicated button retransmits the full current state (mode + fan + temp) via IR. Accounts for missed commands.

## 4. Feedback

All state is reflected via LEDs — there is no screen.

| Feedback | Mechanism |
|---|---|
| Active mode | LED per mode position (or WS2812B ring) |
| Fan speed | LED per speed position |
| Temperature | LED bar / arc (one LED per step) |
| IR transmitted | Brief flash on a dedicated TX LED |
| Secondary mode active | LED integrated in or next to each button |

LED count estimate: ~16–20. Preferred approach: WS2812B addressable chain (single GPIO, color-coded per function: blue = cool, red = heat, etc.).

## 5. Microcontroller

**Wi-Fi and Bluetooth: not required.** The device is fully standalone.

**Preferred: Raspberry Pi Pico** (RP2040, no Wi-Fi variant)

Rationale: no ADC conflicts (all pins usable simultaneously), large flash (2 MB+), low power consumption (better for battery), simpler firmware (no Wi-Fi stack), sufficient memory for Daikin IR library.

Alternative: ESP32 DevKit — viable but overkill without Wi-Fi; higher quiescent current hurts battery life.

Arduino Uno/Nano: ruled out (32 KB flash insufficient for full Daikin IR library).

## 6. Input Architecture

### Rotary selectors

Use mechanical rotary selector switches (e.g. Lorlin CK1032 or equivalent 1P12T bridged to fewer positions, or Alpha SR16).

- Mode selector: 5-position (OFF, Fan, Cool, Heat, Dry)
- Fan selector: 5-position
- Temperature: 11-position rotary selector (16–26 °C, 1 °C steps) — Lorlin 1P12T bridged to 11 positions; **or** EC11 encoder with firm detents if 11-position selector is impractical in the enclosure

Wiring: resistor ladder on a single analog pin (no ADC conflict concerns on Pico).

### Push buttons

3–4 momentary or latching buttons for secondary modes. Each wired to a digital GPIO with internal pull-up.

### Available rotary switch (on hand)

Commutateur 8404-3C (code 07206) — 3 circuits, 4 positions (3P4T). Suitable for Mode or Fan selector (only 4 positions, not 5; confirm if Dry mode will be dropped or a different switch used).

## 7. IR Transmission

- Library: **IRremoteESP8266** (ESP32 compatible)
- Class: `IRDaikinAC` (or appropriate variant once remote model is identified)
- IR LED: 940 nm LED + NPN transistor driver (2N2222A or S8050), ~100 mA peak
- Recommended GPIO: 4 (standard for IRremoteESP8266 examples)
- IR receiver (TSOP38238): used during development to capture and verify original remote signals; not needed in final device

## 8. Power

Li-Po battery. The device must work untethered (handheld use case).

- Pico quiescent: ~1–2 mA sleep, ~25 mA active — significantly better than ESP32
- Battery sizing TBD (target: weeks of normal use on a single charge)
- Charging circuit: TP4056 module or similar, exposed via USB-C port on enclosure
- Low-battery indicator LED desirable (nice to have)

## 9. Enclosure

3D-printed. Target footprint: **~80 × 100 × 25 mm** (credit-card footprint, thicker).

Dual-use: designed to work both handheld and wall-mounted (magnetic or screw mount on the back).

Constraint: IR LED must have unobstructed forward-facing window. All rotary controls and buttons on the top face.

## 10. Open Questions

The following still need answers:

1. **Mode count: 4 or 5?** — the 8404-3C on hand is 4-position. Decide: keep Dry mode (needs a 5-position switch) or drop it (use the 8404-3C as-is for OFF/Fan/Cool/Heat)?
2. **Secondary modes: which ones?** — check the ARC466A33 / FTXM20N2V1B for Powerful, Night/Quiet, Econo support. Determines button count.
3. **IR variant for ARC466A33** — cross-check against IRremoteESP8266 supported protocols list. Likely `DAIKIN` or `DAIKIN2`; needs verification.
4. **Temperature control: 11-position Lorlin or EC11 encoder?** — depends on whether an 11-position selector fits the 80×100 mm enclosure layout alongside the other two knobs.
5. **Screen as optional add-on?** — "main functions work without it" implies a screen port could be added later. Confirm: reserve I2C pins on the PCB for a future OLED, or ignore entirely?
