#!/usr/bin/env -S uv run --quiet
# /// script
# requires-python = ">=3.11"
# dependencies = ["textual", "pyserial", "pyserial-asyncio"]
# ///
"""Daikin serial remote — Textual TUI.

Usage:
    uv run tools/daikin_tui/app.py --mock
    uv run tools/daikin_tui/app.py --port /dev/cu.usbserial-10
"""

from __future__ import annotations

import argparse
import sys
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from textual.app import App, ComposeResult
from textual.binding import Binding
from textual.containers import Grid, Horizontal, Vertical
from textual.widgets import Button, Footer, Header, Label, Log, RadioButton, RadioSet, Static


class NavRadioSet(RadioSet):
    """RadioSet that passes left/right focus to the next widget at boundaries."""

    def action_previous_button(self) -> None:
        if self._selected == 0:
            self.app.action_focus_previous()
        else:
            super().action_previous_button()

    def action_next_button(self) -> None:
        if self._selected == len(self._nodes) - 1:
            self.app.action_focus_next()
        else:
            super().action_next_button()

from backend import MockBackend, SerialBackend
from protocol import TEMP_MAX, TEMP_MIN


# Maps RadioSet index → protocol value, per control
FAN_VALUES  = ["0", "1", "2", "3", "4", "5", "quiet", "auto"]
MODE_VALUES = ["fan", "cool", "heat", "dry", "auto"]
SWING_VALUES = ["off", "on"]


class TempControl(Static):
    """− / value / + temperature selector."""

    DEFAULT_CSS = """
    TempControl {
        layout: horizontal;
        height: 3;
        align: left middle;
        margin-top: 1;
    }
    TempControl #temp_label {
        width: 8;
        content-align: center middle;
    }
    TempControl Button { min-width: 5; }
    """

    def __init__(self, initial: int = 24) -> None:
        super().__init__()
        self._temp = initial

    def compose(self) -> ComposeResult:
        yield Button("  −  ", id="temp_down", variant="default")
        yield Label(f"{self._temp} °C", id="temp_label")
        yield Button("  +  ", id="temp_up", variant="default")

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "temp_down" and self._temp > TEMP_MIN:
            self._temp -= 1
        elif event.button.id == "temp_up" and self._temp < TEMP_MAX:
            self._temp += 1
        self.query_one("#temp_label", Label).update(f"{self._temp} °C")
        event.stop()

    @property
    def value(self) -> int:
        return self._temp


