"""Backend: one Protocol class + SerialBackend and MockBackend implementations."""

from __future__ import annotations

import asyncio
from typing import AsyncIterator, Protocol

import serial_asyncio  # type: ignore[import-untyped]

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
        self._reader: asyncio.StreamReader | None = None
        self._writer: asyncio.StreamWriter | None = None

    async def connect(self) -> str:
        self._reader, self._writer = await serial_asyncio.open_serial_connection(
            url=self._port, baudrate=self._baud
        )
        # Drain any boot banner (READY line)
        version = ""
        try:
            raw = await asyncio.wait_for(self._reader.readline(), timeout=2.0)
            kind, rest = parse_reply(raw.decode(errors="replace"))
            if kind == "READY":
                version = rest
        except asyncio.TimeoutError:
            pass

        # Liveness check
        await self._writeline("PING")
        raw = await asyncio.wait_for(self._reader.readline(), timeout=2.0)
        kind, _ = parse_reply(raw.decode(errors="replace"))
        if kind != "PONG":
            raise ConnectionError(f"Expected PONG, got: {raw!r}")

        return version

    async def send(self, fan: str, mode: str, temp: int, swing: str) -> tuple[str, str]:
        assert self._reader and self._writer
        await self._writeline(build_send(fan, mode, temp, swing))
        raw = await asyncio.wait_for(self._reader.readline(), timeout=5.0)
        return parse_reply(raw.decode(errors="replace"))

    async def close(self) -> None:
        if self._writer:
            self._writer.close()
            await self._writer.wait_closed()

    async def _writeline(self, line: str) -> None:
        assert self._writer
        self._writer.write((line + "\n").encode())
        await self._writer.drain()


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
