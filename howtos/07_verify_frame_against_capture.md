# Verifying `daikin_build_frame()` Against the Real Remote Capture

`06_ir_rx_dump` captured ten real frames from the ARC466A33 remote
([data/dump_fan_loop.txt](data/dump_fan_loop.txt)).  This howto decodes those
frames back into bytes and diffs them against what `daikin_build_frame()`
produces — the cross-check the `06` howto promised but didn't run.

The headline result: **the bit-level encoding and checksum logic are correct,
but four *fixed* bytes the firmware emits do not match what the real remote
sends.**  Those four bytes are the most likely reason the AC ignored the ATmega
in `08_daikin_fan_toggle`.

## Decoding the dump

Each frame in the dump is `M`/`S` (mark/space) lines split into sections by
`--- nnnnn` gaps and terminated by `===`.  To turn one section into bytes:

1. Skip the leader (6 short marks before the first `---`).
2. For each data section: skip the header `M ~3500 / S ~1700` pair.
3. Read the rest as (bit-mark, space) pairs — `space > 700 µs → 1`, else `0`.
4. Pack 8 bits **LSB-first** into each byte.

That yields exactly 35 bytes per frame (8 + 8 + 19), and **every section
checksum matches** (`sum(bytes) & 0xFF` of each section equals its last byte).
A clean checksum on all ten independently-captured frames is strong evidence the
mark/space→bit decode is right, so the decoded bytes are trustworthy ground
truth.

## The fan-speed loop decodes correctly

The capture cycles `auto → 1 → 2 → 3 → 4 → 5 → quiet → auto`.  Byte 24 (Fan in
the high nibble) tracks it exactly:

| Frame(s) | byte 24 | Fan nibble | Meaning |
|----------|---------|-----------|---------|
| 0, 7     | `A0`    | `A`       | auto    |
| 1, 8     | `B0`    | `B`       | quiet   |
| 2, 9     | `30`    | `3`       | speed 1 |
| 3        | `40`    | `4`       | speed 2 |
| 4        | `50`    | `5`       | speed 3 |
| 5        | `60`    | `6`       | speed 4 |
| 6        | `70`    | `7`       | speed 5 |

This matches the firmware's encoding comment in `firmware/daikin_frame.cpp`
(`auto=0xA, quiet=0xB, speed1..5 = 3..7`).  **Fan encoding and checksums:
confirmed correct.**

## Four fixed bytes don't match

Comparing the real remote's invariant bytes against what
`daikin_build_frame()` hardcodes:

| Byte | Real remote | Firmware emitted | Field |
|------|-------------|------------------|-------|
| 5    | `0x10`      | `0x00`           | section-1 reserved |
| 13   | `0x04`      | `0x00`           | section-2 "clock" |
| 14   | `0x20`      | `0x00`           | section-2 "clock" |
| 31   | `0xC1`      | `0xC0`           | section-3 reserved (bit 0) |
| 32   | `0x80`      | `0x00`           | section-3 reserved (bit 7) |

All five are **invariant across all ten captured frames**, so they are fixed
bits the remote always emits, not state.  Bytes 5, 13, 14, 32 feed their section
checksums, so the firmware also computes
the *wrong* checksums for sections 1 and 2:

- Section 1 — real: `… 10 00 E7`, firmware would emit `… 00 00 D7`.
- Section 2 — real: `… 04 20 78`, firmware would emit `… 00 00 42`.

A Daikin unit validates each section's checksum and rejects the whole frame on a
mismatch — so even a perfect section 3 won't be accepted.  This is almost
certainly why the unit didn't respond in `08_daikin_fan_toggle` (see its open
"Status" note).

## Where do the bugs come from — the library or the port?

**Neither, in the sense of a coding mistake.**  `daikin_build_frame()` is a
*faithful* port of `IRDaikinESP::stateReset()` in
[ir_mock/IRremoteESP8266/src/ir_Daikin.cpp](../ir_mock/IRremoteESP8266/src/ir_Daikin.cpp).
The reference `stateReset()` zeroes the buffer and sets only:

```
raw[0]=0x11 raw[1]=0xDA raw[2]=0x27 raw[4]=0xC5      // section 1
raw[8]=0x11 raw[9]=0xDA raw[10]=0x27 raw[12]=0x42    // section 2
raw[16]=0x11 raw[17]=0xDA raw[18]=0x27 raw[21]=0x49  // section 3
raw[22]=0x1E raw[24]=0xB0 raw[27]=0x06 raw[28]=0x60 raw[31]=0xC0
```

It leaves `raw[5]`, `raw[13]`, `raw[14]`, `raw[32]` at `0x00` and sets
`raw[31]=0xC0` — exactly what the port did.  So the port matched the library
byte-for-byte.

The real cause: **the library's generic default state does not reproduce what
this specific physical remote transmits.**  Bytes 5, 13, 14, and the extra bits
in 31/32 are values the ARC466A33 firmware emits (13/14 are its real-time-clock
field, which the library models as "unused"/zero) that the library's
`stateReset()` never sets.  The library is built to be *decoded from* a real
capture via `setRaw()`, not to regenerate a byte-identical frame from defaults —
and the port inherited that gap.

**Conclusion:** the bug is in trusting the library's *default* state as
ground truth.  The capture is the real ground truth, and the fix is to align the
fixed bytes (and let the checksums follow) to the captured values.

## Applied fix (in `daikin_build_frame()`)

```cpp
frame[5]  = 0x10;   // section-1 reserved, seen in every real capture
frame[13] = 0x04;   // section-2 clock field, low byte
frame[14] = 0x20;   // section-2 clock field, high byte
frame[31] = 0xC1;   // section-3 reserved, bit 0 set on the real remote
frame[32] = 0x80;   // section-3 reserved, bit 7 set on the real remote
```

The section-1/2/3 checksums then follow automatically and match the dump.

## One unresolved difference: byte 21 (power bit)

After the fix, the only remaining mismatch against the capture is **byte 21**:

- capture `0x68` — mode `0b110` (Fan-only), bit3 set, **power bit clear**.
- builder `0x69` — same, but with the power bit (bit 0) **set**.

The capture is *entirely* Fan-mode, so this dump alone can't tell whether the
real remote clears the power bit specifically in Fan mode, or whether power was
simply off during capture.  Changing the builder's power logic on this single
data point risks breaking Cool mode, which no capture covers.  **Left as-is and
flagged; resolve with a Cool-mode capture.**  The section-3 checksum (byte 34)
trivially follows byte 21, so it differs too.

## Note on the state-dependent bytes

The capture was taken in **Fan mode at 25 °C**, not the Cool/22 °C state the
`08` sketch sends, so these legitimately differ and are **not** bugs:

- byte 22 = `0x32` = `25 × 2` = 25 °C.
- byte 24 = `0xA0` = fan auto (see the fan-loop table above).

To diff section 3 properly, drive the builder into the same state as a capture
before comparing — which is exactly what `ir_mock/main.cpp` now does.

## Reproducing

The decode/diff is pure software (no hardware).  `ir_mock/main.cpp` embeds the
decoded capture (`kCapturedFrame`, frame 0 of the dump) as the oracle, drives the
builder into the captured AC state (Fan / 25 °C / fan=auto), and diffs the two:

```bash
cd ir_mock && make && ./daikin_mock
```

It exits non-zero on any *unexpected* mismatch and prints the known byte-21
difference as a `WARN`.  Because the dump is checked in, this guards against
regressions in the frame builder without re-running the hardware capture.
