# Software Architecture

How the firmware is structured, how it maps to the hardware, and how the Linux mock fits in. For the hardware context see [05_electronics_circuit.md](05_electronics_circuit.md); for the IR protocol see [01_IR_protocol_and_mapping.md](01_IR_protocol_and_mapping.md); for the MCU choice see [03_microcontroller_choice.md](03_microcontroller_choice.md).

---

## 1. Overall structure

The firmware is a single-pass event loop with no RTOS. The MCU sleeps almost all the time; a GPIO edge (knob moved, button pressed) wakes it, the ISR records which line changed, and the main loop reads all inputs, builds the Daikin frame, and sends it.

```
┌──────────────────────────────────────────────────────┐
│                     main loop                        │
│                                                      │
│  sleep  ──► wake (GPIO edge) ──► read_inputs()       │
│                                      │               │
│                               build_frame()          │
│                                      │               │
│                               send_ir()              │
│                                      │               │
│                               flash_led()            │
│                                      │               │
│                                   sleep              │
└──────────────────────────────────────────────────────┘
```

Every wake cycle is identical regardless of which input triggered it — the full state is always re-read from hardware and re-sent.

---

## 2. Layers

The code is split into three layers so the logic can be tested on Linux independently of the hardware.

```
┌─────────────────────────────────┐
│           app layer             │  main.cpp
│  read inputs → AC state → send  │
├─────────────────────────────────┤
│          daikin layer           │  daikin_frame.h / .cpp
│  AC state → 35-byte frame       │
│  checksum, constants            │
├────────────────┬────────────────┤
│   HAL (AVR)    │   HAL (Linux)  │  hal_avr.cpp / hal_linux.cpp
│  Timer2 38kHz  │  IRsendTest    │
│  GPIO read     │  stdout dump   │
│  deep sleep    │  (no-op)       │
└────────────────┴────────────────┘
```

The **daikin layer** is pure C++ with no hardware dependencies — it builds and checksums the 35-byte frame. It compiles and runs identically on both targets.

The **HAL** is the only part that differs between targets. It provides three functions:

```cpp
void     hal_send_ir(const uint8_t* frame, uint16_t len);
uint16_t hal_read_inputs(void);   // returns a bitmask of raw GPIO lines
void     hal_sleep(void);
```

---

## 3. Input reading — GPIO → AC state (TBD)

The exact GPIO assignment (which pins carry which rotary code bits) is not yet decided — it depends on the final PCB layout and which MCU is chosen. The mapping lives in one place:

```cpp
// inputs.h  — edit this when the pin assignment is finalised
namespace inputs {
    // Fan/Power switch: N-bit Gray or binary code on these GPIO indices
    constexpr uint8_t FAN_BITS[]  = { /* TBD */ };
    // Mode switch
    constexpr uint8_t MODE_BITS[] = { /* TBD */ };
    // Temperature switch
    constexpr uint8_t TEMP_BITS[] = { /* TBD */ };
    // Buttons
    constexpr uint8_t BTN_POWERFUL = /* TBD */;
    constexpr uint8_t BTN_ECONO    = /* TBD */;
    constexpr uint8_t BTN_SWING    = /* TBD */;
    constexpr uint8_t BTN_RESEND   = /* TBD */;
}
```

`hal_read_inputs()` returns a raw bitmask of all GPIO lines. A decode function in the app layer extracts the rotary positions from that bitmask using the constants above, then maps them to AC state:

```
raw GPIO bitmask
      │
      ▼
decode_inputs()   ← uses inputs.h constants
      │
      ▼
struct ACState { power, mode, temp, fan, powerful, econo, swing }
      │
      ▼
build_frame()     ← daikin layer
      │
      ▼
uint8_t frame[35]
```

The button toggles (Powerful, Econo, Swing) are the only internal software state — they have no absolute physical position. They are stored in a small struct that persists across sleep cycles (RAM retained in the chosen MCUs).

---

## 4. Daikin frame layer

Isolated in `daikin_frame.h / daikin_frame.cpp`. No Arduino, no library dependency.

