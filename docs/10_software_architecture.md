# Software Architecture

How the firmware is structured today. For the hardware context see [05_electronics_circuit.md](05_electronics_circuit.md); for the IR protocol see [01_IR_protocol_and_mapping.md](01_IR_protocol_and_mapping.md); for the MCU choice see [03_microcontroller_choice.md](03_microcontroller_choice.md).

---

## 1. Overall structure

The firmware ([firmware/daikin_knob_remote/daikin_knob_remote.ino](../firmware/daikin_knob_remote/daikin_knob_remote.ino)) is a flat Arduino sketch, plain polling loop, no sleep and no interrupts yet:

```
setup()
  configure switch/button/LED pins, start Timer2 (38 kHz carrier)

loop()
  read + debounce Fan, Mode, Temp switches
  if any changed        → build frame, transmit, log to Serial
  else if Resend pressed → re-transmit current state
```

Every transmit re-reads and re-derives the full AC state from the knobs — there's no separate "dirty" tracking beyond noticing a switch's decoded position changed since last loop. Sleep + PCINT wake (deep sleep between reads) is a deliberately separate, later step — see [howto 09](../howtos/09_rotary_switches_wake_test.md).

---

## 2. Switch reading

Fan, Mode and Temp are diode-encoded rotary switches, each read as a 3-bit code across 3 GPIO pins (see [05_electronics_circuit.md](05_electronics_circuit.md) for wiring). `readDebouncedCode()` polls a switch's pins every `SETTLE_MS` and requires `SETTLE_STABLE_READS` consecutive identical readings before accepting a value — this rides out the inter-detent glitching described in [howto 03](../howtos/03_rs1010_readout.md).

Raw codes are remapped to logical positions per-switch (`rawToPos` table) where the diode wiring doesn't match the logical order — e.g. the Fan switch's encoding is shifted by one position (see [howto 09](../howtos/09_rotary_switches_wake_test.md)).

The Resend button is a separate, simpler edge-triggered debounce (`resendButtonPressed()`) — a single hold-off delay rather than a settle loop, since it's a momentary press, not a detented position.

---

## 3. Position → Daikin state

Each knob position maps to a field of `ACState` (defined in `daikin_frame.h`):

- `applyFanPos()` — position 0 is Off; 1–5 are fan speeds (wire value = position + 2); 6 is Quiet; 7 is Auto.
- `applyModePos()` — Fan / Cool / Heat / Dry / Auto.
- `applyTempPos()` — position → °C, with a mode-dependent base (14 °C for Heat, 20 °C otherwise) in 2 °C steps, per [00_specifications.md §4.3](00_specifications.md).

Swing, Powerful and Econo aren't set by any knob/button yet — `ACState` is zero-initialized, so all three are always off.

---

## 4. Daikin frame layer

Isolated in [firmware/daikin_frame.h / .cpp](../firmware/daikin_frame.h) — pure C++, no Arduino or library dependency, shared (via symlink) by every sketch that needs it. See [firmware/](../firmware/) for the file layout.

```cpp
struct ACState {
    bool    power;
    uint8_t mode;      // DAIKIN_MODE_*
    uint8_t temp;      // 16–26 °C
    uint8_t fan;       // DAIKIN_FAN_*
    bool    swing_v;
    bool    powerful;
    bool    econo;
};

// Fills frame[35] and computes the three checksums.
void daikin_build_frame(const ACState *state, uint8_t frame[35]);
```

The byte layout and checksum algorithm are taken directly from the `IRDaikinESP` implementation in IRremoteESP8266 — it is the reference. [ir_mock/](../ir_mock/) cross-checks `daikin_build_frame()`'s output against a real captured frame from the ARC466A33 remote (see [howto 07](../howtos/07_verify_frame_against_capture.md)) — the library's own default state isn't trusted as ground truth, since that's what broke initially.

---

## 5. IR transmit

Timer2 drives a 38 kHz carrier on OC2B (pin D3): `ir_mark()`/`ir_space()` toggle the timer's compare-output enable to key the carrier on and off for the pulse-distance timing the protocol needs (`DAIKIN_BIT_MARK`, `DAIKIN_ONE_SPACE`, etc. — see [01_IR_protocol_and_mapping.md](01_IR_protocol_and_mapping.md)). `send_daikin()` clocks out the 3 sections of the 35-byte frame with the required header and inter-section gaps.

---

## 6. Linux mock (`ir_mock/`)

A one-shot comparison test, not a general CLI tool: it builds a frame with `daikin_build_frame()` for a fixed test case and prints it alongside the equivalent `IRDaikinESP` reference frame and the real captured ARC466A33 frame, so a byte-level mismatch is visible directly.

```
$ make -C ir_mock
$ ./ir_mock/daikin_mock
```

---

## 7. Open items

| Item | Status |
|---|---|
| Sleep + PCINT wake in `daikin_knob_remote` | proven separately in [howto 09](../howtos/09_rotary_switches_wake_test.md); not yet merged into the main sketch (currently plain polling) |
| Swing switch | no hardware wired; `swing_v` hardcoded false |
| `ir_mock` CLI flags (`--power`, `--mode`, `--temp`…) | not started — currently a fixed test case |
