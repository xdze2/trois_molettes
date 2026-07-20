#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["skidl"]
# ///
"""
Full electrical netlist for the Daikin IR remote, generated with SKiDL.

This is the electrical counterpart to the diagram scripts in this folder:
those draw pictures (schemdraw), this one emits a **KiCad netlist**
(`circuit.net`) you import into Pcbnew to lay out the PCB.

Single source of truth is the design docs:
  - docs/05_electronics_circuit.md  (pin map, diode-encoding tables, buttons)
  - docs/06_IR_LED_wiring.md         (3x TSAL6200 driver, S9013, 22R, 680R, 220uF)
  - docs/07_battery_and_power.md     (TP4056 -> Pro Mini RAW, bulk cap)

The MCU is an Arduino Pro Mini plugged into two 1x12 female headers on the
carrier PCB (see the AskUser decision in the design history): the netlist wires
everything to the Pro Mini's *silkscreen* pins by name, so the module drops in.

Run it:
    uv run schematics/circuit.py            # writes schematics/circuit.net

First run fetches the KiCad 8.0.9 symbol libraries (Device, Switch,
Connector_Generic) into schematics/.kicad-symbols-cache/ via a sparse git
checkout. They are NOT committed (CC-BY-SA share-alike if redistributed as a
collection) and the cache dir is gitignored.
"""

import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
CACHE = HERE / ".kicad-symbols-cache"
OUT = HERE / "circuit.net"

# KiCad 8 tag: single-file .kicad_sym format that SKiDL 2.2.x parses.
# (KiCad 9/10 moved to a per-symbol directory layout SKiDL can't read yet.)
SYMBOLS_TAG = "8.0.9"
SYMBOLS_REPO = "https://gitlab.com/kicad/libraries/kicad-symbols.git"
NEEDED_LIBS = ["Device", "Switch", "Connector_Generic"]


