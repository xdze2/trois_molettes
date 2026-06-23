# IR RX Dump — Reading the Real Remote with the ATmega

The goal is to reverse-engineer the real Daikin remote (ARC466A33) by capturing its
raw IR timings with the ATmega, then comparing them byte-by-byte against the frames
produced by `daikin_build_frame()`.  If the AC unit doesn't respond to the ATmega
transmitter, this diff is the fastest way to find what's wrong.

## Sketch

[sketches/ir_rx_dump/ir_rx_dump.ino](../sketches/ir_rx_dump/ir_rx_dump.ino)

The ATmega listens on the TSOP38238 output (D2) and prints every mark and space
duration to serial as they arrive:

```
M 3648    ← Mark  (IR burst)   duration in µs
S 1620    ← Space (silence)    duration in µs
M 430
S 428
...
---       ← frame separator (silence > 65 ms)
```

**Mark** = IR carrier burst detected (TSOP output LOW).
**Space** = silence between bursts (TSOP output HIGH).

The TSOP already strips the 38 kHz carrier — what arrives at D2 is the demodulated
envelope, so `pulseIn()` measures the protocol-level mark/space directly.

Serial at 9600 baud (faster than the sender sketch to keep serial buffering from
distorting timings mid-frame).

## Wiring

Same TSOP38238 already used for oscilloscope work in `04_ir_receiver_signal`:

```
TSOP OUT (pin 1) ──── D2
TSOP GND (pin 2) ──── GND
TSOP VS  (pin 3) ──── 100 Ω ──── 3.3 V
                              └── 4.7 µF ──── GND
```

## Capturing on macOS

Find the serial device:

```bash
ls /dev/cu.usbserial-*
```

Dump to a file (replace `XXXX` with your device suffix):

```bash
stty -f /dev/cu.usbserial-XXXX 9600 raw && cat /dev/cu.usbserial-XXXX > dump.txt
```

Press a button on the real remote, then Ctrl-C.  Each button press produces one
block of `M`/`S` lines followed by `---`.

## Analysis

A Python script can decode the dump into a byte array:

1. Parse `M`/`S` lines, skip the preamble and section headers.
2. For each bit: mark is always ~428 µs; space < 700 µs → bit 0, space > 700 µs → bit 1.
3. Collect 8 bits LSB-first → one byte.  35 bytes total (Daikin frame length).
4. Compare against `daikin_build_frame()` output from `ir_mock/`.

## What to look for

| Difference | Likely cause |
|------------|-------------|
| Wrong bytes in section 3 only | Frame builder bug (sections 1 & 2 are fixed) |
| Timing values systematically off | `delayMicroseconds()` scaling issue |
| Extra or missing preamble bits | Preamble encoding bug in `send_daikin()` |
| Checksum byte wrong | `daikin_build_frame()` checksum logic |
