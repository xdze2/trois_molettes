#!/usr/bin/env -S uv run --quiet
# /// script
# requires-python = ">=3.10"
# dependencies = ["pyserial"]
# ///
"""Capture serial output from any of the sketches in sketches/.

`stty … && cat` does not work reliably on macOS — `cat` reopens the device
with default termios and resets the baud rate stty just set.  This script
uses pyserial, which opens the port the same way the Arduino IDE Serial
Monitor does.

Usage:
    uv run tools/serial_capture.py /dev/cu.usbserial-10
    uv run tools/serial_capture.py /dev/cu.usbserial-10 -b 9600
    uv run tools/serial_capture.py /dev/cu.usbserial-10 -o dump.txt
    uv run tools/serial_capture.py /dev/cu.usbserial-10 --filter-prefix SEND
"""

import argparse
import sys

import serial


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("port", help="serial device, e.g. /dev/cu.usbserial-10")
    p.add_argument("-b", "--baud", type=int, default=115200,
                   help="wire baud rate (default: 115200)")
    p.add_argument("-o", "--output", help="also write incoming lines to this file")
    p.add_argument("-f", "--filter-prefix", action="append", default=[], metavar="PREFIX",
                   help="only print lines starting with PREFIX (repeatable); "
                        "e.g. -f SEND -f SENT, or -f '*' for change-flagged lines")
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
                if args.filter_prefix and not any(line.startswith(pfx) for pfx in args.filter_prefix):
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