def ensure_symbols() -> Path:
    """Sparse-checkout just the symbol libraries we use, once, into CACHE."""
    if all((CACHE / f"{lib}.kicad_sym").exists() for lib in NEEDED_LIBS):
        return CACHE
    print(f"Fetching KiCad {SYMBOLS_TAG} symbol libs into {CACHE} ...")
    CACHE.mkdir(parents=True, exist_ok=True)
    run = lambda *a: subprocess.run(a, cwd=CACHE, check=True,
                                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not (CACHE / ".git").exists():
        run("git", "init", "-q")
        run("git", "remote", "add", "origin", SYMBOLS_REPO)
        run("git", "config", "core.sparseCheckout", "true")
        sparse = CACHE / ".git" / "info" / "sparse-checkout"
        sparse.write_text("\n".join(f"{lib}.kicad_sym" for lib in NEEDED_LIBS) + "\nLICENSE.md\n")
    run("git", "fetch", "-q", "--depth", "1", "origin", f"refs/tags/{SYMBOLS_TAG}")
    run("git", "checkout", "-q", "FETCH_HEAD")
    missing = [lib for lib in NEEDED_LIBS if not (CACHE / f"{lib}.kicad_sym").exists()]
    if missing:
        sys.exit(f"error: symbol libs not fetched: {missing}")
    return CACHE


# Point SKiDL at the fetched libs *before* importing it.
os.environ["KICAD8_SYMBOL_DIR"] = str(ensure_symbols())

# SKiDL opens its log/ERC files at import time, in the CWD, named after this
# script. chdir into schematics/ first so circuit.log / circuit.erc /
# circuit_sklib.py land beside the script (all gitignored) instead of the repo
# root. Paths above are already absolute, so this is safe.
os.chdir(HERE)

from skidl import Net, Part, generate_netlist, set_default_tool, KICAD8  # noqa: E402

set_default_tool(KICAD8)


# ---------------------------------------------------------------------------
# Nets
# ---------------------------------------------------------------------------
vcc = Net("+3V3")          # regulated 3.3 V rail (Pro Mini VCC, switch wipers)
gnd = Net("GND")
vbat = Net("VBAT")         # cell voltage from TP4056 OUT+ -> Pro Mini RAW
vcc.drive = gnd.drive = 3  # let SKiDL treat these as power nets


# ---------------------------------------------------------------------------
# Arduino Pro Mini as two 1x12 female headers (silkscreen-labelled pins).
# We name each GPIO net after its function so the netlist reads like the docs.
# ---------------------------------------------------------------------------
# Pro Mini silkscreen -> our net. Only the pins we use are wired; the rest of
# the header pins stay unconnected (they still exist as header pads on the PCB).
pin_signals = {
    # rotary code lines
    "D10": Net("FAN_b0"),   "D11": Net("FAN_b1"),  "D12": Net("FAN_b2"),
    "A0":  Net("MODE_b0"),  "A1":  Net("MODE_b1"), "A2":  Net("MODE_b2"),
    "D4":  Net("TEMP_b0"),  "D5":  Net("TEMP_b1"), "D6":  Net("TEMP_b2"),
    # buttons
    "D2":  Net("RESEND"),   "D7":  Net("SWING"),
    # outputs
    "D3":  Net("IR_TX"),    "D13": Net("TX_LED"),  "A3": Net("FB_LED"),
}

# Two 12-pin headers the module sits on. J1 = "left" row, J2 = "right" row.
# The label->pad mapping here is a straightforward sequential assignment; when
# you lay out the board, match physical header pads to the Pro Mini's actual
# pinout. What matters for the netlist is the *net* on each function.
J1 = Part("Connector_Generic", "Conn_01x12", ref="J1", value="ProMini_row_A")
J2 = Part("Connector_Generic", "Conn_01x12", ref="J2", value="ProMini_row_B")

# Power to the module: VCC + GND + RAW live on the header rows too. We put them
# on the last free pads of each row for a clean starting point.
J1[1] += vcc            # VCC (3.3 V regulated out of the Pro Mini LDO)
J1[2] += gnd            # GND
J1[3] += vbat           # RAW (battery in, through onboard LDO)

# Signal pins spread across the two rows in a stable, documented order.
_signal_pads = [
    (J1, 4, "D2"), (J1, 5, "D3"), (J1, 6, "D4"), (J1, 7, "D5"),
    (J1, 8, "D6"), (J1, 9, "D7"), (J1, 10, "D10"), (J1, 11, "D11"),
    (J1, 12, "D12"), (J2, 1, "D13"), (J2, 2, "A0"), (J2, 3, "A1"),
    (J2, 4, "A2"), (J2, 5, "A3"),
]
for hdr, pad, label in _signal_pads:
    hdr[pad] += pin_signals[label]


# ---------------------------------------------------------------------------
# Diode-encoded rotary switches (docs/05 section 2).
# Each switch: wiper (pin 1) -> +3V3. From each *position* contact, a 1N4148
# routes to the code line(s) that must read HIGH for that position. Code lines
# have 1 M pull-downs to GND and go to plain-INPUT GPIOs.
# ---------------------------------------------------------------------------
# position -> (b2, b1, b0) natural binary. We derive the diodes from these.
FAN_POS = 8    # SR16 1P8T, positions 0..7
TEMP_POS = 8   # SR16 1P8T, positions 0..7
MODE_POS = 5   # RS1010 1P,  positions 0..4

_diode_count = 0


def encode_switch(ref, rot_symbol, n_positions, code_nets):
    """Wire one diode-encoded rotary switch. code_nets = [b0, b1, b2] (LSB first).

    SW_Rotary_1xN_MP: pin 1 = common/wiper, pins 2..N+1 = throws, MP = mount.
    """
    global _diode_count
    sw = Part("Switch", rot_symbol, ref=ref)
    sw[1] += vcc                    # wiper to +3V3
    if "MP" in [p.num for p in sw.pins]:
        sw["MP"] += gnd             # mounting pin -> GND (mechanical/shield)
    for pos in range(n_positions):
        throw = sw[pos + 2]         # pos 0 -> pin 2, pos 1 -> pin 3, ...
        for bit in range(len(code_nets)):
            if (pos >> bit) & 1:    # this bit is HIGH in this position's code
                _diode_count += 1
                d = Part("Device", "D", ref=f"D{ref}", value="1N4148")
                # anode(2) at the switch throw, cathode(1) to the code line:
                # current flows +3V3 -> wiper -> throw -> anode -> cathode -> line
                throw += d["A"]
                code_nets[bit] += d["K"]
    return sw


FAN = encode_switch("SW1", "SW_Rotary_1x8_MP", FAN_POS,
                    [pin_signals["D10"], pin_signals["D11"], pin_signals["D12"]])
MODE = encode_switch("SW2", "SW_Rotary_1x5_MP", MODE_POS,
                     [pin_signals["A0"], pin_signals["A1"], pin_signals["A2"]])
TEMP = encode_switch("SW3", "SW_Rotary_1x8_MP", TEMP_POS,
                     [pin_signals["D4"], pin_signals["D5"], pin_signals["D6"]])

# 1 M pull-downs to GND on every code line (docs/05 section 3).
for label in ["D10", "D11", "D12", "A0", "A1", "A2", "D4", "D5", "D6"]:
    r = Part("Device", "R", ref="Rpd", value="1M")
    pin_signals[label] += r[1]
    r[2] += gnd


# ---------------------------------------------------------------------------
# Buttons (docs/05 section 4).
# Resend: +3V3 -> button -> GPIO, external pull-down to GND (active-high).
# Swing:  GPIO -> button -> GND, internal pull-up (active-low), no ext resistor.
# ---------------------------------------------------------------------------
sw_resend = Part("Switch", "SW_Push", ref="SW4", value="Resend")
sw_resend[1] += vcc
sw_resend[2] += pin_signals["D2"]
r_resend = Part("Device", "R", ref="R", value="1M")   # external pull-down
pin_signals["D2"] += r_resend[1]
r_resend[2] += gnd

sw_swing = Part("Switch", "SW_Push", ref="SW5", value="Swing")
sw_swing[1] += pin_signals["D7"]
sw_swing[2] += gnd    # internal pull-up in firmware; no external resistor


# ---------------------------------------------------------------------------
# IR LED driver: 3x TSAL6200 in parallel, each with its own 22R, common
# S9013 low-side switch, 680R base resistor, 220uF bulk cap (docs/06).
# ---------------------------------------------------------------------------
q = Part("Device", "Q_NPN_EBC", ref="Q1", value="S9013")  # TO-92, E-B-C pinout
collector = Net("IR_COLLECTOR")                            # common LED cathode node
q["E"] += gnd
q["C"] += collector

r_base = Part("Device", "R", ref="R", value="680")        # 3-LED value (docs/06)
pin_signals["D3"] += r_base[1]                            # GPIO OC2B -> base R
r_base[2] += q["B"]

for i, ang in enumerate(["-30deg", "0deg", "+30deg"]):
    rs = Part("Device", "R", ref="R", value="22")
    led = Part("Device", "LED", ref=f"D_IR{i+1}", value=f"TSAL6200_{ang}")
    vcc += rs[1]
    rs[2] += led["A"]          # anode to the resistor / +3V3 side
    led["K"] += collector      # cathode to the shared collector node

c_bulk = Part("Device", "C", ref="C1", value="220u")      # bulk, near collector/VCC
vcc += c_bulk[1]
c_bulk[2] += gnd


# ---------------------------------------------------------------------------
# Indicator LEDs (docs/05 pin map): direct GPIO drive through a series R.
# TX_LED on D13, feedback LED on A3. 330R limit (BOM).
# ---------------------------------------------------------------------------
for ref, net_label, val in [("D_TX", "D13", "TX"), ("D_FB", "A3", "FB")]:
    rs = Part("Device", "R", ref="R", value="330")
    led = Part("Device", "LED", ref=ref, value=val)
    pin_signals[net_label] += rs[1]
    rs[2] += led["A"]
    led["K"] += gnd


# ---------------------------------------------------------------------------
# Power input: TP4056 module OUT+ / OUT- as a 2-pin header into VBAT / GND.
# (The TP4056 itself is a module, represented as a connector on this PCB.)
# ---------------------------------------------------------------------------
jpwr = Part("Connector_Generic", "Conn_01x02", ref="J3", value="TP4056_OUT")
jpwr[1] += vbat
jpwr[2] += gnd

# MCU decoupling cap (BOM: 100 nF across VCC/GND at the module).
c_dec = Part("Device", "C", ref="C2", value="100n")
vcc += c_dec[1]
c_dec[2] += gnd


generate_netlist(file_=str(OUT))
print(f"Diodes generated: {_diode_count} "
      f"(12 Fan + 5 Mode + 12 Temp = 29)")
print(f"Wrote netlist: {OUT}")
