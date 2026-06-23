#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["schemdraw", "matplotlib"]
# ///
"""IR LED single-LED driver — S9013 + TSAL6200 at 3.3 V."""

import schemdraw
import schemdraw.elements as elm
from pathlib import Path

OUT = Path(__file__).with_suffix(".png")

with schemdraw.Drawing(fontsize=11) as d:
    d.config(inches_per_unit=0.3)
    # Current flows left → right: 3.3 V → R → LED → collector → emitter → GND
    d.add(elm.Line().left(0.5).label("3.3 V", loc="left"))
    d.add(elm.Resistor().right().label("22 Ω", loc="top"))
    d.add(elm.Line().right(0.3))
    d.add(elm.LED().right().label("TSAL6200", loc="top").color("red"))
    d.add(elm.Line().right(0.3))
    # Transistor mirrored: collector points left (into the LED branch),
    # base points out to the right → R_base → GPIO, emitter down → GND.
    q = d.add(elm.BjtNpn(circle=True).anchor("collector").theta(0).reverse().label("S9013", loc="top"))
    d.add(elm.Line().at(q.emitter).down(0.5))
    d.add(elm.Ground())
    # Base resistor — straight out to the right
    d.add(elm.Resistor().at(q.base).right().label("2.2 kΩ", loc="bottom"))
    d.add(elm.Dot().label("GPIO (IR_TX)", loc="right"))

d.save(OUT, dpi=150, transparent=False)
print(f"Saved: {OUT}")
