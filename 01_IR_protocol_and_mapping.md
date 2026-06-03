# IR Protocol & Interface Mapping

## 1. Protocol Identification

| Item | Value |
|---|---|
| Remote model | ARC466A33 |
| Protocol variant | **DAIKIN** (base protocol) |
| IRremoteESP8266 class | `IRDaikinAC` (in `ir_Daikin.h / ir_Daikin.cpp`) |
| Carrier frequency | 38 kHz |

---

## 2. Frame Structure

Each transmission sends **3 frames**, total **35 bytes**:

| Frame | Bytes | Contains |
|---|---|---|
| 1 | 8 | Header / preamble (fixed) |
| 2 | 8 | Header / preamble (fixed) |
| 3 | 19 | All AC state (mode, temp, fan, flags…) |

Each frame ends with a checksum byte. Encoding is **Pulse Distance** (mark duration constant, space duration encodes the bit value).

The library handles frame assembly, checksum computation, and 38 kHz modulation automatically.

---

## 3. Controllable Parameters

### 3.1 Power

| State | Value |
|---|---|
| Off | `false` |
| On | `true` |

Method: `setPower(bool)` / `getPower()`

---

### 3.2 Mode

| Label | Constant | Bits |
|---|---|---|
| Auto | `kDaikinAuto` | `0b000` |
| Dry | `kDaikinDry` | `0b010` |
| Cool | `kDaikinCool` | `0b011` |
| Heat | `kDaikinHeat` | `0b100` |
| Fan only | `kDaikinFan` | `0b110` |

Method: `setMode(uint8_t)` / `getMode()`

---

### 3.3 Temperature

| Item | Value |
|---|---|
| Minimum | 10 °C (`kDaikinMinTemp`) |
| Maximum | 32 °C (`kDaikinMaxTemp`) |
| Step | 1 °C |
| Useful range for this project | 16–26 °C |

Method: `setTemp(uint8_t)` / `getTemp()`

---

### 3.4 Fan Speed

| Label | Constant | Value |
|---|---|---|
| Speed 1 (Min) | `kDaikinFanMin` | `1` |
| Speed 2 | — | `2` |
| Speed 3 (Medium) | `kDaikinFanMed` | `3` |
| Speed 4 | — | `4` |
| Speed 5 (Max) | `kDaikinFanMax` | `5` |
| Auto | `kDaikinFanAuto` | `0b1010` |
| Quiet | `kDaikinFanQuiet` | `0b1011` |

Method: `setFan(uint8_t)` / `getFan()`

Note: "Quiet" fan is a special fan speed value, not a separate boolean flag.

---

### 3.5 Swing

| Axis | On | Off |
|---|---|---|
| Vertical | `0b1111` | `0b0000` |
| Horizontal | `0b1111` | `0b0000` |

Methods: `setSwingVertical(bool)` / `setSwingHorizontal(bool)`

---

### 3.6 Special Modes (boolean flags)

| Mode | Method | Notes |
|---|---|---|
| Powerful | `setPowerful(bool)` | Boost mode — mutually exclusive with Econo and Quiet |
| Econo | `setEcono(bool)` | Energy saving |
| Quiet | `setQuiet(bool)` | Low noise — mutually exclusive with Powerful |
| Comfort | `setComfort(bool)` | Adjusts airflow direction automatically |
| Mold prevention | `setMold(bool)` | Runs fan after cooling to dry the coil |
| Sensor | `setSensor(bool)` | Intelligent eye sensor (unit-dependent) |

---

### 3.7 Timers (out of scope)

The protocol supports on-timer, off-timer, weekly timer, and current time/day. Methods exist but are not used in this project. Scheduling is delegated to the BRP069B41 WiFi adapter and home automation — see specs for rationale.

---

## 4. Libraries

