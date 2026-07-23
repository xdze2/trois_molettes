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
| Range used by this project | 14–31 °C, 1 °C steps (14–21 °C heating, 24–31 °C cooling; see [00_specifications.md §4.3](00_specifications.md#43-temperature--8-position-rotary-sr16-mode-dependent-mapping)) |

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

Minimal usage example (reference — IRremoteESP8266 on an ESP32). On the ATmega328P the same `setPower/setMode/setTemp/send` logic is ported by hand, since this library targets ESP only; it remains the cleanest reference for the frame contents:

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

For the current control mapping (Fan/Mode/Temp knob positions → AC state, Send/Swing
buttons, LED feedback), see [00_specifications.md §4](00_specifications.md#4-controls--functions)
and [05_electronics_circuit.md](05_electronics_circuit.md).
