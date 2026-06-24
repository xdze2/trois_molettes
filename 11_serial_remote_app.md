# 11 — Serial Remote App (Python Textual TUI)

A desktop **soft front-panel** for the AC: a [Textual](https://textual.textualize.io/)
terminal UI that mirrors the physical controls (Fan / Mode / Temp / Send) and drives
the **real** Daikin unit over USB serial → the ATmega328P → its IR LED.

This is a tool/companion to the hardware remote, **not a replacement**. It reuses the
exact, validated transmit path: `daikin_build_frame()` → `send_daikin()` → Timer2
38 kHz carrier → IR LED (proven end-to-end in
[howtos/08_daikin_fan_toggle.md](howtos/08_daikin_fan_toggle.md)). The 328P stays the
protocol authority; Python is a thin client.

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

**Division of responsibility — deliberately thin Python:**

- The **328P owns the protocol**: frame bytes, fixed/reserved bytes, the three section
  checksums, the 3-section pulse structure, the real-capture gap timings. All of this is
  already written and validated in C — the TUI must not duplicate or diverge from it.
- **Python owns the UI and intent**: it sends *what the user wants* (`fan=3`, `mode=cool`,
  `temp=24`, `SEND`) as plain text, and renders the device's replies. No bit-banging, no
  checksums, no frame layout in Python.

This mirrors the stateless-interface principle of the hardware ([00_specifications.md §3](00_specifications.md)):
the TUI widgets *are* the state; pressing **Send** transmits whatever they currently show.

---

## 2. Why "ATmega as IR transmitter" (vs. the alternatives)

| Option | Where the frame is built | Verdict |
|---|---|---|
| **328P transmitter (chosen)** | C, on-device (existing `daikin_build_frame`) | Reuses the validated path verbatim; thinnest, lowest-risk Python |
| 328P as dumb modem | Python builds 35 bytes, streams raw | Duplicates checksum/fixed-byte logic in Python — re-opens bugs howto 07 already closed |
| PC IR dongle, no 328P | Python + USB-IR hardware | Different emitter, different timing path — throws away the proven chain |

The chosen split means the **only new firmware** is a line parser around code that already
works. No protocol re-derivation.

---

## 3. Serial protocol

Line-based ASCII, `\n`-terminated, human-typable in a plain terminal (`screen` / `pyserial`
miniterm) for debugging without the TUI. Case-insensitive command words.

### 3.1 Host → device (commands)

| Command | Argument | Effect |
|---|---|---|
| `FAN <off\|1\|2\|3\|4\|5\|quiet\|auto>` | fan setting | sets `ACState.fan` (and `power=off` for `off`) |
| `MODE <fan\|cool\|heat\|dry\|auto>` | mode | sets `ACState.mode` |
| `TEMP <14..30>` | °C | sets `ACState.temp` (range-checked) |
| `POWER <on\|off>` | power | sets `ACState.power` |
| `SWING <on\|off>` | optional louvre | sets `ACState.swing_v` |
| `SEND` | — | `daikin_build_frame()` + `send_daikin()`; flash LED |
| `STATE` | — | reply with current `ACState` (no transmit) |
| `PING` | — | liveness check → `PONG` |

`FAN off` is the primary power-off, matching the hardware fan-knob position 0
([00_specifications.md §4.1](00_specifications.md)). Setting any control does **not**
transmit — only `SEND` does. This avoids spamming IR while the user adjusts widgets,
exactly as the hardware only transmits on the Send button.

### 3.2 Device → host (replies)

| Reply | Meaning |
|---|---|
| `READY <fw-version>` | printed once on boot |
| `OK <echoed-command>` | command parsed and applied |
| `ERR <reason>` | parse failure or out-of-range (e.g. `ERR temp 40 out of range 14..30`) |
| `STATE power=on mode=cool temp=24 fan=3 swing=off` | current state dump |
| `SENT <70-hex-chars>` | frame transmitted; the 35 bytes as hex, for verification |
| `PONG` | answer to `PING` |

The `SENT <hex>` reply is the key debugging affordance: the TUI (and the human) sees the
**exact 35 bytes** that went out. Those can be diffed against the `ir_mock` oracle
([ir_mock/main.cpp](ir_mock/main.cpp)) or a real `ir_rx_dump` capture
([howtos/07_verify_frame_against_capture.md](howtos/07_verify_frame_against_capture.md))
to confirm the TUI and the hardware-remote firmware produce identical frames for the same
state.

### 3.3 Example session

```
← READY daikin-serial/1.0
→ MODE cool
← OK MODE cool
→ TEMP 24
← OK TEMP 24
→ FAN 3
← OK FAN 3
→ SEND
← SENT 11DA2700C500...   (35 bytes)
   (AC beeps, sets Cool / 24 °C / fan 3)
```

---

## 4. Firmware side — `sketches/daikin_serial/`

A near-trivial refactor of [sketches/daikin_fan_toggle](sketches/daikin_fan_toggle):

- **Keep verbatim:** Timer2 setup, `ir_mark` / `ir_space` / `ir_space_long`, `send_byte`,
  `send_section`, `send_daikin`, all the protocol timing constants. This is the validated
  IR path — do not touch it.
- **Replace `loop()`:** instead of toggling fan every 30 s, read a line from `Serial`,
  parse it into the module-static `ACState`, and act:
  - mutating commands update the struct and reply `OK`,
  - `SEND` calls `daikin_build_frame()` + `send_daikin()` and replies `SENT <hex>`,
  - `STATE` formats the struct.
- **Bump baud to 115200.** `daikin_fan_toggle` runs 4800; for a responsive TUI use 115200.
  This board is proven at 115200 — [sketches/ir_rx_dump](sketches/ir_rx_dump) does exactly
  that, clearing the prescaler at runtime (`CLKPR = 0` in `setup()`) so the chip runs at
  nominal 8 MHz regardless of the CKDIV8 fuse state. Copy that idiom.
- **Reuse the symlink trick:** `daikin_frame.{h,cpp}` are symlinked from `firmware/`, like
  the other sketches, so the frame builder stays single-source.

Parsing stays tiny: a fixed command table, `strtok` on space, no dynamic allocation.
Target well under the existing sketch's footprint — there is no protocol logic to add.

---

## 5. Python app — `tools/daikin_tui/`

`uv`-managed (PEP 723 inline-metadata script or a small package), matching the repo's
existing uv convention ([schematics/](schematics/), [sketches/ir_rx_dump/capture.py](sketches/ir_rx_dump/capture.py)).

### 5.1 Modules

| Module | Responsibility | Hardware? |
|---|---|---|
| `protocol.py` | encode commands → lines, parse replies → objects | none (pure, unit-testable) |
| `backend.py` | abstract transport: `send(cmd) -> reply` | interface |
| `serial_backend.py` | pyserial implementation (115200, line I/O, timeout) | real board |
| `mock_backend.py` | same interface; echoes `OK`, fakes `SENT <hex>` | none |
| `app.py` | Textual UI: Fan / Mode / Temp widgets + Send + TX log pane | via backend |

The **`mock_backend`** is the point of building this layer: the entire UI can be developed
and demoed with no board plugged in (`--mock`), then switched to the real serial port with
a flag. It also makes `protocol.py` testable in CI without hardware — and it can embed the
same captured-frame oracle the C mock uses, so "what the TUI would send" is checkable.

### 5.2 UI sketch

```
┌─ Daikin Remote ──────────────────────────────┐
│                                               │
│   MODE   ( Fan  ‹Cool›  Heat  Dry  Auto )     │
│   FAN    ( Off  1  2  ‹3›  4  5  Quiet  Auto ) │
│   TEMP   ‹ 24 °C ›        [ − ]  [ + ]         │
│   SWING  [ off ]                               │
│                                               │
│              [  ▶  SEND  ]                     │
│                                               │
│ ── TX log ───────────────────────────────────│
│ 12:31:04  SEND → SENT 11DA2700C5...  ✓ beep   │
│ 12:30:58  TEMP 24 → OK                        │
└───────────────────────────────────────────────┘
   q quit   ←/→ select   enter send   m mock
```

Widget selections map 1:1 to the `FAN` / `MODE` / `TEMP` / `SWING` commands; **Send**
emits `SEND` and appends the `SENT <hex>` reply to the log. The temperature widget can
apply the same mode-dependent range the hardware uses
([00_specifications.md §4.3](00_specifications.md)) so the displayed °C matches what a
physical knob position would mean — or just expose the raw 14–30 range; decide during build.

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
   ([howtos/data/dump_fan_loop.txt](howtos/data/dump_fan_loop.txt)), accounting for the
   known state-dependent and still-open byte-21 difference recorded in
   [howtos/07_verify_frame_against_capture.md](howtos/07_verify_frame_against_capture.md).
3. **End-to-end.** The honest test is the AC: send `SEND`, the unit beeps and changes
   state, same as the fan-toggle milestone.

---

## 7. Open questions

- [ ] **Baud / framing.** 115200 + newline-terminated lines assumed; confirm the CH340
  link is clean at 115200 over the cable in use (the `ir_rx_dump` sketch already runs it).
- [ ] **Temp range in the UI.** Expose raw 14–30 °C, or apply the mode-dependent
  cooling/heating mapping from [00_specifications.md §4.3](00_specifications.md) so the TUI
  reads like the physical knob? (Leaning: mode-dependent, to match the hardware mental model.)
- [ ] **Swing / secondary modes.** Include `SWING` (and later Powerful/Econo) only once
  confirmed accepted by the FTXM20N2V1B over IR — same open question as the hardware
  ([00_specifications.md §9](00_specifications.md)).
- [ ] **Packaging.** Single PEP 723 uv script vs. a small `tools/daikin_tui/` package with
  `pyproject.toml` and tests. Lean toward the package once `protocol.py` has unit tests.
- [ ] **Auto-detect port.** Nice-to-have: scan for the CH340 VID/PID instead of requiring
  `--port`.
```