class DaikinApp(App):
    TITLE = "Daikin Remote"
    CSS = """
    Screen { padding: 1 2; }

    /* two-column card grid */
    #card_grid {
        layout: grid;
        grid-size: 2;
        grid-columns: 1fr 1fr;
        grid-rows: auto auto;
        grid-gutter: 1 2;
        height: auto;
    }

    .card {
        border: round $accent;
        padding: 0 1;
        height: auto;
    }

    .card-title {
        text-style: bold;
        padding: 0 0 0 1;
        color: $accent;
    }

    RadioSet { border: none; height: auto; }
    RadioButton { height: 1; }

    /* bottom strip: temp + swing side by side */
    #bottom_strip {
        layout: grid;
        grid-size: 2;
        grid-columns: 1fr 1fr;
        grid-gutter: 1 2;
        height: auto;
        margin-top: 0;
    }

    #send_btn { margin-top: 1; width: 100%; }

    #tx_log {
        height: 8;
        border: solid $accent;
        margin-top: 1;
    }

    #status { margin-top: 0; color: $text-muted; }
    """

    BINDINGS = [
        Binding("q", "quit", "Quit"),
        Binding("s", "send", "Send"),
    ]

    def __init__(self, backend: MockBackend | SerialBackend) -> None:
        super().__init__()
        self._backend = backend
        self._connected = False

    def compose(self) -> ComposeResult:
        yield Header()

        with Grid(id="card_grid"):
            # ── FAN (top-left) ──────────────────────────────
            with Vertical(classes="card"):
                yield Label("FAN", classes="card-title")
                with NavRadioSet(id="fan_set"):
                    yield RadioButton("Off",   id="fan_0")
                    yield RadioButton("1",     id="fan_1")
                    yield RadioButton("2",     id="fan_2")
                    yield RadioButton("3",     id="fan_3", value=True)
                    yield RadioButton("4",     id="fan_4")
                    yield RadioButton("5",     id="fan_5")
                    yield RadioButton("Quiet", id="fan_quiet")
                    yield RadioButton("Auto",  id="fan_auto")

            # ── MODE (top-right) ────────────────────────────
            with Vertical(classes="card"):
                yield Label("MODE", classes="card-title")
                with NavRadioSet(id="mode_set"):
                    yield RadioButton("Fan",  id="mode_fan")
                    yield RadioButton("Cool", id="mode_cool", value=True)
                    yield RadioButton("Heat", id="mode_heat")
                    yield RadioButton("Dry",  id="mode_dry")
                    yield RadioButton("Auto", id="mode_auto")

        with Grid(id="bottom_strip"):
            # ── TEMP (bottom-left) ──────────────────────────
            with Vertical(classes="card"):
                yield Label("TEMP", classes="card-title")
                yield TempControl(initial=24)

            # ── SWING (bottom-right) ────────────────────────
            with Vertical(classes="card"):
                yield Label("SWING", classes="card-title")
                with NavRadioSet(id="swing_set"):
                    yield RadioButton("Off", id="swing_off", value=True)
                    yield RadioButton("On",  id="swing_on")

        yield Button("▶  SEND", id="send_btn", variant="primary")
        yield Log(id="tx_log", highlight=True)
        yield Label("connecting…", id="status")
        yield Footer()

    async def on_mount(self) -> None:
        self.run_worker(self._connect(), exclusive=True)

    async def _connect(self) -> None:
        status = self.query_one("#status", Label)
        try:
            version = await self._backend.connect()
            self._connected = True
            label = f"connected — {version}" if version else "connected"
            status.update(label)
            self._log(f"← READY {version}")
        except Exception as exc:
            status.update(f"[red]disconnected: {exc}[/red]")

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "send_btn":
            self.run_worker(self._do_send(), exclusive=True)
        elif event.button.id in ("temp_down", "temp_up"):
            self.run_worker(self._do_send(), exclusive=True)

    def on_radio_set_changed(self, event: RadioSet.Changed) -> None:
        self.run_worker(self._do_send(), exclusive=True)

    def action_send(self) -> None:
        self.run_worker(self._do_send(), exclusive=True)

    def _selected(self, set_id: str, values: list[str]) -> str:
        rs = self.query_one(f"#{set_id}", RadioSet)
        idx = rs.pressed_index
        return values[idx] if idx is not None and 0 <= idx < len(values) else values[0]

    async def _do_send(self) -> None:
        if not self._connected:
            self._log("[red]not connected[/red]")
            return

        fan   = self._selected("fan_set",   FAN_VALUES)
        mode  = self._selected("mode_set",  MODE_VALUES)
        swing = self._selected("swing_set", SWING_VALUES)
        temp  = self.query_one(TempControl).value

        self._log(f"→ SEND fan={fan} mode={mode} temp={temp} swing={swing}")
        try:
            kind, payload = await self._backend.send(fan, mode, temp, swing)
            if kind == "SENT":
                self._log(f"[green]← SENT {payload[:20]}…[/green]")
            else:
                self._log(f"[red]← {kind} {payload}[/red]")
        except Exception as exc:
            self._log(f"[red]error: {exc}[/red]")
            self._connected = False
            self.query_one("#status", Label).update(f"[red]disconnected: {exc}[/red]")

    def _log(self, text: str) -> None:
        ts = datetime.now().strftime("%H:%M:%S")
        self.query_one("#tx_log", Log).write_line(f"{ts}  {text}")

    async def on_unmount(self) -> None:
        await self._backend.close()


def main() -> None:
    p = argparse.ArgumentParser(description="Daikin serial remote TUI")
    group = p.add_mutually_exclusive_group()
    group.add_argument("--port", help="serial device, e.g. /dev/cu.usbserial-10")
    group.add_argument("--mock", action="store_true", help="use mock backend (no board needed)")
    p.add_argument("-b", "--baud", type=int, default=115200)
    args = p.parse_args()

    if args.mock or args.port is None:
        backend: MockBackend | SerialBackend = MockBackend()
    else:
        backend = SerialBackend(port=args.port, baud=args.baud)

    DaikinApp(backend).run()


if __name__ == "__main__":
    main()