```cpp
struct ACState {
    bool    power;
    uint8_t mode;      // kDaikinCool, kDaikinHeat, kDaikinFan, kDaikinDry
    uint8_t temp;      // 16–26
    uint8_t fan;       // kDaikinFanMin..Max, kDaikinFanAuto
    bool    powerful;
    bool    econo;
    bool    swing_v;
};

// Fills frame[35] and apputes the three checksums.
void daikin_build_frame(const ACState& state, uint8_t frame[35]);
```

The byte layout and checksum algorithm are taken directly from the `IRDaikinESP` implementation in IRremoteESP8266 — it is the reference. The Linux mock is used to verify that `daikin_build_frame()` produces identical bytes.

---

## 5. HAL — AVR implementation

On the ATmega328P (or nRF52840 equivalent):

**`hal_send_ir(frame, len)`**

Calls `sendDaikin()` — the transmit loop that alternates `mark()` / `space()` calls:
- `mark(us)`: enable Timer2 CTC toggle on OC2B (pin D3) for `us` microseconds → 38 kHz burst
- `space(us)`: disable toggle, pin low, `_delay_us(us)`

Timer2 setup: prescaler /1, OCR2A = 104 → 38.1 kHz (see [10 §IR modulation](#) and [03_microcontroller_choice.md](03_microcontroller_choice.md)).

**`hal_read_inputs()`**

Reads all relevant GPIO pins and packs them into a `uint16_t` bitmask. The bit positions match the indices in `inputs.h`.

**`hal_sleep()`**

Puts the MCU into its deepest viable sleep with all relevant GPIO lines armed for both-edge wake. On AVR: power-down mode + PCINT on all input pins, BOD disabled.

---

## 6. HAL — Linux implementation

The Linux HAL lives in `ir_mock/` and is used for two purposes:

1. **Verify the frame builder** — call `daikin_build_frame()` and compare the 35 bytes against `IRDaikinESP::getRaw()` from the full library. Any mismatch is a bug in the port.
2. **Inspect pulse timings** — feed the frame to `IRsendTest::sendDaikin()` and print the pulse/space sequence.

```cpp
// hal_linux.cpp
void hal_send_ir(const uint8_t* frame, uint16_t len) {
    IRsendTest irsend(0);
    irsend.begin();
    irsend.sendDaikin(frame, len);
    std::cout << irsend.outputStr() << "\n";
}

uint16_t hal_read_inputs(void) {
    // Inputs are injected via argv or stdin for automated testing
    return parse_test_inputs();
}

void hal_sleep(void) { /* no-op */ }
```

The test workflow:

```
$ make -C ir_mock
$ ./ir_mock/daikin_mock --power on --mode cool --temp 22 --fan auto
# prints frame bytes + pulse timings
# compare bytes against IRDaikinESP reference
```

---

## 7. File layout

```
ir_mock/
  IRremoteESP8266/     ← git submodule (reference + Linux test HAL)
  main.cpp             ← Linux entry point
  hal_linux.cpp        ← Linux HAL
  Makefile

firmware/
  main.cpp             ← AVR/nRF entry point (same app logic as Linux)
  daikin_frame.h
  daikin_frame.cpp     ← portable Daikin frame builder
  inputs.h             ← GPIO pin assignment (TBD)
  hal_avr.cpp          ← AVR HAL (Timer2, GPIO, sleep)
```

The `main.cpp` app logic is shared between Linux and firmware builds via `#include` or a common source file — the only compile-time difference is which HAL is linked.

---

## 8. Open items

| Item | Status |
|---|---|
| GPIO pin assignment (`inputs.h`) | TBD — depends on final MCU and board layout |
| Rotary code scheme (binary vs Gray) | TBD — follows from circuit §3 decision |
| `daikin_build_frame()` port and verification against library | not started |
| AVR HAL (`hal_avr.cpp`) — Timer2 + PCINT | not started |
| CLI flags for `ir_mock` (--power, --mode, --temp…) | not started |
