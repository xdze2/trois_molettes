#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["schemdraw", "matplotlib"]
# ///
"""IR LED 3-LED driver — S9013 + 3× TSAL6200 in parallel at 3.3 V, with bulk cap."""

import schemdraw
import schemdraw.elements as elm
from pathlib import Path

OUT = Path(__file__).with_suffix(".png")

with schemdraw.Drawing(fontsize=11) as d:
    d.config(inches_per_unit=0.3)

    branch_gap = 2.5          # vertical spacing between the three rows
    branch_len = 5.0          # horizontal length of R + LED per row
    vcc_x = 0.0               # x of the VCC rail (left)
    col_x = branch_len        # x of the common collector node (right)

    # Three horizontal LED branches, current flowing left → right.
    # Each row: VCC rail → R → LED → common collector node.
    rows = ["LED1\n−30°", "LED2\n0°", "LED3\n+30°"]
    for i, label in enumerate(rows):
        y = -i * branch_gap
        d.add(elm.Resistor().at((vcc_x, y)).right(branch_len / 2).label("22 Ω", loc="top"))
        d.add(elm.LED().right(branch_len / 2).label(label, loc="bottom").color("red"))

    top_y = 0.0
    bot_y = -(len(rows) - 1) * branch_gap

    # VCC rail (left) tying the three resistors together
    d.add(elm.Line().at((vcc_x, top_y)).to((vcc_x, bot_y)))
    d.add(elm.Line().at((vcc_x, top_y)).up(0.8).label("3.3 V", loc="top"))

    # Common collector rail (right) tying the three LED cathodes together
    d.add(elm.Line().at((col_x, top_y)).to((col_x, bot_y)))

    # Transistor mirrored, to the right of the collector rail:
    # collector points left (into the LED branches), base exits right → GPIO.
    d.add(elm.Line().at((col_x, bot_y)).right(0.5))
    q = d.add(elm.BjtNpn(circle=True).anchor("collector").theta(0).reverse().label("S9013", loc="bottom"))

    # Emitter to GND
    d.add(elm.Line().at(q.emitter).down(0.5))
    d.add(elm.Ground())

    # Base resistor — straight out to the right, clear of the LED rows
    d.add(elm.Resistor().at(q.base).right().label("2.2 kΩ", loc="bottom"))
    d.add(elm.Dot().label("GPIO (IR_TX)", loc="right"))

    # Bulk cap from VCC rail (top) down to GND
    d.add(elm.Line().at((vcc_x, top_y)).left(1.0))
    d.add(elm.Capacitor().down().label("220 µF", loc="left"))
    d.add(elm.Ground())

d.save(OUT, dpi=150, transparent=False)
print(f"Saved: {OUT}")
