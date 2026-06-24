#!/usr/bin/env -S uv run --quiet
# /// script
# requires-python = ">=3.11"
# dependencies = ["textual", "pyserial"]
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
from textual.widgets import Button, Footer, Header, Label, RichLog, Static
from textual.message import Message

from backend import MockBackend, SerialBackend
from protocol import TEMP_MAX, TEMP_MIN


FAN_OPTIONS   = [("Off", "0"), ("1", "1"), ("2", "2"), ("3", "3"),
                 ("4", "4"), ("5", "5"), ("Quiet", "quiet"), ("Auto", "auto")]
MODE_OPTIONS  = [("Fan", "fan"), ("Cool", "cool"), ("Heat", "heat"),
                 ("Dry", "dry"), ("Auto", "auto")]
SWING_OPTIONS = [("Off", "off"), ("On", "on")]

DEFAULT_FAN   = "0"
DEFAULT_MODE  = "fan"
DEFAULT_SWING = "off"
DEFAULT_TEMP  = 20


class OptionPicked(Message):
    """User picked an option in a group."""
    def __init__(self, group: str, value: str) -> None:
        super().__init__()
        self.group = group
        self.value = value


class TempChanged(Message):
    """User bumped the temperature."""
    def __init__(self, temp: int) -> None:
        super().__init__()
        self.temp = temp


class OptionGroup(Static):
    """Dumb renderer: shows one row of buttons, marks the one matching `current` green."""

    DEFAULT_CSS = """
    OptionGroup { height: auto; }
    OptionGroup Horizontal { height: auto; }
    OptionGroup Button { min-width: 7; margin-right: 1; }
    """

    def __init__(self, group: str, options: list[tuple[str, str]], **kwargs) -> None:
        super().__init__(id=group, **kwargs)
        self._group = group
        self._options = options

    def compose(self) -> ComposeResult:
        with Horizontal():
            for label, value in self._options:
                yield Button(label, id=f"{self._group}_{value}", variant="default")

    def render_state(self, current: str) -> None:
        for _, value in self._options:
            btn = self.query_one(f"#{self._group}_{value}", Button)
            btn.variant = "success" if value == current else "default"

    def on_button_pressed(self, event: Button.Pressed) -> None:
        prefix = f"{self._group}_"
        if not event.button.id or not event.button.id.startswith(prefix):
            return
        event.stop()
        self.post_message(OptionPicked(self._group, event.button.id[len(prefix):]))


class TempControl(Static):
    """− / value / + temperature selector. Posts TempChanged on bump."""

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

    def __init__(self, **kwargs) -> None:
        super().__init__(**kwargs)
        self._temp = TEMP_MIN

    def compose(self) -> ComposeResult:
        yield Button("  −  ", id="temp_down", variant="default")
        yield Label("-- °C", id="temp_label")
        yield Button("  +  ", id="temp_up", variant="default")

    def render_state(self, temp: int) -> None:
        self._temp = temp
        self.query_one("#temp_label", Label).update(f"{temp} °C")

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id not in ("temp_down", "temp_up"):
            return
        event.stop()
        delta = -1 if event.button.id == "temp_down" else 1
        new = max(TEMP_MIN, min(TEMP_MAX, self._temp + delta))
        if new != self._temp:
            self.post_message(TempChanged(new))


