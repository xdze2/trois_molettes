# Daikin Custom Remote — Specifications

## 1. Motivation

Two goals:
1. Build something cool and learn by doing it.
2. Produce a tangible, simple, robust, usable, low-complexity, screenless, app-less, open-source, and repairable AC control device.

The AC unit's original remote is a black box — fragile, app-dependent, and not repairable. This project replaces it with something physical, honest, and long-lived.

## 2. Target AC Unit

| Item | Value |
|---|---|
| Indoor unit | FTXM20N2V1B (2020/06) |
| Original remote | ARC466A33 |
| Interface used | IR only |

The WiFi adapter and REST API are explicitly out of scope. This device communicates via IR only.

## 3. Core Design Principle — Stateless Interface

**The physical position of every knob and button reflects the full state of the system.**

There is no hidden internal state. No screen. No memory to synchronize. If you look at the device, you know what it is doing (or what it will do on next send). This is the central design constraint that drives all interface decisions.

Implications:
- All controls are hardware (rotary switches, physical buttons) — not virtual or touch-based.
- The IR protocol does not return status from the unit (current temperature, etc.), so no richer feedback is needed or possible.
- A "resend" action retransmits whatever the knobs currently show — no stored state to diverge from physical position.

## 4. Controls & Functions

Controls are ordered by frequency of use.

### 4.1 Fan speed — 8-position rotary (SR16, daily use)

| Position | Meaning |
|---|---|
| 0 | Off |
| 1–5 | Fan speed 1–5 |
| 6 | Quiet |
| 7 | Auto |

Fan position 0 is Off. This makes the fan knob the primary on/off control — the one touched every day.

### 4.2 Mode — 5-position rotary (RS1010)

| Position | Mode |
|---|---|
| 0 | Fan |
| 1 | Cool |
| 2 | Heat |
| 3 | Dry |
| 4 | Auto |

### 4.3 Temperature — 8-position rotary (SR16), mode-dependent mapping

One 8-position switch covers both heating and cooling ranges via a mode-dependent offset
applied in firmware:

| Position | Cooling (Cool/Fan/Dry/Auto) | Heating (Heat) |
|---|---|---|
| 0 | 20 °C | 14 °C |
| 1 | 22 °C | 16 °C |
| 2 | 24 °C | 18 °C |
| 3 | 26 °C | 20 °C |
| 4 | 28 °C | 22 °C |
| 5 | 30 °C | 24 °C |
| 6 | 32 °C | 26 °C |
| 7 | 34 °C | 28 °C |

2 °C steps throughout. The mode knob position determines which mapping is active —
the physical temp knob position is unambiguous, the firmware applies the correct range.

**Alternative considered:** two RS1010 switches (5×5 = 25 positions) for finer
resolution. Rejected: doubles the switch count, complicates the panel layout, and
2 °C steps cover the practical daily-use range adequately.

### 4.4 Send — 1 push button

Transmits the full current state via IR. No setting changes. Pressing Send is the
only action that causes the AC unit to respond.

### 4.5 Swing — 1 toggle (optional)

On/off toggle for louvre swing. Nice to have; included if panel space allows.
Support depends on what the FTXM20N2V1B accepts via IR — to be confirmed.

## 5. Feedback

The only feedback mechanism is a single LED:

1. **IR transmission** — brief flash when a signal is sent.

No mode indicators, no temperature bar. The knob positions are the feedback.

## 6. Power

- Battery powered. Must work fully untethered.
- **Target battery life: more than 6 months** of normal use on a single charge (goal: years).
- USB charging port on the enclosure.
- Low-battery indicator is desirable (nice to have).

Battery life is a hard constraint that drives microcontroller selection and sleep strategy.

## 7. Physical Form

- **Dimensions: ~80 × 100 × 25 mm** (thickness is the hardest to reduce; <30 mm required).
- **Primary use: wall-mounted.** Must be usable handheld, but the design is optimized for wall/surface mount.
- Mount method: magnetic or screw mount on the back face.
- **IR LED must have an unobstructed forward-facing window.**
- **All rotary controls and buttons on the top face.**
- 3D-printed enclosure. Design must be solid, well-made, and visually intentional — not a prototype box.

## 8. Non-functional Requirements

- **Open source** — hardware design, firmware, and enclosure files published.
- **Repairable** — standard components, accessible internals, no glued-shut assembly.
- **No app, no network, no cloud dependency.**
- **No screen.**
- Low complexity — the firmware should be small and auditable.
- **BOM cost: < €35 total** (excluding enclosure and tools). This is a hard constraint on component selection.

## 9. Open Questions

1. **Swing support** — Confirm the FTXM20N2V1B accepts swing toggle via IR.
2. **Temperature mapping validation** — Confirm the Daikin IR protocol accepts 2 °C step targets across both ranges (14–28 °C heating, 20–34 °C cooling).
3. **6-month battery target** — Hard constraint. Sleep current ≈ average current. Must be measured on bench before cell is sized. See [07_battery_and_power.md](07_battery_and_power.md).
4. **Panel layout** — Confirm two SR16 + one RS1010 + Send button (+ optional swing toggle) fit the 80×100 mm face.

Resolved: MCU = ATmega328P (Pro Mini 3.3 V), on hand, sufficient for this design. Readout = diode encoding on 1P switches. Feedback = single TX LED.
