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
LEADER  6 pulses
gap     25432 us
SECTION 1
M 3492     <- section header mark (~3.5 ms)
S 1266     <- section header space (~1.7 ms)
M 454      <- data bit mark  (~450 us, always)
S 402      <- data bit space (<700 us -> bit 0, >700 us -> bit 1)
...
--- 34800  <- inter-section gap (~35 ms)
SECTION 2
...
===        <- end of full frame (gap > 60 ms)
```

A full Daikin frame has 3 sections separated by `---`; the whole frame ends
with `===`.

**Mark** = IR carrier burst detected (TSOP output LOW).
**Space** = silence between bursts (TSOP output HIGH).

The TSOP already strips the 38 kHz carrier — what arrives at D2 is the demodulated
envelope.

Serial at **115200 baud**.  `setup()` clears the AVR clock prescaler at runtime
(`CLKPR = 0`) so the chip always runs at its nominal 8 MHz regardless of the
CKDIV8 fuse state.

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

`stty … && cat` does **not** work reliably on macOS — `cat` reopens the device
with default termios and resets the baud rate stty just set, so you get nothing
or garbage.  Use the pyserial helper script (replace `XXXX` with your device
suffix):

```bash
uv run sketches/ir_rx_dump/capture.py /dev/cu.usbserial-XXXX
uv run sketches/ir_rx_dump/capture.py /dev/cu.usbserial-XXXX -o dump.txt
```

The script is a self-contained [uv](https://docs.astral.sh/uv/) script (PEP 723
inline metadata pulls in `pyserial`), defaults to 115200 baud, and prints each
line as it arrives.  Press a button on the real remote, then Ctrl-C.  Each
button press produces one full frame ending with `===`.

## Implementation status — timing problem unsolved

**The sketch does not yet produce reliable output.**  After multiple iterations,
the core timing problem remains: the ATmega cannot accurately measure both marks
and spaces in a polling loop at 8 MHz.

### What was tried and why it failed

**`pulseIn(HIGH)` for spaces** — misses spaces already in progress.
`pulseIn(HIGH, timeout)` waits for a LOW→HIGH transition before starting its
timer.  When called right after a mark ends the pin is already HIGH, so it skips
the current space and waits for the next one.  The 35 ms inter-section gap gets
silently consumed, and the next header space (~1.3 ms) is returned instead.

**`Serial.print()` between mark and space** compounds the miss.  At 9600 baud
a single `M 454\n` line takes ~6 ms — longer than a data-bit space (~400 µs).
`pulseIn(HIGH)` called after that print arrives after the space is already over.
Raising baud to 115200 cuts this to ~0.5 ms but does not eliminate it.

**`micros()`-based polling loop for spaces** — wrong values at 8 MHz.
Replacing `pulseIn(HIGH)` with a `while (digitalRead() == HIGH)` loop timed by
`micros()` avoids the transition-wait problem, but `micros()` has 8 µs resolution
at 8 MHz (Timer0 ticks every 8 µs).  The `digitalRead()` call itself takes ~4 µs,
so the loop misses short transitions and produces spurious 8/16/24 µs pulses for
what should be clean 450 µs marks.

**Hybrid `pulseIn(LOW)` + `micros()` delta for spaces** — timing drift.
Using `pulseIn(LOW)` for mark duration and `micros()` deltas for space duration
(computed as elapsed time minus the next mark) gives marks in the right range but
the space calculation drifts: `(t1 − t0) − next_mark` subtracts a mark that was
already measured after `pulseIn` returned, overcorrecting and producing incorrect
space values.  Sections 2 and 3 are never reached.

### Root cause

The Daikin protocol has two irreconcilable timing requirements for a polling loop:

- Data-bit spaces: **~400 µs** — must be caught within microseconds of the mark
  ending, before the next mark starts.
- Inter-section gaps: **~35 ms** — 87× longer, must not be mistaken for a timeout.

Any `Serial.print`, interrupt, or function-call overhead between measuring a mark
and starting to measure the space risks missing the short spaces.  There is no safe
place to call `Serial.print` in the hot loop.

### Recommended fix for next session

Use a **pin-change interrupt (PCINT) to timestamp every edge** into a ring buffer,
then decode and print from `loop()` once the full frame is captured.

```cpp
volatile uint32_t timestamps[256];
volatile uint8_t  edge_count = 0;

ISR(PCINT2_vect) {
    timestamps[edge_count++] = micros();  // ~1 µs, no Serial, no missed edges
}
```

**How to know the frame is complete:** `loop()` polls `edge_count`.  When it has
been non-zero but stops growing for longer than `FRAME_GAP` (60 ms), the frame is
done.  There is no need to detect inter-section gaps in real time — the Daikin
frame structure is fixed (3 sections, 2 × ~35 ms inter-section gaps, 1 × >60 ms
end gap), so the decode pass can identify sections by gap size after the fact.

The buffer needs ~200 entries for a full Daikin frame (leader + 3 sections of
~20–40 bits each, 2 edges per bit).  `uint32_t timestamps[256]` fits in the
ATmega's 2 KB SRAM with room to spare.

**Print order:** after the end-of-frame timeout, disable the ISR, decode the
timestamp array into mark/space durations, print the full labelled output, then
re-enable the ISR.  All Serial output happens during inter-frame silence — no
timing pressure at all.

## Analysis (once capture works)

A Python script can decode the dump into a byte array:

1. Parse `M`/`S` lines; skip `LEADER`, `gap`, `SECTION`, `---`, `===` lines.
2. Skip the first `M`/`S` pair of each section (the header mark/space).
3. For each bit: space < 700 µs → bit 0, space > 700 µs → bit 1.
4. Collect 8 bits LSB-first → one byte.  35 bytes total (Daikin frame length).
5. Compare against `daikin_build_frame()` output from `ir_mock/`.

## What to look for

| Difference | Likely cause |
|------------|-------------|
| Wrong bytes in section 3 only | Frame builder bug (sections 1 & 2 are fixed) |
| Timing values systematically off | `delayMicroseconds()` scaling issue |
| Extra or missing preamble bits | Preamble encoding bug in `send_daikin()` |
| Checksum byte wrong | `daikin_build_frame()` checksum logic |
