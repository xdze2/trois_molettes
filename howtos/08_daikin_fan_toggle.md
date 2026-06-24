# Daikin Fan Toggle — First End-to-End IR Frame Test

First test of a real Daikin frame sent over the IR LED.  The sketch alternates
fan speed between MIN and MAX every 30 s.  If the AC unit responds, the full
chain works: `daikin_build_frame()` → pulse encoder → Timer2 carrier → IR LED → AC.

No oscilloscope needed — the AC unit is the loopback.

## Sketch

[sketches/daikin_fan_toggle/daikin_fan_toggle.ino](../sketches/daikin_fan_toggle/daikin_fan_toggle.ino)

Fixed AC state: Power ON, Mode Cool, Temp 22 °C.  Fan alternates:

| Step | Fan speed |
|------|-----------|
| odd  | MIN (1)   |
| even | MAX (5)   |

Serial output at 4800 baud.

## How it works

### Frame building

`daikin_build_frame()` from `firmware/daikin_frame.cpp` fills the 35-byte
frame (same code verified against the IRremoteESP8266 reference in `ir_mock/`).

### Pulse encoding

`send_daikin()` replicates the 3-section structure from `IRDaikinESP::sendDaikin()`
in the library:

```
5-bit "00000" preamble  →  gap (29 ms)
Section 1  bytes  0.. 7  →  HDR mark/space + 8 bytes + bit-mark + gap
Section 2  bytes  8..15  →  HDR mark/space + 8 bytes + bit-mark + gap
Section 3  bytes 16..34  →  HDR mark/space + 19 bytes + bit-mark + gap
```

Timing constants (from `ir_Daikin.h`):

| Symbol          | Value  |
|-----------------|--------|
| HDR mark        | 3650 µs |
| HDR space       | 1623 µs |
| Bit mark        |  428 µs |
| Zero space      |  428 µs |
| One space       | 1280 µs |
| Inter-section gap | 29 000 µs |

### Carrier generation — Timer2 CTC

Timer2 in CTC mode toggles OC2B (D3) at the compare match.  With F_CPU = 8 MHz
and prescaler /1:

```
f = 8 000 000 / (2 × (OCR2A + 1)) = 8 000 000 / (2 × 105) ≈ 38.1 kHz
```

`OCR2A = 104` is set once in `setup()`.  `ir_mark()` reconnects the OC2B toggle;
`ir_space()` disconnects it and holds the pin LOW.

### Clock note — the CKDIV8 misdiagnosis

Earlier sketches in this repo (and earlier revisions of this one) assumed the
CKDIV8 fuse was active, dividing the 8 MHz internal RC clock down to 4 MHz.
The compensations included:

- `delayMicroseconds(us / 2)` everywhere (to undo a supposed 2× slowdown)
- `Serial.begin(4800)` with the monitor at 2400 baud (same idea)
- `OCR2A` values computed against a 4 MHz F_CPU

A scope measurement of the carrier disproved all of this:

| OCR2A | Predicted (4 MHz) | Predicted (8 MHz) | Measured |
|-------|-------------------|-------------------|----------|
| 52    | 37.7 kHz          | 75.5 kHz          | —        |
| 58    | 33.9 kHz          | 66.7 kHz          | **66.7 kHz** (15 µs period) |

The 8 MHz column matched exactly — so CKDIV8 is **not** active on this board.
All four corrections were undone:

- `delayMicroseconds(us)` — pass the protocol value as-is
- `Serial.begin(4800)` — monitor at 4800 baud
- `OCR2A = 104` — gives 38.1 kHz at 8 MHz

**Lesson for the other sketches:** any of `ir_modulation_test`, `serial_test`,
etc. that assume CKDIV8 will be running 2× too fast.  Re-measure before
trusting their timing.

## Oscilloscope — full frame

- CH1 (scope, black clip): TSOP38238 OUT pin
- CH2 (scope, grey clip): IR LED GPIO input (D3, base side of transistor circuit)

![Scope — CH2 (top): IR GPIO; CH1 (bottom): TSOP demodulated output — full frame wide view](images/PXL_20260623_163335375_web.jpg)

![Scope — CH2 (top): IR GPIO; CH1 (bottom): TSOP demodulated output — full frame detail](images/PXL_20260623_163738574_web.jpg)

The oscilloscope confirms the 3-section structure: preamble burst, inter-section
gaps (~29 ms), and the three HDR mark / data / bit-mark sequences.

## Expected result

The AC unit should beep and change fan speed 30 s after each transmission.
Serial monitor shows:

```
Daikin fan toggle — MIN/MAX every 30 s
Sending fan=MIN (1)
Sending fan=MAX (5)
...
```

## Troubleshooting

| Symptom | Likely cause |
|---------|-------------|
| AC does not respond at all | LED not pointing at AC receiver; check alignment and reduce distance |
| AC responds to some frames but not others | Carrier frequency off — measure on a scope and tweak OCR2A (±1 ≈ ±0.6 kHz) |
| Garbled serial output | Wrong baud rate in monitor — use 4800 |
| Sketch won't compile, missing `daikin_frame.h` | The sketch dir contains symlinks to `../../firmware/daikin_frame.{h,cpp}` — make sure they survived a checkout (Windows clones drop symlinks by default) |

> **Status (2026-06-23):** The frame structure looks correct on the oscilloscope
> but the real Daikin unit does not respond yet.  Investigation ongoing.
