"""Tests for CLI dispatch, desktop entrypoint, and GUI main command routing."""

from __future__ import annotations

import json
import runpy
import sys
from pathlib import Path
from typing import Any

import pytest

import ezsnmp_data_logger.cli as cli
import ezsnmp_data_logger.entry_desktop as entry_desktop
from ezsnmp_data_logger.gui import can_use_tkinter_display, main as gui_main, select_frontend


def _create_minimal_config(tmp_path: Path) -> Path:
    cfg = tmp_path / "config.json"
    doc = {
        "logger": {
            "poll_interval_ms": 100,
            "queue_depth": 500,
            "worker_threads": 2,
            "stats_interval_ms": 100,
        },
        "duts": [
            {
                "name": "switch-01",
                "dut_ip": "127.0.0.1",
                "enabled": True,
                "snmp": {
                    "mode": "stub",
                    "version": "2c",
                    "community": "public",
                    "port_number": 161,
                },
                "metrics": [
                    {"name": "sysUpTime", "oid": "1.3.6.1.2.1.1.3.0", "operation": "get"},
                    {"name": "cpuLoad", "oid": "1.3.6.1.4.1.2021.10.1.3.1", "operation": "get"},
                ],
            }
        ],
        "sinks": [
            {
                "type": "sqlite3",
                "enabled": True,
                "path": str(tmp_path / "telemetry.sqlite"),
                "commit_every_n": 1,
                "commit_every_ms": 50,
            }
        ],
        "gui": {
            "frontend": "auto",
            "host": "127.0.0.1",
            "port": 0,
            "theme": "dark",
            "max_log_lines": 200,
            "cli_executable": "",
        },
    }
    cfg.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
    return cfg


def test_package_and_gui_mains_and_runpy(monkeypatch: pytest.MonkeyPatch) -> None:
    # Safely mock C++ CliRunner.run to return exit code 0
    monkeypatch.setattr(
        "ezsnmp_data_logger.cli.CliRunner.run",
        staticmethod(lambda vec: 0),
    )
    monkeypatch.setattr(sys, "argv", ["ezsnmp-data-logger", "version"])

    # 1. ezsnmp_data_logger.__main__
    with pytest.raises(SystemExit) as exc:
        runpy.run_module("ezsnmp_data_logger", run_name="__main__")
    assert exc.value.code == 0

    # 2. ezsnmp_data_logger.gui.__main__
    monkeypatch.setattr("ezsnmp_data_logger.gui.main", lambda argv=None: 84)
    with pytest.raises(SystemExit) as exc:
        runpy.run_module("ezsnmp_data_logger.gui", run_name="__main__")
    assert exc.value.code == 84

    # 3. ezsnmp_data_logger.cli.__main__
    with pytest.raises(SystemExit) as exc:
        runpy.run_module("ezsnmp_data_logger.cli", run_name="__main__")
    assert exc.value.code == 0

    # 4. ezsnmp_data_logger.entry_desktop.__main__
    with pytest.raises(SystemExit) as exc:
        runpy.run_module("ezsnmp_data_logger.entry_desktop", run_name="__main__")
    assert exc.value.code == 0


def test_cli_dispatch_and_gui_forwarding(monkeypatch: pytest.MonkeyPatch) -> None:
    captured: list[list[str]] = []

    def mock_gui_main(argv: list[str] | None = None) -> int:
        captured.append(list(argv or []))
        return 99

    monkeypatch.setattr("ezsnmp_data_logger.gui.main", mock_gui_main)
    rc = cli.main(["gui", "--port", "1234"])
    assert rc == 99
    assert captured == [["--port", "1234"]]


def test_entry_desktop_all_branches(monkeypatch: pytest.MonkeyPatch) -> None:
    gui_captured: list[list[str]] = []
    cli_captured: list[list[str]] = []

    monkeypatch.setattr(
        "ezsnmp_data_logger.gui.main",
        lambda argv=None: (gui_captured.append(list(argv or [])), 1)[1],
    )
    monkeypatch.setattr(
        "ezsnmp_data_logger.cli.main",
        lambda argv=None: (cli_captured.append(list(argv or [])), 2)[1],
    )

    # Empty args
    assert entry_desktop.main([]) == 1
    assert gui_captured[-1] == []

    # macOS Finder process serial number filtered
    assert entry_desktop.main(["-psn_0_98765"]) == 1
    assert gui_captured[-1] == []

    # 'gui' command explicitly
    assert entry_desktop.main(["gui", "--frontend", "web"]) == 1
    assert gui_captured[-1] == ["--frontend", "web"]

    # Unknown arguments default to GUI
    assert entry_desktop.main(["--custom-option", "value"]) == 1
    assert gui_captured[-1] == ["--custom-option", "value"]

    # 'cli' subcommand
    assert entry_desktop.main(["cli", "run", "-c", "test.json"]) == 2
    assert cli_captured[-1] == ["run", "-c", "test.json"]

    # Direct CLI subcommand in CLI_SUBCOMMANDS
    for sub in ("run", "validate", "probe", "request", "set", "inspect", "schema", "version"):
        assert entry_desktop.main([sub, "--help"]) == 2
        assert cli_captured[-1] == [sub, "--help"]


