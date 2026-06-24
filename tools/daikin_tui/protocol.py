"""Protocol helpers — pure, no I/O, unit-testable without hardware."""

from __future__ import annotations

VALID_MODES = ("fan", "cool", "heat", "dry", "auto")
VALID_FAN = ("0", "1", "2", "3", "4", "5", "quiet", "auto")
TEMP_MIN = 14
TEMP_MAX = 30
VALID_SWING = ("on", "off")


def build_send(fan: str, mode: str, temp: int, swing: str) -> str:
    """Return a SEND line ready to write to the serial port (no newline)."""
    return f"SEND fan={fan} mode={mode} temp={temp} swing={swing}"


def parse_reply(line: str) -> tuple[str, str]:
    """Return (kind, rest) where kind is READY | SENT | ERR | PONG | UNKNOWN."""
    line = line.strip()
    if line.startswith("READY"):
        return "READY", line[len("READY"):].strip()
    if line.startswith("SENT"):
        return "SENT", line[len("SENT"):].strip()
    if line.startswith("ERR"):
        return "ERR", line[len("ERR"):].strip()
    if line == "PONG":
        return "PONG", ""
    return "UNKNOWN", line
