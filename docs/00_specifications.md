# Daikin Custom Remote — Specifications

## 1. Motivation

1. Build something cool and learn by doing it.
2. Get a nice tangible, simple, robust, low-complexity, screenless, app-less, open-source Daikin AC remote.

## 2. Target AC Unit

| Item | Value |
|---|---|
| Indoor unit | FTXM20N2V1B (2020/06) |
| Original remote | ARC466A33 |
| Interface used | IR only |

The WiFi adapter and REST API are explicitly out of scope. This device communicates via IR only.

## 3. Core Design Principle — Stateless Interface

**The physical position of every knob and button reflects the full state of the system.** No hidden state, no screen, nothing to synchronize — look at the device and you know what it will send next. This drives every interface decision below.

The IR protocol is one-way: it does not return status from the unit (current temperature, etc.), so no richer feedback is needed or possible. The only feedback is the audible beep from the AC unit itself.

Implications:
- All controls are hardware (rotary switches, physical buttons) — not virtual or touch-based.
- A "resend" action retransmits whatever the knobs currently show — no stored state to diverge from physical position.

## 4. Controls & Functions

Controls are ordered by frequency of use.

Rotary switches are the logical choice here: they are stateless and tangible (a rotary *encoder*, by contrast, is not stateless — it only reports movement, not absolute position).

Off-the-shelf rotary switches commonly offer 3 to 12 positions; the mapping below is designed around that range.

### 4.1 Fan speed — 8-position rotary

| Position | Meaning |
|---|---|
| 0 | Off |
| 1–5 | Fan speed 1–5 |
| 6 | Quiet |
| 7 | Auto |

Fan position 0 is Off. This makes the fan knob the primary on/off control — the one touched every day.

### 4.2 Mode — 5-position rotary

| Position | Mode |
|---|---|
| 0 | Fan |
| 1 | Cool |
| 2 | Heat |
| 3 | Dry |
| 4 | Auto |

### 4.3 Temperature — 8-position rotary, mode-dependent range

Fitting the full temperature range onto 8 positions takes a small trick: a mode-dependent mapping. One 8-position switch covers both heating and cooling ranges. The knob turns from
**coldest at the leftmost detent to warmest at the rightmost**; the active mode
selects which range those eight detents span:

| Detent (left → right) | Cooling (Cool/Fan/Dry/Auto) | Heating (Heat) |
|---|---|---|
| leftmost | 24 °C | 14 °C |
| | 25 °C | 15 °C |
| | 26 °C | 16 °C |
| | 27 °C | 17 °C |
| | 28 °C | 18 °C |
| | 29 °C | 19 °C |
| | 30 °C | 20 °C |
| rightmost | 31 °C | 21 °C |

1 °C steps throughout. The mode knob determines which range is active — the physical
temp knob position is unambiguous, and the correct range is applied automatically.
The panel labelling must make the mode-dependent scale clear.

(The protocol itself allows 0.5 °C steps, unused here.)

**Alternatives considered:**
- Two switches (5×5 = 25 positions) for finer resolution. Rejected: doubles the
  switch count, complicates the panel layout, and 1 °C steps over an 8-position
  range cover the practical daily-use band adequately.
- A continuous potentiometer instead of a detented switch — possible, but loses the
  discrete, labelled detents that make the setting readable at a glance.

### 4.4 Send — 1 push button

Transmits the full current state via IR. No setting changes. Pressing Send is the
only action that causes the AC unit to respond.

### 4.5 Swing — 1 toggle (optional)

On/off toggle for louvre swing. Nice to have; included if panel space allows.
Support depends on what the FTXM20N2V1B accepts via IR — to be confirmed.

## 5. Feedback

The knob positions *are* the feedback — there are no mode or temperature indicators. A single LED gives a brief flash on IR send to confirm the device is alive; the substantive confirmation is the AC unit's own beep (see §3).

## 6. Power

- Battery powered, fully untethered.
- **Target battery life: more than 6 months** on a single charge (goal: years) — a hard constraint driving MCU selection and sleep strategy.
- USB charging port on the enclosure, if needed.
- Low-battery indicator: not included.

## 7. Physical Form

- **Dimensions: ~80 × 100 × 25 mm** (thickness is the hardest to reduce; <30 mm required).
- **Primary use: wall-mounted.** Must be usable handheld, but the design is optimized for wall/surface mount.
- Mount method: magnetic or screw mount on the back face.
- **IR LED must have an unobstructed forward-facing window.**
- **All rotary controls and buttons on the top face.**
- 3D-printed enclosure. Design must be solid, well-made, and visually intentional — not a prototype box.

Rotary switches take noticeable torque to turn, so the device must be held with both hands — or, if wall-mounted, fixed firmly enough (at least two screws) to turn a knob one-handed without the whole unit twisting.

## 8. Non-functional Requirements

- **Open source** — hardware design, firmware, and enclosure files published.
- **Repairable** — standard components, accessible internals, no glued-shut assembly.
- **No app, no network, no cloud dependency.**
- **No screen.**
- Low complexity — the firmware should be small and auditable.
- **BOM cost: < €35 total** (excluding enclosure and tools). This is a hard constraint on component selection.

### 8.1 Constraints imposed by the Daikin IR frame

The IR frame is not just a firmware detail — its shape sets hard floors on the
hardware. These consequences drive choices justified in the dedicated docs;
the frame reference itself is in [Annex A1](A1_IR_protocol_and_mapping.md).

| The frame requires… | …which constrains |
|---|---|
| 35-byte, 3-frame fixed structure (byte array + checksum) | flash/RAM floor — trivial for any candidate MCU, so **not** a differentiator (see [03_microcontroller_choice.md](03_microcontroller_choice.md)) |
| **38 kHz** carrier, pulse-distance timing | a hardware timer for the carrier (Timer2 on the 328P — see [10_software_architecture.md](10_software_architecture.md)) |
| Transmit-only, hand-portable (no ESP-only library needed) | keeps firmware "small and auditable" and frees the MCU from library-supported chips — so it is chosen on **power/wake first** |


## 9. Nice to have (out of scope for v1)

Both would add significant complexity and are deliberately excluded from v1:

- Actual room-temperature indicator — needs a screen plus a thermometer (or the WiFi protocol to read the unit back).
- Time-scheduling functions — needs a clock and stored state, breaking the stateless principle.