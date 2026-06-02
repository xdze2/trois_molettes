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

### 4.1 Primary controls

| # | Function | Range | Control type | Frequency of change |
|---|---|---|---|---|
| 1 | Power + Fan speed | OFF / 1 / 2 / 3 / 4 / 5 | Rotary selector, 6 positions | Daily |
| 2 | Mode | Fan / Cool / Heat / Dry | Rotary selector, 4–5 positions | Seasonal (every few months) |
| 3 | Target temperature | 16–26 °C | Rotary selector, 11 positions (1 °C steps) | Infrequent (per room setting) |

Fan speed position 0 is OFF. This makes the fan knob the primary on/off control — the one touched every day.

Mode is set seasonally and does not include OFF (power is handled by the fan knob).

### 4.2 Secondary modes (push buttons)

| Function |
|---|
| Powerful |
| Econo |
| Swing |

Support for these depends on what the FTXM20N2V1B actually accepts via IR — to be confirmed.

### 4.3 Resend

A dedicated action (button press or knob push) retransmits the full current state via IR. Handles missed commands without changing any setting.

### 4.4 Timer / scheduling — out of scope

Daily scheduling (wake/sleep timer) is explicitly out of scope. Spring-wound mechanical timers are not available in a form factor compatible with the <30 mm depth constraint, and the IR protocol timer interface requires a clock and display. Scheduling is better handled externally via the BRP069B41 WiFi adapter and home automation.

## 5. Feedback

The only feedback mechanism is a single LED (or minimal LEDs):

1. **Battery not dead** — confirms the device is powered.
2. **IR transmission** — brief flash when a signal is sent.

No LED matrix, no mode indicators, no temperature bar. The knob positions are the feedback. This keeps the design simple and power consumption low.

## 6. Power

- Battery powered. Must work fully untethered.
- **Target battery life: more than 6 months** of normal use on a single charge (goal: years).
- USB charging port on the enclosure.
- Low-battery indicator is desirable (nice to have).

Battery life is a hard constraint that drives microcontroller selection, LED count, and sleep strategy.

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

1. **Mode count: 4 or 5?** — Does the design include Dry mode? Determines switch position count.
2. **Secondary modes support** — Confirm which of Powerful / Night / Econo the FTXM20N2V1B accepts via IR.
3. **IR variant for ARC466A33** — Identify the correct IRremoteESP8266 protocol class.
4. **Temperature control resolution** — Confirm 11-position selector fits the enclosure layout alongside the other two knobs.