class DaikinApp(App):
    TITLE = "Daikin Remote"
    ALLOW_SELECT = False  # workaround for textual 8.x AssertionError on mouse drag
    CSS = """
    Screen { padding: 1 2; }

    #card_grid {
        layout: grid;
        grid-size: 2;
        grid-columns: 1fr 1fr;
        grid-gutter: 1 2;
        height: auto;
    }

    .card {
        border: round $accent;
        padding: 0 1 1 1;
        height: auto;
    }

    .card-title {
        text-style: bold;
        padding: 0 0 0 1;
        color: $accent;
    }

    #resend_btn { margin-top: 1; width: 100%; }

    #tx_log {
        height: 8;
        border: solid $accent;
        margin-top: 1;
    }

    #status { margin-top: 0; color: $text-muted; }
    """

    BINDINGS = [
        Binding("q", "quit", "Quit"),
        Binding("right", "focus_next",     show=False),
        Binding("left",  "focus_previous", show=False),
    ]

    def __init__(self, backend: MockBackend | SerialBackend) -> None:
        super().__init__()
        self._backend = backend
        self._connected = False
        self._state = {
            "fan":   DEFAULT_FAN,
            "mode":  DEFAULT_MODE,
            "swing": DEFAULT_SWING,
            "temp":  DEFAULT_TEMP,
        }

    def compose(self) -> ComposeResult:
        yield Header()
        with Grid(id="card_grid"):
            with Vertical(classes="card"):
                yield Label("FAN", classes="card-title")
                yield OptionGroup("fan", FAN_OPTIONS)
            with Vertical(classes="card"):
                yield Label("MODE", classes="card-title")
                yield OptionGroup("mode", MODE_OPTIONS)
            with Vertical(classes="card"):
                yield Label("SWING", classes="card-title")
                yield OptionGroup("swing", SWING_OPTIONS)
            with Vertical(classes="card"):
                yield Label("TEMP", classes="card-title")
                yield TempControl(id="temp")
        yield Button("▶  RESEND", id="resend_btn", variant="primary")
        yield RichLog(id="tx_log", markup=True)
        yield Label("connecting…", id="status")
        yield Footer()

    async def on_mount(self) -> None:
        self._render_all()
        self.run_worker(self._connect(), exclusive=True)

    def _render_all(self) -> None:
        self.query_one("#fan",   OptionGroup).render_state(self._state["fan"])
        self.query_one("#mode",  OptionGroup).render_state(self._state["mode"])
        self.query_one("#swing", OptionGroup).render_state(self._state["swing"])
        self.query_one("#temp",  TempControl).render_state(self._state["temp"])

    async def _connect(self) -> None:
        status = self.query_one("#status", Label)
        try:
            version = await self._backend.connect()
            self._connected = True
            status.update(f"connected — {version}" if version else "connected")
            self._log(f"← READY {version}")
        except Exception as exc:
            self._log(f"[red]connection failed: {exc}[/red]")
            status.update(f"[red]disconnected — {type(exc).__name__}[/red]")

    def on_option_picked(self, event: OptionPicked) -> None:
        if self._state.get(event.group) == event.value:
            return
        self._state[event.group] = event.value
        self.run_worker(self._do_send(), exclusive=True)

    def on_temp_changed(self, event: TempChanged) -> None:
        if self._state["temp"] == event.temp:
            return
        self._state["temp"] = event.temp
        self.run_worker(self._do_send(), exclusive=True)

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "resend_btn":
            self.run_worker(self._do_send(), exclusive=True)

    async def _do_send(self) -> None:
        if not self._connected:
            self._log("[red]not connected — use --port or --mock[/red]")
            return

        s = self._state
        self._log(f"→ SEND fan={s['fan']} mode={s['mode']} temp={s['temp']} swing={s['swing']}")
        try:
            kind, payload = await self._backend.send(s["fan"], s["mode"], s["temp"], s["swing"])
            if kind == "SENT":
                self._log(f"[green]← SENT {payload[:20]}…[/green]")
                self._render_all()
            else:
                self._log(f"[red]← {kind} {payload}[/red]")
        except Exception as exc:
            self._connected = False
            self._log(f"[red]serial error ({type(exc).__name__}): {exc}[/red]")
            self.query_one("#status", Label).update(f"[red]disconnected — {type(exc).__name__}[/red]")

    def _log(self, text: str) -> None:
        ts = datetime.now().strftime("%H:%M:%S")
        self.query_one("#tx_log", RichLog).write(f"{ts}  {text}")

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
