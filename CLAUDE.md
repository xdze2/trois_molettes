# daikin_remote

Design notes and hardware docs for a Daikin AC IR remote built on an ATmega328P
Pro Mini at 3.3 V. The repo is mostly Markdown design docs (`00_*`–`10_*`), with
generated circuit schematics.

## Schematics workflow

Schematics are **generated from Python scripts** in `schematics/` using
[schemdraw](https://schemdraw.readthedocs.io/). Do not hand-edit the PNGs — edit
the script and re-render.

- One script per diagram: `schematics/<name>.py`.
- Each script is a self-contained [uv](https://docs.astral.sh/uv/) script (PEP 723
  inline metadata declares `schemdraw` + `matplotlib`), so it runs with no setup:

  ```bash
  uv run schematics/<name>.py        # render one
  for f in schematics/*.py; do uv run "$f"; done   # render all
  ```

- Each script writes its PNG **next to itself**: `<name>.py` → `<name>.png`
  (via `Path(__file__).with_suffix(".png")`). Docs reference these directly,
  e.g. `![…](schematics/ir_led_single.png)`.

  (Older diagrams unrelated to schemdraw — `rs1010_wiring_2`, `rc001` — still live
  in `images/circuit/`.)

### Conventions

- **Current flows left → right** (3.3 V → R → LED → transistor → GND). Keeps wire
  labels above/below the run instead of crowding a vertical stack.
- Transistors are drawn **mirrored** (`.reverse()`) so the base / GPIO terminal
  exits to the right, clear of the LED branches.
- After changing a script, re-render and visually check the PNG for label
  collisions before committing both the `.py` and the `.png`.
