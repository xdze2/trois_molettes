# 11 — Serial Remote App (Python Textual TUI)

A **test/demo** soft front-panel for the AC: a [Textual](https://textual.textualize.io/)
terminal UI that mirrors the physical controls (Fan / Mode / Temp / Send) and drives
the **real** Daikin unit over USB serial → the ATmega328P → its IR LED.

This is a bench tool for exercising the IR path from a keyboard, not a replacement
for the hardware remote. It reuses the exact, validated transmit path:
`daikin_build_frame()` → `send_daikin()` → Timer2 38 kHz carrier → IR LED (proven
end-to-end in [howtos/08_daikin_fan_toggle.md](../howtos/08_daikin_fan_toggle.md)).

The 328P stays stateless — like the future knob firmware: read inputs, build a
frame, transmit. Python owns the UI state; the serial link is a single
`SEND <fields>` line carrying a full snapshot. No state machine on either side.

---

## 1. Architecture

```
┌─────────────────────┐     USB serial (115200)     ┌──────────────────────┐
│  Textual TUI (PC)   │  ── line commands ───────►  │  ATmega328P          │
│                     │                             │  daikin_serial sketch│
│  Fan / Mode / Temp  │  ◄── OK / ERR / SENT ────   │                      │
│  Send · TX log      │                             │  daikin_build_frame  │
└─────────────────────┘                             │  send_daikin → IR LED│
                                                     └──────────┬───────────┘
                                                                │ 940 nm IR
                                                                ▼
                                                            Daikin FTXM20N2V1B
```

**Division of responsibility — stateless 328P, stateful Python:**

- The 328P owns the IR protocol and nothing else: frame bytes, fixed/reserved bytes,
  the three section checksums, the pulse structure, the real-capture gap timings —
  already written and validated in C, must not be duplicated in Python.
- The 328P holds no state between lines. Each `SEND` line carries a full snapshot
  (fan, mode, temp, swing); the firmware parses, builds the frame, transmits, replies.
  This mirrors the future knob firmware: knobs *are* the state, the MCU just reads
  them on Send-press.
- Python owns the UI state. Widgets are the source of truth; pressing **Send** ships
  the current widget snapshot in one line.

This matches the stateless-interface principle of the hardware
([00_specifications.md §3](00_specifications.md)): the controls *are* the state.

---

## 2. Why "ATmega as IR transmitter" (vs. the alternatives)

| Option | Where the frame is built | Verdict |
|---|---|---|
| **328P transmitter (chosen)** | C, on-device (existing `daikin_build_frame`) | Reuses the validated path verbatim; thinnest, lowest-risk Python |
| 328P as dumb modem | Python builds 35 bytes, streams raw | Duplicates checksum/fixed-byte logic in Python — re-opens bugs howto 07 already closed |
| PC IR dongle, no 328P | Python + USB-IR hardware | Different emitter, different timing path — throws away the proven chain |

The chosen split means the only new firmware is a line parser around code that
already works. No protocol re-derivation.

---

## 3. Serial protocol

Line-based ASCII, `\n`-terminated, human-typable in a plain terminal (`screen` /
`pyserial` miniterm) for debugging without the TUI. Case-insensitive.

**One command, one snapshot.** The 328P has no state to mutate, so the protocol has
no setters. `SEND` carries every field needed to build a frame.

### 3.1 Host → device

| Command | Effect |
|---|---|
| `SEND fan=<0\|1\|2\|3\|4\|5\|quiet\|auto> mode=<fan\|cool\|heat\|dry\|auto> temp=<14..30> swing=<on\|off>` | parse → `daikin_build_frame()` → `send_daikin()` |
| `PING` | liveness check → `PONG` |

`fan=0` is power-off, matching the hardware fan-knob position 0
([00_specifications.md §4.1](00_specifications.md)). There is no separate `POWER`
command — the fan setting *is* the power switch, same as the knob.

All four fields are **required** on every `SEND`. Mirrors the knob model: every knob
always has a position, there is no "unset." Order is free (`key=value` pairs).

### 3.2 Device → host

| Reply | Meaning |
|---|---|
| `READY daikin-serial/<v>` | printed once on boot |
| `SENT <70-hex-chars>` | frame transmitted; the 35 bytes as hex |
| `ERR <reason>` | parse failure or out-of-range (e.g. `ERR temp 40 out of range 14..30`) |
| `PONG` | answer to `PING` |

The `SENT <hex>` reply is the key debugging affordance: the TUI (and the human) sees
the **exact 35 bytes** that went out. Those can be diffed against the `ir_mock` oracle
([ir_mock/main.cpp](../ir_mock/main.cpp)) or a real `ir_rx_dump` capture
([howtos/07_verify_frame_against_capture.md](../howtos/07_verify_frame_against_capture.md))
to confirm the TUI and the hardware-remote firmware produce identical frames for the
same inputs.

### 3.3 Example session

```
← READY daikin-serial/1.0
→ SEND fan=3 mode=cool temp=24 swing=off
← SENT 11DA2700C500...   (35 bytes)
   (AC beeps, sets Cool / 24 °C / fan 3)
→ SEND fan=0 mode=cool temp=24 swing=off
← SENT 11DA2700C500...
   (AC powers off)
```

---

## 4. Firmware side — `sketches/daikin_serial/`

A near-trivial refactor of [sketches/daikin_fan_toggle](../sketches/daikin_fan_toggle):

- **Keep verbatim:** Timer2 setup, `ir_mark` / `ir_space` / `ir_space_long`,
  `send_byte`, `send_section`, `send_daikin`, all the protocol timing constants —
  this is the validated IR path, do not touch it.
- **Replace `loop()`:** instead of toggling fan every 30 s, read a line from `Serial`.
  - `PING` → reply `PONG`.
  - `SEND fan=… mode=… temp=… swing=…` → parse into local variables, call
    `daikin_build_frame(fan, mode, temp, swing)` + `send_daikin()`, reply
    `SENT <hex>`. No module-static state.
  - Anything else, or any missing/out-of-range field → `ERR <reason>`.
- **Bump baud to 115200.** `daikin_fan_toggle` runs 4800; for a responsive TUI use
  115200, proven on this board by [sketches/ir_rx_dump](../sketches/ir_rx_dump) —
  clear the prescaler at runtime (`CLKPR = 0` in `setup()`) so the chip runs at
  nominal 8 MHz regardless of the CKDIV8 fuse state. Copy that idiom.
- **Reuse the symlink trick:** `daikin_frame.{h,cpp}` are symlinked from `firmware/`,
  like the other sketches, so the frame builder stays single-source. Its signature
  may need to shift from a struct pointer to plain args
  (`daikin_build_frame(fan, mode, temp, swing, buf)`), which also prepares it for
  the knob firmware.

Parsing stays tiny: `strtok` on space then on `=`, a small switch per key, no dynamic
allocation. No command table, no state machine.

---

## 5. Python app — `tools/daikin_tui/`

`uv`-managed (PEP 723 inline-metadata script or a small package), matching the repo's
existing uv convention ([schematics/](../schematics/), [tools/serial_capture.py](../tools/serial_capture.py)).

### 5.1 Modules

| Module | Responsibility | Hardware? |
|---|---|---|
| `protocol.py` | format a `SEND` line from widget state, parse replies | none (pure, unit-testable) |
| `backend.py` | one `Backend` protocol + two impls (`SerialBackend`, `MockBackend`) | both |
| `app.py` | Textual UI: Fan / Mode / Temp / Swing widgets + Send + TX log pane | via backend |

The mock backend lets the entire UI be developed and demoed with no board plugged
in (`--mock`), then switched to the real serial port with a flag. It also makes
`protocol.py` testable in CI without hardware. Keep it one file — three backend
modules is overkill for a demo tool.

### 5.2 UI

![Daikin TUI](../images/screenshot_app_tui.png)

Each picker click — or temp ±, or Enter/Space on the focused button — immediately
ships the full current state as one `SEND fan=… mode=… temp=… swing=…` line and
appends the `SENT <hex>` reply to the log. The green-highlighted button in each
card is the **last value successfully sent** (so the UI reflects what the AC is
actually doing, not unsent intent). The standalone **▶ RESEND** button re-sends
the current state without changing it. `←`/`→` are aliased to Tab / Shift-Tab —
pure focus motion, no selection side-effects.

### 5.3 Connection handling

- `--port /dev/ttyUSB0` (or auto-detect a CH340) for the real backend; `--mock` for none.
- On connect, send `PING`, expect `PONG`; show a clear "disconnected / wrong port" state
  rather than hanging.
- Serial is line-oriented and slow relative to the UI; do I/O off the UI thread (Textual
  `@work` / asyncio) so the interface never blocks on a `SEND` (~120 ms of IR).

---

## 6. Verification

The `SENT <hex>` reply closes the loop cheaply:

1. **Frame parity with the hardware remote.** For a given state, the bytes the
   `daikin_serial` sketch emits must equal what `daikin_build_frame()` produces for the
   physical-remote firmware — they share the same `firmware/daikin_frame.cpp`, so they
   should be byte-identical. Diff `SENT <hex>` against `ir_mock`'s output for the same state.
2. **Frame parity with the real remote.** Diff against a captured ARC466A33 frame
   ([howtos/data/dump_fan_loop.txt](../howtos/data/dump_fan_loop.txt)), accounting for the
   known state-dependent and still-open byte-21 difference recorded in
   [howtos/07_verify_frame_against_capture.md](../howtos/07_verify_frame_against_capture.md).
3. **End-to-end.** The honest test is the AC: send `SEND`, the unit beeps and changes
   state, same as the fan-toggle milestone.

---

## 7. Open questions

- [ ] **Baud / framing.** 115200 + newline-terminated lines assumed; confirm the CH340
  link is clean at 115200 over the cable in use (the `ir_rx_dump` sketch already runs it).
- [ ] **Temp range in the UI.** Expose raw 14–30 °C, or apply the mode-dependent
  cooling/heating mapping from [00_specifications.md §4.3](00_specifications.md) so the
  TUI reads like the physical knob? (Leaning: mode-dependent, to match the hardware
  mental model.)
- [ ] **Swing / secondary modes.** Include `swing=` (and later Powerful/Econo) only
  once confirmed accepted by the FTXM20N2V1B over IR — same open question as the
  hardware ([00_specifications.md §9](00_specifications.md)).
- [ ] **Auto-detect port.** Nice-to-have: scan for the CH340 VID/PID instead of
  requiring `--port`.
```
