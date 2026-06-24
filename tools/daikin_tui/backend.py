"""Backend: one Protocol class + SerialBackend and MockBackend implementations."""

from __future__ import annotations

import asyncio
import serial  # type: ignore[import-untyped]
from typing import Protocol

from protocol import build_send, parse_reply


class Backend(Protocol):
    async def connect(self) -> str:
        """Open connection, perform PING, return version string."""
        ...

    async def send(self, fan: str, mode: str, temp: int, swing: str) -> tuple[str, str]:
        """Send SEND line, return (kind, payload) of the reply."""
        ...

    async def close(self) -> None: ...


class SerialBackend:
    def __init__(self, port: str, baud: int = 115200) -> None:
        self._port = port
        self._baud = baud
        self._ser: serial.Serial | None = None

    async def connect(self) -> str:
        return await asyncio.to_thread(self._connect_sync)

    def _connect_sync(self) -> str:
        # serial.Serial opens exactly like the Arduino IDE Serial Monitor does.
        # timeout=2 matches capture.py; the port open itself triggers the DTR
        # reset on CH340 — Serial waits for the board to reboot before reading.
        self._ser = serial.Serial(self._port, self._baud, timeout=2)

        # Drain boot banner
        version = ""
        while True:
            line = self._ser.readline().decode(errors="replace").strip()
            if not line:
                break  # timeout → no more queued lines
            kind, rest = parse_reply(line)
            if kind == "READY":
                version = rest

        # Liveness check
        self._ser.write(b"PING\n")
        self._ser.flush()
        line = self._ser.readline().decode(errors="replace").strip()
        kind, _ = parse_reply(line)
        if kind != "PONG":
            raise ConnectionError(f"Expected PONG, got: {line!r}")

        return version

    async def send(self, fan: str, mode: str, temp: int, swing: str) -> tuple[str, str]:
        return await asyncio.to_thread(self._send_sync, fan, mode, temp, swing)

    def _send_sync(self, fan: str, mode: str, temp: int, swing: str) -> tuple[str, str]:
        assert self._ser
        cmd = build_send(fan, mode, temp, swing) + "\n"
        self._ser.write(cmd.encode())
        self._ser.flush()
        self._ser.timeout = 5  # IR burst takes ~120 ms; give generous margin
        line = self._ser.readline().decode(errors="replace").strip()
        self._ser.timeout = 2
        return parse_reply(line)

    async def close(self) -> None:
        if self._ser:
            self._ser.close()


class MockBackend:
    """Simulates the 328P for UI development without hardware."""

    _MOCK_HEX = "11DA2700C50000D711DA27004200005411DA2700600000" + "0" * 26

    async def connect(self) -> str:
        await asyncio.sleep(0.05)
        return "daikin-serial/mock"

    async def send(self, fan: str, mode: str, temp: int, swing: str) -> tuple[str, str]:
        await asyncio.sleep(0.12)  # simulate ~120 ms IR burst
        return "SENT", self._MOCK_HEX

    async def close(self) -> None:
        pass
