#!/usr/bin/env -S uv run --quiet
# /// script
# requires-python = ">=3.10"
# dependencies = ["pyserial"]
# ///
"""Capture serial output from the rotary_switches_poll_test sketch.

`stty … && cat` does not work reliably on macOS — `cat` reopens the device
with default termios and resets the baud rate stty just set.  This script
uses pyserial, which opens the port the same way the Arduino IDE Serial
Monitor does.

Usage:
    uv run sketches/rotary_switches_poll_test/capture.py /dev/cu.usbserial-10
    uv run sketches/rotary_switches_poll_test/capture.py /dev/cu.usbserial-10 -o dump.txt
    uv run sketches/rotary_switches_poll_test/capture.py /dev/cu.usbserial-10 --changes-only
"""

import argparse
import sys

import serial


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("port", help="serial device, e.g. /dev/cu.usbserial-10")
    p.add_argument("-b", "--baud", type=int, default=9600,
                   help="wire baud rate (default: 9600)")
    p.add_argument("-o", "--output", help="also write incoming lines to this file")
    p.add_argument("-c", "--changes-only", action="store_true",
                   help="only print lines the sketch flagged as changed (leading '*')")
    args = p.parse_args()

    out = open(args.output, "w", buffering=1) if args.output else None

    with serial.Serial(args.port, args.baud, timeout=1) as s:
        print(f"# listening on {args.port} @ {args.baud} baud — Ctrl-C to stop",
              file=sys.stderr)
        try:
            while True:
                line = s.readline().decode(errors="replace")
                if not line:
                    continue
                if out:
                    out.write(line)
                if args.changes_only and not line.startswith("*"):
                    continue
                sys.stdout.write(line)
                sys.stdout.flush()
        except KeyboardInterrupt:
            print("\n# stopped", file=sys.stderr)
        finally:
            if out:
                out.close()


if __name__ == "__main__":
    main()
