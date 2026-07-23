# Software Architecture

How the firmware is structured today. For the hardware context see [05_electronics_circuit.md](05_electronics_circuit.md); for the IR protocol see [Annex A1](A1_IR_protocol_and_mapping.md); for the MCU choice see [03_microcontroller_choice.md](03_microcontroller_choice.md).

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

Each transmit logs three lines to Serial: `CHANGE` (which knobs moved, with both the raw gpio reading `raw=` and the decoded `pos=`/label — so a mis-wired or reversed knob shows as a raw↔pos mismatch), `SEND` (the decoded AC state), then `SENT` (the 35 raw frame bytes in hex). The `SEND`/`SENT` labels are the filter targets used in the [howto 09](../howtos/09_rotary_switches_wake_test.md) capture examples.

Every transmit re-reads and re-derives the full AC state from the knobs — there's no separate "dirty" tracking beyond noticing a switch's decoded position changed since last loop. Sleep + PCINT wake (deep sleep between reads) is a deliberately separate, later step — see [howto 09](../howtos/09_rotary_switches_wake_test.md).

---

## 2. Switch reading

Fan, Mode and Temp are diode-encoded rotary switches, each read as a 3-bit code across 3 GPIO pins (see [05_electronics_circuit.md](05_electronics_circuit.md) for wiring). `readDebouncedCode()` polls a switch's pins every `SETTLE_MS` and requires `SETTLE_STABLE_READS` consecutive identical readings before accepting a value — this rides out the inter-detent glitching described in [howto 03](../howtos/03_rs1010_readout.md).

Raw codes are remapped to **physical positions** per-switch (`rawToPos` table) where the diode wiring doesn't match the left→right detent order — e.g. the Fan switch's encoding is shifted by one position (see [howto 09](../howtos/09_rotary_switches_wake_test.md)). This raw→position step is the *only* place hardware quirks (diode shift, swapped pins, a reversed rotation direction) are corrected; a knob reading reversed is fixed here, not in the position→state tables below.

The Resend button is a separate, simpler edge-triggered debounce (`resendButtonPressed()`) — a single hold-off delay rather than a settle loop, since it's a momentary press, not a detented position.

---

## 3. Position → Daikin state

The full position→meaning mapping lives in **one declarative table per knob** (`FAN_POS`, `MODE_POS`), one row per detent in left→right panel order — so the table reads top-to-bottom exactly as the knob reads left-to-right. Each row holds both the Serial label and the resulting `ACState` value, so the two can't drift apart. `fanMeaning()`/`modeMeaning()` and `applyFanPos()`/`applyModePos()` are thin lookups into these tables (they were previously three separate `switch` statements per knob that had to be kept in sync by hand). To verify a knob against the panel you read a single table top-to-bottom.

- `FAN_POS` — position 0 is Off (`power=false`); 1–5 are fan speeds; 6 is Quiet; 7 is Auto.
- `MODE_POS` — Fan / Cool / Heat / Dry / Auto.
- `applyTempPos()` — stays a formula, not a table: position → °C with a mode-dependent base (14 °C for Heat, 20 °C otherwise) in 2 °C steps, per [00_specifications.md §4.3](00_specifications.md).

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

Timer2 drives a 38 kHz carrier on OC2B (pin D3): `ir_mark()`/`ir_space()` toggle the timer's compare-output enable to key the carrier on and off for the pulse-distance timing the protocol needs (`DAIKIN_BIT_MARK`, `DAIKIN_ONE_SPACE`, etc. — see [Annex A1](A1_IR_protocol_and_mapping.md)). `send_daikin()` clocks out the 3 sections of the 35-byte frame with the required header and inter-section gaps.

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
