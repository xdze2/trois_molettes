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

Serial output at 2400 baud (sketch requests 4800; CKDIV8 halves it — see
[02_serial_debug.md](02_serial_debug.md)).

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

Timer2 in CTC mode toggles OC2B (D3) at the compare match.  With the chip at
4 MHz effective (CKDIV8) and prescaler /1:

```
f = 4 000 000 / (2 × (OCR2A + 1)) = 4 000 000 / (2 × 53) ≈ 37.7 kHz
```

OCR2A = 52 is set once in `setup()`.  `ir_mark()` reconnects the OC2B toggle;
`ir_space()` disconnects it and holds the pin LOW.

### CKDIV8 timing correction

The IDE compiles for 8 MHz but the chip runs at 4 MHz, so
`delayMicroseconds(N)` executes as approximately `2 N µs`.  Every duration
passed to `delayMicroseconds()` inside `ir_mark()` / `ir_space()` is therefore
halved: `delayMicroseconds(us / 2)`.

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
| AC responds to some frames but not others | Carrier frequency off — try OCR2A = 51 or 53 |
| Garbled serial output | Wrong baud rate in monitor — use 2400 |
| Sketch won't compile | Arduino IDE cannot resolve the `../../firmware/` include path — open the sketch from the repo root or add `firmware/` to the IDE library path |