def test_can_use_tkinter_display_and_select_frontend(monkeypatch: pytest.MonkeyPatch) -> None:
    # 1. Test tkinter import exception
    orig_import = __import__

    def mock_import(name: str, *args: Any, **kwargs: Any) -> Any:
        if name == "tkinter":
            raise ImportError("Mocked tkinter missing")
        return orig_import(name, *args, **kwargs)

    monkeypatch.setattr("builtins.__import__", mock_import)
    assert can_use_tkinter_display() is False

    # 2. Test win32 and darwin direct display support
    monkeypatch.undo()
    monkeypatch.setattr(sys, "platform", "win32")
    assert can_use_tkinter_display() is True
    monkeypatch.setattr(sys, "platform", "darwin")
    assert can_use_tkinter_display() is True

    # 3. Test Linux DISPLAY / WAYLAND_DISPLAY check
    monkeypatch.setattr(sys, "platform", "linux")
    monkeypatch.delenv("DISPLAY", raising=False)
    monkeypatch.delenv("WAYLAND_DISPLAY", raising=False)
    assert can_use_tkinter_display() is False
    assert select_frontend("auto") == "web"

    monkeypatch.setenv("DISPLAY", ":0")
    assert can_use_tkinter_display() is True
    assert select_frontend("auto") == "tk"

    monkeypatch.delenv("DISPLAY", raising=False)
    monkeypatch.setenv("WAYLAND_DISPLAY", "wayland-0")
    assert can_use_tkinter_display() is True

    # 4. Explicit frontend selections
    assert select_frontend("tk") == "tk"
    assert select_frontend("web") == "web"


def test_gui_main_cli_dispatch(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    cfg = _create_minimal_config(tmp_path)

    # 1. Desktop launcher installation option
    launcher_dir = tmp_path / "installed_launcher"
    rc = gui_main(["--install-launcher", "--launcher-dir", str(launcher_dir)])
    assert rc == 0
    assert launcher_dir.exists()

    # 2. Tk frontend dispatch
    tk_called: list[Any] = []
    monkeypatch.setattr(
        "ezsnmp_data_logger.gui.desktop_tk.launch_desktop_gui",
        lambda bridge, locale_mgr=None: (tk_called.append(bridge), 11)[1],
    )
    rc_tk = gui_main(["-c", str(cfg), "--frontend", "tk", "--locale", "en"])
    assert rc_tk == 11
    assert len(tk_called) == 1

    # 3. Tk fallback to web studio if launch_desktop_gui raises
    web_called: list[dict[str, Any]] = []

    def mock_launch_desktop_raise(bridge: Any, locale_mgr: Any = None) -> int:
        raise RuntimeError("No Tk display available")

    def mock_launch_web(
        bridge: Any,
        host: str = "127.0.0.1",
        port: int = 8765,
        open_browser: bool = True,
        locale_mgr: Any = None,
    ) -> int:
        web_called.append(
            {"host": host, "port": port, "open_browser": open_browser, "locale": locale_mgr.locale}
        )
        return 22

    monkeypatch.setattr(
        "ezsnmp_data_logger.gui.desktop_tk.launch_desktop_gui", mock_launch_desktop_raise
    )
    monkeypatch.setattr("ezsnmp_data_logger.gui.server.launch_web_gui", mock_launch_web)

    rc_fallback = gui_main(["-c", str(cfg), "--frontend", "tk"])
    assert rc_fallback == 22
    assert len(web_called) == 1

    # 4. Web frontend direct invocation with full arguments
    rc_web = gui_main(
        [
            "-c",
            str(cfg),
            "--frontend",
            "web",
            "--host",
            "0.0.0.0",
            "--port",
            "9999",
            "--no-browser",
            "--locale",
            "es",
        ]
    )
    assert rc_web == 22
    assert web_called[-1] == {
        "host": "0.0.0.0",
        "port": 9999,
        "open_browser": False,
        "locale": "es",
    }
