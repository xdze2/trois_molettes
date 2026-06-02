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

### 3.7 Timers (out of scope for v1)

The protocol supports on-timer, off-timer, weekly timer, and current time/day. Methods exist but are not used in this project.

---

## 4. Libraries

| Library | Language | Notes |
|---|---|---|
| [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) | C++ (Arduino) | Primary library. Full Daikin support. Use `IRDaikinAC` class. |
| [blafois/Daikin-IR-Reverse](https://github.com/blafois/Daikin-IR-Reverse) | Documentation | Original community reverse-engineering. Frame structure reference. |
| [Arduino-IRremote](https://github.com/Arduino-IRremote/Arduino-IRremote) | C++ (Arduino) | Alternative, less complete Daikin support. Not recommended. |

Minimal usage example (Arduino / Pico with Arduino core):

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

Design principle: **stateless physical controls**. Every knob and switch directly encodes the current AC state — there is no hidden software state. What you see (knob position + LEDs) is what the AC is set to.

### 5.1 Control layout

```
┌─────────────────────────────────────┐
│                                     │
│   [MODE]      [FAN]     [TEMP]      │
│    knob        knob      knob       │
│                                     │
│   ● ● ● ● ●  ● ● ● ● ●  ● ● ● ● ● ● ● ● ● ● ●
│   mode LEDs  fan LEDs       temp LEDs (16–26°C)
│                                     │
│   [PWR]  [ECO]  [SWING]  [RESEND]  │
│                                     │
│                          ◉ IR TX    │
└─────────────────────────────────────┘
```

### 5.2 Mode selector (rotary, 5 positions)

| Position | Label | AC state sent |
|---|---|---|
| 0 | OFF | `setPower(false)` |
| 1 | FAN | `setPower(true)`, `setMode(kDaikinFan)` |
| 2 | COOL | `setPower(true)`, `setMode(kDaikinCool)` |
| 3 | HEAT | `setPower(true)`, `setMode(kDaikinHeat)` |
| 4 | DRY | `setPower(true)`, `setMode(kDaikinDry)` |

LED feedback: one LED per position, lit in mode color (blue = cool, red = heat, green = fan, yellow = dry, off = off).

Note: the 8404-3C on hand (4 positions) covers positions 0–3 (no Dry). A 5-position switch is needed if Dry is required.

### 5.3 Fan speed selector (rotary, 5 positions)

| Position | Label | Value sent |
|---|---|---|
| 0 | AUTO | `kDaikinFanAuto` |
| 1 | 1 | `1` |
| 2 | 2 | `2` |
| 3 | 3 | `3` |
| 4 | 4 | `4` |
| 5 | MAX | `5` |

6 positions total (Auto + 1–5). Alternatively: drop Auto and use positions 1–5 only (simpler, one fewer position).

LED feedback: one LED per position, all same colour.

### 5.4 Temperature selector (rotary, 11 positions)

| Position | °C sent |
|---|---|
| 0 | 16 |
| 1 | 17 |
| … | … |
| 10 | 26 |

A Lorlin 1P12T bridged to 11 positions covers this range exactly. Wired as a resistor ladder on one analog pin.

If the 11-position selector is too large for the enclosure, fall back to an EC11 encoder (infinite rotation, 1-click = 1 °C, push to resend).

LED feedback: 11 LEDs in an arc, one lit per temperature step.

### 5.5 Push buttons

| Button | Function | IR call | LED |
|---|---|---|---|
| PWR | Toggle Powerful mode | `setPowerful(!state)` | Lit when active |
| ECO | Toggle Econo mode | `setEcono(!state)` | Lit when active |
| SWING | Toggle vertical swing | `setSwingVertical(!state)` | Lit when active |
| RESEND | Retransmit full state | `send()` | Brief flash on TX LED |

Powerful and Econo are mutually exclusive — activating one clears the other.

### 5.6 Send trigger

The firmware sends a new IR frame **on every control change** (knob moved to a new position, button pressed). No "confirm" step needed.

The RESEND button re-transmits the current state without changing anything — recovers from missed commands.

### 5.7 LED summary

| Zone | Count | Type | Purpose |
|---|---|---|---|
| Mode | 5 | WS2812B | Active mode colour |
| Fan | 6 | WS2812B | Active speed |
| Temperature | 11 | WS2812B | Active temperature |
| Button indicators | 3 | WS2812B | PWR / ECO / SWING active |
| IR TX | 1 | Simple LED | Blinks on every send |
| **Total** | **26** | | |

All 25 WS2812B LEDs on a single data chain → 1 GPIO pin. IR TX LED on a separate GPIO (direct drive, no transistor needed at indicator current).
