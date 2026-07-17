# IR Modulation Test — TSAL6200 + TSOP38238 + Oscilloscope

Bench notes for verifying the IR LED circuit using the receiver as a loopback
and the oscilloscope to observe both the raw carrier and the demodulated output.

See [06_IR_LED_wiring.md](../docs/06_IR_LED_wiring.md) for the circuit design, and
[04_ir_receiver_signal.md](04_ir_receiver_signal.md) for the receiver wiring.

## Step 0 — Sanity check with a visible LED (do this first)

Before trusting *any* result from the IR + TSOP loopback below, verify the
transistor switching with a red LED you can see with the naked eye.

Sketch: [sketches/ir_led_blink/ir_led_blink.ino](../sketches/ir_led_blink/ir_led_blink.ino)
— toggles D3 (and the onboard LED on D13) HIGH/LOW every 1 s. No carrier, no
Timer2, just `digitalWrite()`.

Swap the TSAL6200 for a red LED with a suitable series resistor (e.g. 330 Ω at
3.3 V) in the same socket. Keep everything else identical: same transistor,
same R_base, same GPIO.

Expected:
- Onboard LED (D13) blinks at 1 Hz.
- Red LED blinks **in sync** with the onboard LED.

If the red LED stays **always on** while D13 blinks, the transistor wiring is
wrong — almost certainly a **pinout mix-up**. The S9013 datasheet pinout is
**E – B – C** (centre is base), not E – C – B as some references and an earlier
version of [06_IR_LED_wiring.md](../docs/06_IR_LED_wiring.md) claimed. With base and
collector swapped, current flows VCC → R → LED → base → emitter → GND through
the B-E diode permanently, and the GPIO toggling does almost nothing visible.

While the test runs, measure **V across the 22 Ω series resistor** in the LED ON
phase. Expected ~1.6–1.7 V → I ≈ 75–80 mA through the LED. Anything in that
range confirms the transistor is saturating and current limiting is working as
designed.

Only once this passes should you move on to the modulation test below.

## Goal

Confirm that:
1. The transistor switches at the expected frequency (38 kHz carrier visible on CH2)
2. The TSOP38238 demodulates the burst and produces a clean active-low pulse (CH1)

No Daikin protocol needed — just a single burst every second from a minimal sketch.

## Bench setup

![Bench — IR LED circuit and receiver on breadboard, Pro Mini connected via USB](images/PXL_20260623_154933929_web.jpg)

- Left breadboard: TSOP38238 receiver
- Main breadboard: IR LED + S9013 transistor circuit (see [06_IR_LED_wiring.md](../docs/06_IR_LED_wiring.md))
- ATmega328PB Pro Mini connected via USB–serial adapter
- CH1 (scope, black clip): TSOP38238 OUT pin
- CH2 (scope, grey clip): IR LED GPIO input (D3, base side of transistor circuit)

Point the LED at the receiver from a few centimetres away.

## Sketch

[sketches/ir_modulation_test/ir_modulation_test.ino](../sketches/ir_modulation_test/ir_modulation_test.ino)

Sends one ~475 µs burst at 38 kHz every second. Serial output at 2400 baud
(sketch requests 4800 — halved by CKDIV8, see [02_serial_debug.md](02_serial_debug.md)).

### Timing workaround — CKDIV8 fuse

The chip runs at 4 MHz effective (CKDIV8 fuse active, IDE compiles for 8 MHz).
`delayMicroseconds()` executes 2× slower than requested. The sketch uses
`HALF_PERIOD_US 4` (requested) → ~8 µs actual half-period → ~62 kHz nominal,
but the `digitalWrite()` overhead at 4 MHz adds enough time to land near 36 kHz.

**Measured result: ~36 kHz** — close enough for the TSOP38238 (bandpass centred
at 38 kHz, −3 dB at roughly ±4 kHz). The receiver triggers reliably.

If the receiver stops responding, increase `HALF_PERIOD_US` by 1 and re-flash.

## Oscilloscope

![Scope — CH2 (top): 38 kHz carrier on IR GPIO; CH1 (bottom): TSOP demodulated output](images/PXL_20260623_153547738_web.jpg)

Settings used:
- Timebase: **100 µs/div**
- CH1, CH2: 2 V/div, DC, 10:1 probe
- Trigger: CH2 rising edge, single shot

**CH2 (top):** raw 38 kHz carrier on D3 — ~18 pulses over ~4 divisions (~400 µs burst).

**CH1 (bottom):** TSOP38238 OUT — active-low pulse. Idle HIGH, drops LOW for the
duration of the burst, returns HIGH cleanly. The demodulator output confirms the
LED circuit fires correctly and the receiver sees the signal.

The burst width on CH1 matches CH2 within one or two carrier cycles — expected,
as the TSOP has a short internal propagation delay (~50–100 µs typical).

## ⚠️ False-positive trap — this test passed with the wrong wiring

This loopback test originally passed with the **transistor wired wrong** (base
and collector swapped — see Step 0 above). It still produced a "clean
demodulated burst" on the TSOP38238 output. Don't trust this test in isolation.

What was actually happening:

```
3.3 V → 22 Ω → LED → base → emitter → GND        (always-on, ~76 mA DC)
                          ↑
              GPIO via 2.2 kΩ wiggling the (mis-wired) collector
```

The base-emitter junction was permanently forward-biased, so the LED sat at
~76 mA DC. The GPIO toggling at 38 kHz injected a small current at the
mis-wired collector pin, modulating the BE current by a few percent. Result:
an LED emitting a strong DC IR signal with a **tiny 38 kHz ripple** on top.

A TSOP38238 a few centimetres away does not care. It has automatic gain
control, a 38 kHz bandpass filter, and is designed to detect very weak
signals — a few percent AM modulation at point-blank range demodulates
beautifully. The scope showed a textbook active-low burst.

But against the real Daikin AC unit (~2–3 m away, behind a tinted IR window,
weak coupling), that ~5 % modulation depth is nowhere near enough. A properly
switched circuit cycles the LED between **off and on**, giving 100 %
modulation depth — which is what's needed to survive real-world path loss.

**Lesson:** the TSOP loopback at close range is a *necessary* check, not a
*sufficient* one. Always pair it with the Step 0 visible-LED sanity check, and
ideally a "does the AC actually beep" end-to-end test (howto 08).
