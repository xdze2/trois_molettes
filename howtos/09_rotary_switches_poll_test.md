# Fan + Mode Switches — Bench Readout Test

Bench notes from testing the diode-encoded Fan speed and Mode rotary switches
together, after wiring them per [05_electronics_circuit.md](../05_electronics_circuit.md).
See [03_rs1010_readout.md](03_rs1010_readout.md) for the original single-switch
(Mode) bring-up notes this test builds on.

## Sketch

[sketches/rotary_switches_poll_test/rotary_switches_poll_test.ino](../sketches/rotary_switches_poll_test/rotary_switches_poll_test.ino)

Polls all three switches (Fan, Mode, Temp), debounces each independently, and
prints bits + decoded meaning on any change or every 5 s heartbeat. No sleep,
no IRQ — pure polling, for wiring/mapping bring-up only.

```
uv run --version   # Arduino upload as usual, then:
uv run tools/serial_capture.py /dev/cu.usbserial-XXXX -b 9600
uv run tools/serial_capture.py /dev/cu.usbserial-XXXX -b 9600 -f '*'
```

[tools/serial_capture.py](../tools/serial_capture.py) is the shared
pyserial-based capture helper (since `stty … && cat` doesn't hold baud rate
reliably on macOS). `-f '*'` filters to lines the sketch flags with a leading
`*` — useful once you trust the wiring and just want to watch a knob move.

## Fan speed: diode encoding shifted by one position

Bench result: turning the Fan knob through all 8 detents gave raw codes
`0, 7, 6, 5, 4, 3, 2, 1` instead of the intended `0, 1, 2, ..., 7`. Checking
each end of the switch directly: the leftmost detent (meant to be Off = `000`)
read `001`, and the rightmost (meant to be Auto = `111`) read `000`. Every
position's diode encoding is shifted by one relative to the intended table,
wrapping mod 8 — not a reversed rotation direction, and not a bit-order swap
(both hypotheses were checked against the data and ruled out).

Fixed in firmware with a raw-code → position remap table rather than
re-wiring the diodes:

```c
const uint8_t FAN_RAW_TO_POS[8] = {0, 7, 6, 5, 4, 3, 2, 1};
```

`printReading()` looks up `pos = FAN_RAW_TO_POS[code]` and feeds `pos` (not
the raw code) to `fanMeaning()`. The raw `code=` is still printed alongside
`pos=` for debugging.

## Mode: pins were swapped, not the switch

Bench result: Cool and Heat read correctly, but Auto and Dry did not, and a
resting detent read `code=6`, a value the RS1010 (5 positions) should never
produce. Checking bit patterns against the intended table ruled out both a
bit0/bit2 swap and a debounce artifact (this was confirmed to be a **stable**
reading, not a mid-rotation transient).

Root cause: the Mode code lines were physically wired to the wrong GPIOs —
**A0 and A2 were swapped** relative to the pin table in
[05_electronics_circuit.md §1](../05_electronics_circuit.md). Fixed by
reordering the pins array to match the actual wiring, rather than touching
the board:

```c
{ "Mode", {A2, A1, A0}, nullptr, 0xFF },   // was {A0, A1, A2}
```

After the swap, all 5 positions read exactly per the intended table:
Fan=0, Cool=1, Heat=2, Dry=3, Auto=4.

## Debounce: bump `SETTLE_STABLE_READS` if a position pops briefly

Even after the pin fix, "Fan" (code 0) occasionally flashed briefly between
real Mode positions — the inter-detent float where all lines momentarily read
0 (§5 of the circuit doc). The original debounce accepted a code after just
**two** consecutive agreeing reads, which a lingering transient can still
pass. Requiring more consecutive agreeing reads cleared it up:

```c
const uint8_t SETTLE_STABLE_READS = 5;  // was effectively 2
```

`SETTLE_MS` (10 ms per read) is unchanged; this only extends the minimum
settle time to `SETTLE_STABLE_READS × SETTLE_MS` (50 ms) before a new code is
accepted, still well under any plausible human re-rotation interval.

## Takeaway for bring-up

When a rotary switch reads "almost right," check in this order before
assuming the switch itself is faulty:

1. **Bit order** — read each known position directly and compare against the
   intended table; a real swap shows up as a consistent, stable misread.
2. **Pin assignment** — verify the firmware's GPIO list matches the traced
   wiring, not just the design doc.
3. **Diode shift** — compare the *sequence* of raw codes around the full
   rotation against the intended sequence; a systematic shift (mod N) points
   at diode wiring, not firmware.
4. **Debounce** — only after ruling out the above, treat leftover glitches
   during rotation as settle-window tuning.

## End-to-end: knob change → IR transmit → real unit

[sketches/daikin_knob_remote/daikin_knob_remote.ino](../sketches/daikin_knob_remote/daikin_knob_remote.ino)
combines this bring-up's validated Fan + Mode readout with the IR TX path from
`daikin_serial` / `daikin_fan_toggle`. It's a near-final knob firmware: Temp is
not wired yet (see Next below) so it's held at a fixed 24 °C, but Fan and Mode
are live.

There is **no Send button** in this sketch — every change of either knob
immediately builds a frame via `daikin_build_frame()` and transmits it. The
switch position *is* the state, matching the stateless-device model from
[11_serial_remote_app.md](../11_serial_remote_app.md). An earlier version gated
transmission behind a Send-button press edge; the button wiring was never
actually connected on this bench so nothing was ever sent. Removing the
button requirement and transmitting straight off the change-detect in the
poll loop fixed it — confirmed against the real Daikin unit, e.g.:

```
Fan: pos=1 (Speed 1)
SEND fan=Speed 1 mode=Cool temp=24
SENT 11DA2700C51000E711DA27004204207811DA27000038300...

Fan: pos=3 (Speed 3)
SEND fan=Speed 3 mode=Cool temp=24
SENT 11DA2700C51000E711DA27004204207811DA27000039300...
```

```
uv run tools/serial_capture.py /dev/cu.usbserial-XXXX
uv run tools/serial_capture.py /dev/cu.usbserial-XXXX -f SEND -f SENT
```

## Next

- **Wire the Temperature switch** (D4/D5/D6 per the pin table) and repeat this
  same bring-up: check bit order, pin assignment, and diode sequence before
  trusting the readout. Temp reuses the same SR16 part as Fan, so the same
  off-by-one diode shift is plausible until proven otherwise. Then wire it
  into `daikin_knob_remote` in place of `TEMP_DEFAULT_C`.
- **Sleep mode test**: repeat the above on `rs1010_wiring_test`-style
  `SLEEP_MODE_PWR_DOWN` + PCINT wake, across all three switches' code lines
  plus Send/Swing, per [05_electronics_circuit.md §5](../05_electronics_circuit.md).
- **Battery test**: run from the TP4056 + cell instead of USB power, per
  [07_battery_and_power.md](../07_battery_and_power.md), to confirm reads stay
  reliable and sleep current matches the budget on real battery voltage (not
  a regulated bench supply).