| Library | Language | Notes |
|---|---|---|
| [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) | C++ (Arduino) | Primary library. Full Daikin support. Use `IRDaikinAC` class. |
| [blafois/Daikin-IR-Reverse](https://github.com/blafois/Daikin-IR-Reverse) | Documentation | Original community reverse-engineering. Frame structure reference. |
| [Arduino-IRremote](https://github.com/Arduino-IRremote/Arduino-IRremote) | C++ (Arduino) | Alternative, less complete Daikin support. Not recommended. |

Minimal usage example (reference — IRremoteESP8266 on an ESP32). On the chosen nRF52840 / STM32L4 / AVR the same `setPower/setMode/setTemp/send` logic is ported by hand, since this library targets ESP only; it remains the cleanest reference for the frame contents:

```cpp
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Daikin.h>

const uint16_t kIrLedPin = 4;
IRDaikinAC ac(kIrLedPin);

void setup() {
  ac.begin();
  ac.setPower(true);
  ac.setMode(kDaikinCool);
  ac.setTemp(22);
  ac.setFan(kDaikinFanAuto);
  ac.setSwingVertical(true);
  ac.send();
}
```

---

## 5. Tangible Interface Mapping

Design principle: **stateless physical controls**. Every knob and switch directly encodes the current AC state — there is no hidden software state. What you see (knob position) is what the AC is set to.

Knobs are ordered by frequency of use: Fan (daily) → Mode (seasonal) → Temperature (set-and-forget).

### 5.1 Control layout

```
┌─────────────────────────────────────┐
│                                     │
│   [FAN]      [MODE]     [TEMP]      │
│    knob       knob       knob       │
│                                     │
│   [PWR]  [ECO]  [SWING]  [RESEND]  │
│                                     │
│                          ◉ IR TX    │
└─────────────────────────────────────┘
```

### 5.2 Fan speed / Power selector (rotary, 6 positions)

This is the primary daily control. Position 0 is OFF.

| Position | Label | AC state sent |
|---|---|---|
| 0 | OFF | `setPower(false)` |
| 1 | 1 | `setPower(true)`, `setFan(kDaikinFanMin)` |
| 2 | 2 | `setPower(true)`, `setFan(2)` |
| 3 | 3 | `setPower(true)`, `setFan(kDaikinFanMed)` |
| 4 | 4 | `setPower(true)`, `setFan(4)` |
| 5 | MAX | `setPower(true)`, `setFan(kDaikinFanMax)` |

Feedback: none per position — the knob position is the state. Only the shared TX LED flashes on send.

Note: `kDaikinFanAuto` is not exposed — it adds a position and is rarely used in practice.

### 5.3 Mode selector (rotary, 4 positions)

Set seasonally. Does not include OFF (handled by the fan knob).

| Position | Label | AC state sent |
|---|---|---|
| 0 | FAN | `setMode(kDaikinFan)` |
| 1 | COOL | `setMode(kDaikinCool)` |
| 2 | HEAT | `setMode(kDaikinHeat)` |
| 3 | DRY | `setMode(kDaikinDry)` |

Feedback: none per position — the knob position is the state.

Note: the 8404-3C on hand is 4 positions — matches exactly (FAN / COOL / HEAT / DRY). If Dry is dropped, 3 positions suffice.

### 5.4 Temperature selector (rotary, 11 positions)

| Position | °C sent |
|---|---|
| 0 | 16 |
| 1 | 17 |
| … | … |
| 10 | 26 |

A 2P12T switch bridged to 11 positions covers this range exactly. One pole wired as a resistor ladder on one analog pin; the second pole provides the alternating-contact wake edge.

If the 11-position selector is too large for the enclosure, fall back to a **10-position switch** mapped to 16–25 °C (dropping 26 °C) — see [04_rotary_switch_choice.md](04_rotary_switch_choice.md#temperature-range-with-10-positions). An EC11 incremental encoder is **not** a valid fallback: its output is relative, requiring stored software state, which violates the stateless principle and loses position across deep sleep / battery removal.

Feedback: none per position — the knob position is the state.

### 5.5 Push buttons

| Button | Function | IR call |
|---|---|---|
| PWR | Toggle Powerful mode | `setPowerful(!state)` |
| ECO | Toggle Econo mode | `setEcono(!state)` |
| SWING | Toggle vertical swing | `setSwingVertical(!state)` |
| RESEND | Retransmit full state | `send()` |

Powerful and Econo are mutually exclusive — activating one clears the other.

The button toggle states (PWR / ECO / SWING) are the **only** internal software state in the device — they have no absolute physical position to read back, unlike the knobs. Every send flashes the shared TX LED.

Note: Quiet is a fan speed value (`kDaikinFanQuiet`), not a boolean flag. It is not exposed as a button — it would conflict with the fan knob's stateless readout.

### 5.6 Send trigger

The firmware sends a new IR frame **on every control change** (knob moved to a new position, button pressed). No "confirm" step needed.

The RESEND button re-transmits the current state without changing anything — recovers from missed commands.

### 5.7 LED summary

| Zone | Count | Type | Purpose |
|---|---|---|---|
| IR TX | 1 | Simple LED | Blinks on every send; confirms battery is live |
| **Total** | **1** | | |

A single TX indicator LED, direct GPIO drive (no transistor needed at indicator current). The per-position WS2812B chain was dropped — incompatible with the 6-month battery target (~15 mA quiescent for ~25 LEDs) and redundant with the stateless "knob position = state" principle. See [01_technical_design_overview.md §5](01_technical_design_overview.md).
