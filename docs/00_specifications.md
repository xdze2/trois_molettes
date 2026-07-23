# Daikin Custom Remote — Specifications

## 1. Motivation

1. Build something cool and learn by doing it.
2. Replace the original remote — a fragile, app-dependent, non-repairable black box — with something tangible, simple, robust, low-complexity, screenless, app-less, open-source, and repairable.

## 2. Target AC Unit

| Item | Value |
|---|---|
| Indoor unit | FTXM20N2V1B (2020/06) |
| Original remote | ARC466A33 |
| Interface used | IR only |

The WiFi adapter and REST API are explicitly out of scope. This device communicates via IR only.

## 3. Core Design Principle — Stateless Interface

**The physical position of every knob and button reflects the full state of the system.** No hidden state, no screen, nothing to synchronize — look at the device and you know what it will send next. This drives every interface decision below.

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

The table is indexed by **raw gpio code**, matching the firmware's `TEMP_C` table.
The knob is wired reversed — raw 0 is the **rightmost** detent = warmest, raw 7 the
leftmost = coldest — so the ranges count *down* with raw code, absorbing the wiring
reversal into row order (same convention as the Fan/Mode knobs):

| Raw code | Detent | Cooling (Cool/Fan/Dry/Auto) | Heating (Heat) |
|---|---|---|---|
| 0 | rightmost | 31 °C | 21 °C |
| 1 | | 30 °C | 20 °C |
| 2 | | 29 °C | 19 °C |
| 3 | | 28 °C | 18 °C |
| 4 | | 27 °C | 17 °C |
| 5 | | 26 °C | 16 °C |
| 6 | | 25 °C | 15 °C |
| 7 | leftmost | 24 °C | 14 °C |

1 °C steps throughout. The mode knob position determines which mapping is active —
the physical temp knob position is unambiguous, the firmware applies the correct range.

**Alternative considered:** two RS1010 switches (5×5 = 25 positions) for finer
resolution. Rejected: doubles the switch count, complicates the panel layout, and
1 °C steps over an 8-position range cover the practical daily-use range adequately.

### 4.4 Send — 1 push button

Transmits the full current state via IR. No setting changes. Pressing Send is the
only action that causes the AC unit to respond.

### 4.5 Swing — 1 toggle (optional)

On/off toggle for louvre swing. Nice to have; included if panel space allows.
Support depends on what the FTXM20N2V1B accepts via IR — to be confirmed.

## 5. Feedback

Single LED, brief flash on IR send. No mode indicators, no temperature bar — the knob positions are the feedback.

## 6. Power

- Battery powered, fully untethered.
- **Target battery life: more than 6 months** on a single charge (goal: years) — a hard constraint driving MCU selection and sleep strategy.
- USB charging port on the enclosure.
- Low-battery indicator: nice to have.

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
2. **Temperature mapping validation** — Confirm the Daikin IR protocol accepts 1 °C step targets across both ranges (14–21 °C heating, 24–31 °C cooling).
3. **6-month battery target** — Hard constraint. Sleep current ≈ average current. Must be measured on bench before cell is sized. See [07_battery_and_power.md](07_battery_and_power.md).
4. **Panel layout** — Confirm two SR16 + one RS1010 + Send button (+ optional swing toggle) fit the 80×100 mm face.

Resolved: MCU = ATmega328P (Pro Mini 3.3 V), on hand, sufficient for this design. Readout = diode encoding on 1P switches. Feedback = single TX LED.
