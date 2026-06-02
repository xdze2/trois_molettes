---
title: "Custom Daikin AC remote control"
source: "https://claude.ai/chat/683e1dc7-c9fd-42d3-a2bf-2fd9257da869"
author:
published:
created: 2026-06-01
description: "Claude conversation with 6 messages"
tags:
  - "clippings"
---
**You**

I have a daikin AC unit (air air), I want to build a custom remote, what is possible to do? Is the IR protocol documented?

---

**Claude**

Great news — the Daikin IR protocol is very well documented by the community. Here's a comprehensive overview:

---

### Is the Protocol Documented?

Yes, very well. Daikin never published it officially, but the community has reverse-engineered it thoroughly. The key resources are:

- **[blafois/Daikin-IR-Reverse](https://github.com/blafois/Daikin-IR-Reverse)** — the foundational reverse-engineering project, covering the full frame structure, encoding, checksums, temperature, fan, swing, timers, etc.
- **[IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266)** — a mature Arduino library for ESP8266/ESP32 that fully implements Daikin IR sending/receiving.

---

### Protocol Basics

Each remote control transmission produces 3 frames. The first 2 frames are 8 bytes long, and the 3rd is 19 bytes. The protocol uses **Pulse Distance Encoding** (HIGH states have constant duration; the bit value is determined by the LOW duration). The carrier frequency is **38 kHz**. [GitHub](https://github.com/blafois/Daikin-IR-Reverse)

Temperature encoding is straightforward: take the temperature in Celsius, multiply by 2, and encode it in hex. For example, 20°C → 40 → 0x28. [GitHub](https://github.com/blafois/Daikin-IR-Reverse)

There are **multiple Daikin IR variants** depending on your remote model. Supported variants include DAIKIN (standard), DAIKIN2, DAIKIN64, DAIKIN128, DAIKIN152, DAIKIN160, DAIKIN176, DAIKIN200, DAIKIN216, and DAIKIN312 — so first check which remote model (ARC\*\*\*) you have. [GitHub](https://github.com/crankyoldgit/IRremoteESP8266/blob/master/SupportedProtocols.md)

---

### What You Can Control

All the standard AC functions are documented:

- **Power** on/off
- **Mode**: Cool, Heat, Fan-only, Dry, Auto
- **Temperature**: typically 10–30°C
- **Fan speed** and **Swing** mode (top 4 bits for fan, lower 4 bits for swing) [GitHub](https://github.com/blafois/Daikin-IR-Reverse)
- **Timers**: Power On timer and Power Off timer, with delay coded in minutes [GitHub](https://github.com/blafois/Daikin-IR-Reverse)
- **Comfort mode**, **Econo mode**, **Quiet mode**, **Powerful mode** (depending on your unit)

---

### Best Approach: ESP32/ESP8266 + IRremoteESP8266

This is the easiest path to a custom remote:

The IRremoteESP8266 library supports both simple protocols (TV remotes) and complex ones (AC units), making it suitable for home automation and remote control duplication on ESP8266 and ESP32 boards. [DeepWiki](https://deepwiki.com/crankyoldgit/IRremoteESP8266)

**Hardware needed:**

- ESP32 or ESP8266 board (~$3–5)
- IR LED (940nm) + a transistor/resistor driver
- Optionally an IR receiver (TSOP38238) to capture your original remote first

**What you can build:**

- A **Wi-Fi web interface** to control the AC from your phone
- An **MQTT-connected** remote for Home Assistant / Node-RED integration
- A **physical custom remote** with buttons and a small screen
- A **voice-controlled** remote via Home Assistant

The library includes a ready-to-use example `TurnOnDaikinAC.ino` that shows exactly how to send Daikin IR signals from an ESP8266/ESP32. [GitHub](https://github.com/crankyoldgit/IRremoteESP8266/blob/master/examples/TurnOnDaikinAC/TurnOnDaikinAC.ino)

---

### Quick Start Recommendation

1. **Identify your remote model** (look on the back of your original remote, e.g. ARC433A, ARC477A1, etc.)
2. **Match it to the protocol variant** in the IRremoteESP8266 supported list
3. Wire an IR LED to your ESP32 (GPIO4 is recommended)
4. Use the `IRDaikinAC` class from the library to set mode, temp, fan, etc. and call `send()`

Would you like a concrete code example or wiring diagram for a specific setup (ESP32 with web interface, Home Assistant, etc.)?

---

**You**

make a estimated BOM to start the project, including breadboard...etc

---

**Claude**

A few things worth noting:

**The most important component choices:**

The **TSOP38238 IR receiver** is critical for the first step — you'll want to point your original Daikin remote at it and capture the raw signal to verify your board is actually speaking the right protocol variant before going fully custom.

The **NPN transistor** (2N2222A or S8050) is necessary because an ESP32 GPIO can only source ~12 mA, but an IR LED needs ~100 mA for decent range (3–5 meters). The transistor is the amplifier in between.

For the **breadboard**, the ESP32 DevKit V1 (30-pin) is almost exactly as wide as a standard breadboard — you'll have very little room on each side. The 38-pin version is even wider. Buy a full 830-tie board, not a half-size one.

**Build sequence once you have the parts:**

1. Wire up the IR receiver first → capture a signal from your original remote → confirm it decodes in the serial monitor with IRremoteESP8266
2. Wire up the LED + transistor driver → replay the captured signal → confirm the AC responds
3. Then start building custom command generation (set temp, mode, fan, etc.)

Want me to draw the wiring schematic for the ESP32 + IR LED driver + TSOP receiver circuit?

---

**You**

I actually want to make a tangible interface with rotary switch, toggle buttons... Etc

is there similar project published?

---

**Claude**

Here's what the maker community has published that's directly relevant to what you want to build:

---

**🎯 OMOTE — the closest reference project**

OMOTE is an ESP32-based open source universal remote with a capacitive 2.8" touchscreen providing an intuitive LVGL-based UI for switching devices and settings. It features built-in infrared, Wi-Fi and Bluetooth with no hub required, and can run for months on a 2000 mAh Li-Po battery. The hardware is designed to be easily replicated using 3D-printed parts, a 2-layer PCB, and commonly available components. It uses IRremoteESP8266 under the hood, so Daikin support slots right in. The repos are at **github.com/OMOTE-Community**. This is the best foundation to fork for your project. [Hackaday](https://hackaday.io/project/191752-omote-diy-universal-remote) [GitHub](https://github.com/OMOTE-Community/OMOTE-Hardware)

---

**🎛️ ESP32 MQTT Button Box**

One maker built a physical controller with 17 buttons, a potentiometer, an LED, and a rotary encoder all wired to an ESP32, publishing to MQTT topics and integrating with Home Assistant via ESPHome. The writeup at `thibmaek.com` covers the full wiring and ESPHome YAML config — a great reference for the physical input layer. [Thibmaek](https://thibmaek.com/posts/building-an-esp32-mqtt-button-box)

---

**🌡️ Thermostat-style knob (Regulat-ur)**

A maker built a standalone physical thermostat using a DOIT ESP32 DevKit with an OLED display, temperature sensor, and rotary encoder — all housed in a 3D-printed enclosure and running a local webserver. Very close to the form factor you're imagining for an AC controller. [Hackaday](https://hackaday.io/project/180267-regulat-ur-radiator)

---

**What doesn't exist yet (your opportunity)**

There's no published project that combines a **dedicated Daikin AC physical remote** with rotary encoder for temperature, toggle switches for mode/fan, and an OLED status display all in one 3D-printed box. The closest is OMOTE (touchscreen-oriented) and the button box (no display, no IR AC focus). Your build would essentially be a novel combination of these three reference projects.

**Suggested architecture for your build:**

- **Temperature** → rotary encoder (EC11, with push to confirm)
- **Mode** (Cool/Heat/Fan/Dry/Auto) → 5-position rotary selector switch or repeated press on the encoder
- **Fan speed** → 4-position rotary selector or toggle buttons
- **Power / Swing / Eco / Turbo** → latching or momentary toggle switches
- **OLED 128×64** → shows current state (mode icon, temp, fan, swing)
- **ESP32** → drives the IR LED, Wi-Fi optional (for OTA updates or Home Assistant)

Want me to sketch out a wiring diagram and GPIO assignment for this specific layout?