"""Comprehensive test suite to achieve 100% test coverage across ezsnmp_data_logger."""

from __future__ import annotations

import collections
import json
import os
import runpy
import signal
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from http import HTTPStatus
from pathlib import Path
from typing import Any
from unittest.mock import MagicMock

import pytest

import ezsnmp_data_logger.cli as cli
import ezsnmp_data_logger.entry_desktop as entry_desktop
from ezsnmp_data_logger.gui import can_use_tkinter_display, main as gui_main, select_frontend
from ezsnmp_data_logger.gui.cli_bridge import (
    CliSubprocessBridge,
    resolve_cli_command,
)
from ezsnmp_data_logger.gui.launcher import (
    _install_linux_desktop_entry,
    _install_macos_app_bundle,
    _install_windows_shortcut,
    get_icon_path,
    install_desktop_launcher,
)
from ezsnmp_data_logger.gui.locale_manager import (
    DEFAULT_LOCALE_PATH,
    LocaleManager,
    get_default_locale_manager,
)
from ezsnmp_data_logger.gui.server import (
    StudioHttpServer,
    create_web_server,
    launch_web_gui,
)


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


# =========================================================================
# Entrypoint & CLI modules (__main__.py, cli.py, entry_desktop.py)
# =========================================================================


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


# =========================================================================
# gui/__init__.py, launcher.py, locale_manager.py
# =========================================================================


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


def test_launcher_meipass_and_platform_shortcuts(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    # 1. MEIPASS static resolution
    fake_meipass = tmp_path / "meipass"
    fake_static = fake_meipass / "ezsnmp_data_logger" / "gui" / "static"
    fake_static.mkdir(parents=True, exist_ok=True)
    (fake_static / "app_icon.png").write_bytes(b"PNG_DATA")

    monkeypatch.setattr(sys, "_MEIPASS", str(fake_meipass), raising=False)
    # Test get_icon_path unknown kind fallback
    unknown_icon = get_icon_path("nonexistent_kind")
    assert unknown_icon.name == "app_icon.png"

    # 2. Test install_desktop_launcher on Windows platform
    monkeypatch.setattr(sys, "platform", "win32")
    win_out = tmp_path / "win_out"
    path_win = install_desktop_launcher(win_out)
    assert path_win.suffix == ".bat"

    # 3. Test install_desktop_launcher on Linux platform
    monkeypatch.setattr(sys, "platform", "linux")
    linux_out = tmp_path / "linux_out"
    path_linux = install_desktop_launcher(linux_out)
    assert path_linux.suffix == ".desktop"


def test_locale_manager_complete_coverage(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    # 1. MEIPASS check
    fake_meipass = tmp_path / "meipass_loc"
    fake_loc_dir = fake_meipass / "ezsnmp_data_logger" / "gui"
    fake_loc_dir.mkdir(parents=True, exist_ok=True)
    fake_loc_file = fake_loc_dir / "locale.json"
    fake_loc_file.write_text('{"key": {"en": "val"}}', encoding="utf-8")
    monkeypatch.setattr(sys, "_MEIPASS", str(fake_meipass), raising=False)

    # 2. Nonexistent file falls back to DEFAULT_LOCALE_PATH
    mgr = LocaleManager(locale="en", locale_file=tmp_path / "missing.json")
    assert mgr.locale_file == DEFAULT_LOCALE_PATH

    # 3. Corrupt JSON falls back to DEFAULT_LOCALE_PATH
    corrupt = tmp_path / "corrupt.json"
    corrupt.write_text("{not valid json", encoding="utf-8")
    mgr_corrupt = LocaleManager(locale="en", locale_file=corrupt)
    assert "buttons.start_logger" in mgr_corrupt._catalog

    # 4. If DEFAULT_LOCALE_PATH itself is corrupt or empty
    monkeypatch.setattr(
        "ezsnmp_data_logger.gui.locale_manager.DEFAULT_LOCALE_PATH", tmp_path / "empty.json"
    )
    mgr_empty = LocaleManager(locale="en", locale_file=corrupt)
    assert mgr_empty._catalog == {}

    # 5. Scalar entry in catalog, nested traversal, formatting error handling
    test_catalog = {
        "scalar_key": "Direct String Value",
        "normal_key": {
            "en": "English Normal",
            "es": "Spanish Normal",
        },
        "nested": {
            "sub": {
                "en": "English Sub {name}",
                "es": "Spanish Sub {name}",
            },
            "leaf_str": "Direct nested leaf",
        },
        "only_en": {"en": "Only English"},
        "flat_str": "Plain string for flat",
    }
    custom_loc = tmp_path / "catalog_test.json"
    custom_loc.write_text(json.dumps(test_catalog), encoding="utf-8")

    mgr_test = LocaleManager(locale="es", locale_file=custom_loc)
    assert "en" in mgr_test.supported_locales()
    assert "es" in mgr_test.supported_locales()

    mgr_test.set_locale("es")
    assert mgr_test.locale == "es"

    # Missing nested dotted path falls back to key
    assert mgr_test.t("nested.missing.path") == "nested.missing.path"
    assert mgr_test.t("missing_key", default="fallback_val") == "fallback_val"

    # Scalar entry lookup
    assert mgr_test.t("scalar_key") == "Direct String Value"

    # Nested dictionary traversal
    assert mgr_test.t("nested.sub", name="World") == "Spanish Sub World"
    # Fallback to English when key in dict lacks requested locale
    assert mgr_test.t("only_en") == "Only English"
    # Nested leaf string traversal
    assert mgr_test.t("nested.leaf_str") == "Direct nested leaf"

    # Formatting error catches exception and returns unformatted string
    assert mgr_test.t("nested.sub", bad_kwarg="x") == "Spanish Sub {name}"

    # get_locale_dict with fallback to "en" and string entry
    d_es = mgr_test.get_locale_dict("es")
    assert d_es["only_en"] == "Only English"
    assert d_es["flat_str"] == "Plain string for flat"


# =========================================================================
# cli_bridge.py complete coverage
# =========================================================================


def test_cli_bridge_complete_coverage(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    # 1. resolve_cli_command variants
    assert resolve_cli_command("my-custom-cli --flag") == ["my-custom-cli", "--flag"]

    # Frozen executable
    monkeypatch.setattr(sys, "frozen", True, raising=False)
    assert resolve_cli_command() == [sys.executable, "cli"]
    monkeypatch.delattr(sys, "frozen")

    # Local build binary
    repo_root = Path(__file__).resolve().parent.parent
    fake_build_bin = (
        repo_root
        / "build"
        / ("ezsnmp-data-logger.exe" if os.name == "nt" else "ezsnmp-data-logger")
    )
    fake_build_bin.parent.mkdir(parents=True, exist_ok=True)
    fake_build_bin.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
    fake_build_bin.chmod(0o755)
    try:
        assert resolve_cli_command() == [str(fake_build_bin)]
    finally:
        fake_build_bin.unlink()

    # Which CLI found on PATH
    monkeypatch.setattr(
        "shutil.which",
        lambda cmd: "/usr/local/bin/ezsnmp-data-logger" if cmd == "ezsnmp-data-logger" else None,
    )
    assert resolve_cli_command() == ["/usr/local/bin/ezsnmp-data-logger"]

    # Fallback to python module
    monkeypatch.setattr("shutil.which", lambda cmd: None)
    assert resolve_cli_command() == [sys.executable, "-m", "ezsnmp_data_logger.cli"]

    # 2. _ensure_config_exists when CLI init-config fails
    missing_cfg = tmp_path / "sub" / "auto_init.json"
    bridge_missing = CliSubprocessBridge(missing_cfg, cli_override="false")
    assert missing_cfg.is_file()
    assert bridge_missing.load_config_dict()["logger"]["poll_interval_ms"] == 1000

    # 3. _sync_gui_preferences with cli_executable and error handling
    pref_cfg = tmp_path / "prefs.json"
    pref_doc = {"gui": {"cli_executable": "custom-cli", "max_log_lines": 80}}
    pref_cfg.write_text(json.dumps(pref_doc), encoding="utf-8")
    bridge_pref = CliSubprocessBridge(pref_cfg)
    assert bridge_pref._cli_override == "custom-cli"

    # Corrupted config file during _sync_gui_preferences
    pref_cfg.write_text("{invalid json", encoding="utf-8")
    bridge_pref._sync_gui_preferences()  # Should not raise

    # 4. load_config_dict raises ValueError if root is not a dict
    bad_root_cfg = tmp_path / "bad_root.json"
    bad_root_cfg.write_text("[1, 2, 3]", encoding="utf-8")
    bridge_bad_root = CliSubprocessBridge(bad_root_cfg)
    with pytest.raises(ValueError, match="Root JSON configuration must be an object"):
        bridge_bad_root.load_config_dict()

    # 5. One-shot CLI operations and JSON parsing branches
    cfg = _create_minimal_config(tmp_path)
    bridge = CliSubprocessBridge(cfg)

    # Mock _exec_cli to verify each one-shot helper method
    recorded_cmds: list[list[str]] = []

    def mock_exec(cmd: list[str], timeout: float = 30.0) -> dict[str, Any]:
        recorded_cmds.append(cmd)
        return {"ok": True, "exit_code": 0, "stdout": "{}", "stderr": "", "data": {}, "audit": {}}

    monkeypatch.setattr(bridge, "_exec_cli", mock_exec)

    bridge.version_info()
    assert recorded_cmds[-1] == ["version", "--json"]

    bridge.schema()
    assert recorded_cmds[-1] == ["schema"]

    bridge.init_config_template("demo")
    assert recorded_cmds[-1] == ["init-config", "--template", "demo"]

    bridge.validate(config_text='{"test": 1}')
    assert recorded_cmds[-1] == ["validate", "--config-string", '{"test": 1}', "--json"]

    bridge.probe(dut_name="switch-01")
    assert recorded_cmds[-1] == [
        "probe",
        "--config",
        str(bridge.config_path),
        "--json",
        "--dut",
        "switch-01",
    ]

    bridge.inspect(limit=10, dut="sw1", metric="cpu")
    assert recorded_cmds[-1] == [
        "inspect",
        "--config",
        str(bridge.config_path),
        "--limit",
        "10",
        "--json",
        "--dut",
        "sw1",
        "--metric",
        "cpu",
    ]

    bridge.save_config_dict({"logger": {"poll_interval_ms": 50}})
    assert bridge.load_config_dict()["logger"]["poll_interval_ms"] == 50

    # 6. _exec_cli JSON decode error handling (invalid JSON output on stdout or stderr)
    monkeypatch.undo()
    bridge_fresh = CliSubprocessBridge(cfg)

    def mock_run_bad_json(*args: Any, **kwargs: Any) -> Any:
        m = MagicMock()
        m.returncode = 0
        m.stdout = "{bad stdout json"
        m.stderr = "{bad stderr json"
        return m

    monkeypatch.setattr("subprocess.run", mock_run_bad_json)
    res_bad_json = bridge_fresh._exec_cli(["dummy"])
    assert res_bad_json["data"] is None

    # Stderr valid json when stdout is empty
    def mock_run_stderr_json(*args: Any, **kwargs: Any) -> Any:
        m = MagicMock()
        m.returncode = 1
        m.stdout = ""
        m.stderr = '{"error": "from stderr"}'
        return m

    monkeypatch.setattr("subprocess.run", mock_run_stderr_json)
    res_stderr = bridge_fresh._exec_cli(["dummy"])
    assert res_stderr["data"] == {"error": "from stderr"}

    # Stderr non-json string
    def mock_run_stderr_non_json(*args: Any, **kwargs: Any) -> Any:
        m = MagicMock()
        m.returncode = 1
        m.stdout = ""
        m.stderr = "plain text error without json"
        return m

    monkeypatch.setattr("subprocess.run", mock_run_stderr_non_json)
    res_stderr_plain = bridge_fresh._exec_cli(["dummy"])
    assert res_stderr_plain["data"] is None

    # 7. Streaming background run management
    monkeypatch.undo()

    # start_run when already running
    start1 = bridge.start_run()
    assert start1["ok"] is True
    assert bridge.is_running() is True
    start2 = bridge.start_run()
    assert start2["already_running"] is True

    # stop_run
    stop1 = bridge.stop_run()
    assert stop1["ok"] is True
    assert bridge.is_running() is False
    stop2 = bridge.stop_run()
    assert stop2["stopped"] is True

    # Test Windows process group and signals simulation
    fake_proc = MagicMock()
    fake_proc.pid = 1234
    fake_proc.poll.return_value = None
    fake_proc.wait.return_value = 0
    fake_proc.returncode = 0
    fake_proc.stdout = iter([])
    fake_proc.stderr = iter([])
    monkeypatch.setattr("subprocess.Popen", lambda *args, **kwargs: fake_proc)

    bridge_win = CliSubprocessBridge(cfg)
    bridge_win.cli_base_command = lambda: [
        "ezsnmp-data-logger"
    ]  # Avoid WindowsPath instantiation on macOS
    monkeypatch.setattr(os, "name", "nt")
    monkeypatch.setattr("signal.CTRL_BREAK_EVENT", 99, raising=False)
    bridge_win.start_run()
    bridge_win.stop_run()
    fake_proc.send_signal.assert_called_with(99)

    # Windows send_signal OSError fallback to terminate()
    fake_proc.send_signal.side_effect = OSError("Failed to send CTRL_BREAK")
    bridge_win.start_run()
    bridge_win.stop_run()
    fake_proc.terminate.assert_called()

    # Windows without CTRL_BREAK_EVENT fallback to terminate()
    monkeypatch.setattr("signal.CTRL_BREAK_EVENT", None, raising=False)
    bridge_win.start_run()
    bridge_win.stop_run()
    fake_proc.terminate.assert_called()

    # Test stop_run TimeoutExpired -> proc.kill()
    fake_proc_timeout = MagicMock()
    fake_proc_timeout.pid = 9999
    fake_proc_timeout.poll.return_value = None
    fake_proc_timeout.wait.side_effect = [subprocess.TimeoutExpired(cmd="run", timeout=1.0), 0]
    bridge_win._run_proc = fake_proc_timeout
    bridge_win.stop_run()
    fake_proc_timeout.kill.assert_called()

    # 8. _ingest_batch_event and _read_run_stdout/_read_run_stderr events
    monkeypatch.undo()
    bridge_ev = CliSubprocessBridge(cfg)

    # run_once and request with walk/bulkwalk
    bridge_ev.run_once()
    bridge_ev.request("switch-01", "walk", ["1.3.6.1.2.1.1.1.0"])
    bridge_ev.set_oid("switch-01", "1.3.6.1.2.1.1.3.0", "t", "12345")

    # Batch event with type_tag 2 (f64), type_tag 1 (i64), and non-dict sample
    batch_ev = {
        "event": "batch",
        "batch": {
            "ts_unix_ns": 0,  # Fallback to time.time()
            "dut_name": "router1",
            "samples": [
                {"metric_name": "temp", "type_tag": 2, "f64": 45.5},
                {"metric_name": "int_cnt", "type_tag": 1, "i64": 100},
                "non-dict-sample",
            ],
        },
    }
    bridge_ev._ingest_batch_event(batch_ev["batch"])
    assert "router1::temp" in bridge_ev.snapshot()["series"]
    assert "router1::int_cnt" in bridge_ev.snapshot()["series"]

    # Stdout stream processing with started, stats, stopped, non-json, and non-dict
    mock_stream_proc = MagicMock()
    mock_stream_proc.stdout = iter(
        [
            "",  # empty line
            "not json\n",
            "[1, 2, 3]\n",  # non-dict
            json.dumps({"event": "started", "stats": {"uptime_ms": 100}}) + "\n",
            json.dumps({"event": "stats", "stats": {"cycles_completed": 5}}) + "\n",
            json.dumps({"event": "stopped", "stats": {"running": False}}) + "\n",
            json.dumps(batch_ev) + "\n",
        ]
    )
    mock_stream_proc.wait.return_value = 0
    bridge_ev._read_run_stdout(mock_stream_proc)
    assert bridge_ev._latest_stats["cycles_completed"] == 5

    # Stderr stream processing
    mock_stream_proc.stderr = iter(["Some warning line\n", ""])
    bridge_ev._read_run_stderr(mock_stream_proc)
    assert bridge_ev._latest_stats["last_error"] == "Some warning line"

    # None proc.stdout / proc.stderr guards
    mock_none_proc = MagicMock()
    mock_none_proc.stdout = None
    mock_none_proc.stderr = None
    bridge_ev._read_run_stdout(mock_none_proc)
    bridge_ev._read_run_stderr(mock_none_proc)


# =========================================================================
# server.py complete coverage
# =========================================================================


def test_server_complete_coverage(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    cfg = _create_minimal_config(tmp_path)
    bridge = CliSubprocessBridge(cfg)
    srv = create_web_server(bridge, host="127.0.0.1", port=0)
    host, port = srv.server_address[:2]
    base_url = f"http://{host}:{port}"

    thread = threading.Thread(target=srv.serve_forever, daemon=True)
    thread.start()

    try:
        # 0. GET /api/status and /api/locale
        with urllib.request.urlopen(f"{base_url}/api/status") as resp:
            data = json.loads(resp.read().decode("utf-8"))
            assert data["ok"] is True

        with urllib.request.urlopen(f"{base_url}/api/locale?locale=en") as resp:
            data = json.loads(resp.read().decode("utf-8"))
            assert data["ok"] is True
            assert data["locale"] == "en"

        # 1. GET /api/config
        with urllib.request.urlopen(f"{base_url}/api/config") as resp:
            data = json.loads(resp.read().decode("utf-8"))
            assert data["ok"] is True
            assert "config" in data

        # 2. GET /api/version
        with urllib.request.urlopen(f"{base_url}/api/version") as resp:
            data = json.loads(resp.read().decode("utf-8"))
            assert data["ok"] is True

        # 3. GET /api/schema
        with urllib.request.urlopen(f"{base_url}/api/schema") as resp:
            data = json.loads(resp.read().decode("utf-8"))
            assert data["ok"] is True

        # 4. GET /api/template
        with urllib.request.urlopen(f"{base_url}/api/template?name=demo") as resp:
            data = json.loads(resp.read().decode("utf-8"))
            assert data["ok"] is True
            assert data["template"] == "demo"

        # 5. GET /api/inspect with parameters
        with urllib.request.urlopen(
            f"{base_url}/api/inspect?limit=5&dut=switch-01&metric=sysUpTime"
        ) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            assert data["ok"] is True

        # 5b. GET /api/inspect invalid parameter causes exception -> 400
        with pytest.raises(urllib.error.HTTPError) as exc_get_bad:
            urllib.request.urlopen(f"{base_url}/api/inspect?limit=not_an_int")
        assert exc_get_bad.value.code == HTTPStatus.BAD_REQUEST

        # 6. GET static assets & 404 security handling
        for asset in ("index.html", "styles.css", "app.js", "app_icon.png"):
            with urllib.request.urlopen(f"{base_url}/{asset}") as resp:
                assert resp.status == 200

        with pytest.raises(urllib.error.HTTPError) as exc_404:
            urllib.request.urlopen(f"{base_url}/../../../nonexistent_file.txt")
        assert exc_404.value.code == HTTPStatus.NOT_FOUND

        # 7. POST /api/config with raw string
        post_cfg_raw = urllib.request.Request(
            f"{base_url}/api/config",
            data=json.dumps(
                {"raw": json.dumps({"logger": {"poll_interval_ms": 200}, "duts": [], "sinks": []})}
            ).encode("utf-8"),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(post_cfg_raw) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            assert data["ok"] is True

        # 8. POST /api/config with config object
        post_cfg_obj = urllib.request.Request(
            f"{base_url}/api/config",
            data=json.dumps(
                {"config": {"logger": {"poll_interval_ms": 300}, "duts": [], "sinks": []}}
            ).encode("utf-8"),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(post_cfg_obj) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            assert data["ok"] is True

        # 9. POST /api/config invalid body (neither raw nor config) -> 400
        post_cfg_bad = urllib.request.Request(
            f"{base_url}/api/config",
            data=b"{}",
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with pytest.raises(urllib.error.HTTPError) as exc_400:
            urllib.request.urlopen(post_cfg_bad)
        assert exc_400.value.code == HTTPStatus.BAD_REQUEST

        # 10. POST /api/validate
        post_val = urllib.request.Request(
            f"{base_url}/api/validate",
            data=json.dumps({"raw": json.dumps({"logger": {"poll_interval_ms": 100}})}).encode(
                "utf-8"
            ),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(post_val) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            assert "ok" in data

        # 11. POST /api/probe
        post_probe = urllib.request.Request(
            f"{base_url}/api/probe",
            data=json.dumps({"dut": "switch-01"}).encode("utf-8"),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(post_probe) as resp:
            assert resp.status == 200

        # 12. POST /api/request with string oids and list oids
        post_req_str = urllib.request.Request(
            f"{base_url}/api/request",
            data=json.dumps(
                {
                    "dut": "switch-01",
                    "operation": "get",
                    "oids": "1.3.6.1.2.1.1.3.0",
                    "record": True,
                }
            ).encode("utf-8"),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(post_req_str) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            assert "ok" in data

        # 13. POST /api/set
        post_set = urllib.request.Request(
            f"{base_url}/api/set",
            data=json.dumps(
                {
                    "dut": "switch-01",
                    "oid": "1.3.6.1.2.1.1.3.0",
                    "set_type": "t",
                    "value": "12345",
                    "record": True,
                }
            ).encode("utf-8"),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(post_set) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            assert "ok" in data

        # 14. POST /api/run/start and /api/run/stop
        post_start = urllib.request.Request(
            f"{base_url}/api/run/start",
            data=b"{}",
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(post_start) as resp:
            assert json.loads(resp.read().decode("utf-8"))["ok"] is True

        post_stop = urllib.request.Request(
            f"{base_url}/api/run/stop",
            data=b"{}",
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(post_stop) as resp:
            assert json.loads(resp.read().decode("utf-8"))["ok"] is True

        post_once = urllib.request.Request(
            f"{base_url}/api/run/once",
            data=b"{}",
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(post_once) as resp:
            assert json.loads(resp.read().decode("utf-8"))["ok"] is True

        # 15. POST /api/locale
        post_loc = urllib.request.Request(
            f"{base_url}/api/locale",
            data=json.dumps({"locale": "es"}).encode("utf-8"),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(post_loc) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            assert data["ok"] is True
            assert data["locale"] == "es"

        # 16. POST to unknown endpoint -> 404
        post_unknown = urllib.request.Request(
            f"{base_url}/api/nonexistent_endpoint",
            data=b"{}",
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with pytest.raises(urllib.error.HTTPError) as exc_unk:
            urllib.request.urlopen(post_unknown)
        assert exc_unk.value.code == HTTPStatus.NOT_FOUND

        # 17. POST non-dict body -> 400
        post_non_dict = urllib.request.Request(
            f"{base_url}/api/validate",
            data=b"[1, 2, 3]",
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with pytest.raises(urllib.error.HTTPError) as exc_non_dict:
            urllib.request.urlopen(post_non_dict)
        assert exc_non_dict.value.code == HTTPStatus.BAD_REQUEST

    finally:
        bridge.stop_run()
        srv.shutdown()
        srv.server_close()

    # 18. launch_web_gui with mocked serve_forever raising KeyboardInterrupt and browser error
    def mock_serve_forever(self: Any) -> None:
        raise KeyboardInterrupt()

    def mock_browser_open_error(url: str) -> bool:
        raise RuntimeError("No browser")

    monkeypatch.setattr(
        "ezsnmp_data_logger.gui.server.StudioHttpServer.serve_forever", mock_serve_forever
    )
    monkeypatch.setattr("webbrowser.open", mock_browser_open_error)
    rc_launch = launch_web_gui(bridge, host="127.0.0.1", port=0, open_browser=True)
    assert rc_launch == 0


# =========================================================================
# desktop_tk.py complete coverage
# =========================================================================


def test_desktop_tk_complete_coverage(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    if not can_use_tkinter_display():
        pytest.skip("Tkinter graphical display not available in current environment")

    import tkinter as tk
    from ezsnmp_data_logger.gui.desktop_tk import DesktopLoggerStudio, launch_desktop_gui

    cfg = _create_minimal_config(tmp_path)
    bridge = CliSubprocessBridge(cfg)

    studio = DesktopLoggerStudio(bridge, schedule_refresh=False)
    try:
        studio.root.withdraw()

        # 1. Dock and app icon branches
        assert DesktopLoggerStudio._set_macos_dock_icon(tmp_path / "nonexistent.png") is False
        real_png = get_icon_path("png")
        if sys.platform == "darwin":
            assert DesktopLoggerStudio._set_macos_dock_icon(real_png) is True

        # Test dock icon exception handling
        monkeypatch.setattr(
            "ctypes.cdll.LoadLibrary",
            lambda name: (_ for _ in ()).throw(RuntimeError("ctypes error")),
        )
        assert DesktopLoggerStudio._set_macos_dock_icon(real_png) is False
        monkeypatch.undo()

        # Test PhotoImage error in _apply_icon
        orig_photo = tk.PhotoImage
        monkeypatch.setattr(
            "tkinter.PhotoImage", lambda file=None: (_ for _ in ()).throw(RuntimeError("bad png"))
        )
        studio._apply_icon()
        monkeypatch.undo()

        # Test win32 icon branch simulation (normal & exception)
        monkeypatch.setattr(sys, "platform", "win32")
        monkeypatch.setattr(studio.root, "iconbitmap", lambda default=None: None)
        studio._apply_icon()
        monkeypatch.setattr(
            studio.root,
            "iconbitmap",
            lambda default=None: (_ for _ in ()).throw(RuntimeError("bad ico")),
        )
        studio._apply_icon()
        monkeypatch.undo()

        # 2. DUT forms and selection
        studio._populate_dut_form(-1)  # out-of-bounds guard
        assert studio._selected_dut_index == 0
        studio._populate_dut_form(0)
        studio.dut_listbox.selection_set(0)
        studio._on_dut_selected()

        # 3. Action: open config dialog
        monkeypatch.setattr("tkinter.filedialog.askopenfilename", lambda **kw: None)
        studio.action_open_config()
        monkeypatch.setattr("tkinter.filedialog.askopenfilename", lambda **kw: str(cfg))
        studio.action_open_config()

        # 4. Actions: start, stop, run once, save all
        studio.action_start_run()
        studio.action_stop_run()
        studio.action_run_once()
        studio.action_save_all()

        # 5. Actions: probe all & probe selected DUT
        studio.action_probe_all()
        studio.action_probe_selected_dut()

        # Probe population with samples
        sample_payload = {
            "count": 1,
            "results": [
                {
                    "samples": [
                        {
                            "oid": "1.3.6.1.2.1.1.3.0",
                            "oid_index": "0",
                            "snmp_type": "TIMETICKS",
                            "raw_value": "12345",
                            "type_name": "i64",
                            "status": 0,
                        }
                    ]
                }
            ],
        }
        studio._populate_request_results_from_probe(sample_payload)
        assert len(studio.req_tree.get_children()) >= 1

        # 6. Action: validate saved (passed & failed branches)
        studio.action_validate_saved()
        monkeypatch.setattr(
            studio.bridge,
            "validate",
            lambda *args, **kw: {
                "ok": False,
                "data": {"valid": False, "errors": ["Mock validation error"]},
            },
        )
        studio.action_validate_saved()
        assert "FAILED" in studio.json_status_var.get()
        monkeypatch.undo()

        # 7. Actions: add DUT, apply DUT, remove DUT
        # Out-of-bounds guards
        studio._selected_dut_index = 999
        studio.action_remove_dut()
        studio.action_apply_dut()
        studio.action_add_metric()
        studio.action_remove_metric()

        studio._selected_dut_index = 0
        studio.metrics_tree.selection_set([])
        studio.action_remove_metric()

        studio.action_add_dut()
        assert len(studio.config_doc.get("duts", [])) >= 2
        studio.dut_vars["name"].set("renamed-dut")
        studio.action_apply_dut()
        assert studio.config_doc["duts"][studio._selected_dut_index]["name"] == "renamed-dut"
        studio.action_remove_dut()

        # 8. Actions: add metric, remove metric
        studio.new_metric_name.set("sysContact")
        studio.new_metric_oid.set("1.3.6.1.2.1.1.4.0")
        studio.new_metric_op.set("get")
        studio.action_add_metric()
        children = studio.metrics_tree.get_children()
        assert len(children) >= 1
        studio.metrics_tree.selection_set(children[-1])
        studio.action_remove_metric()

        # 9. Action: execute ezsnmp request
        # Error: missing DUT or OID
        studio.req_dut_var.set("")
        studio.action_execute_ezsnmp_request()
        assert "Select a DUT" in studio.req_summary_var.get()

        # Valid GET
        studio.req_dut_var.set("switch-01")
        studio.req_op_var.set("get")
        studio.req_oid_var.set("1.3.6.1.2.1.1.3.0")
        studio.action_execute_ezsnmp_request()
        assert "OK:" in studio.req_summary_var.get()

        # Valid SET
        studio.req_op_var.set("set")
        studio.req_set_type_var.set("t")
        studio.req_set_val_var.set("99999")
        studio.action_execute_ezsnmp_request()

        # Simulated failure
        monkeypatch.setattr(
            studio.bridge,
            "request",
            lambda *a, **k: {
                "ok": False,
                "stderr": "Connection timed out",
                "data": {"error": "Timeout"},
            },
        )
        studio.req_op_var.set("get")
        studio.action_execute_ezsnmp_request()
        assert "Error:" in studio.req_summary_var.get()
        monkeypatch.undo()

        # 10. Action: apply settings tab (theme & locale changes, psql sink preservation)
        studio.config_doc.setdefault("sinks", []).append(
            {"type": "psql", "enabled": True, "conninfo": "host=localhost"}
        )
        studio.settings_vars["gui_theme"].set("light")
        studio.settings_vars["gui_locale"].set("es")
        studio.settings_vars["csv_enabled"].set(True)
        studio.settings_vars["jsonl_enabled"].set(True)
        studio.settings_vars["prom_enabled"].set(True)
        studio.action_apply_settings_tab()
        assert studio.theme_name == "light"

        # Switch back to dark and locale en
        studio.settings_vars["gui_theme"].set("dark")
        studio.settings_vars["gui_locale"].set("en")
        studio.action_apply_settings_tab()
        assert studio.theme_name == "dark"

        # 11. Action: inspect sinks (with existing tree item to cover delete branch)
        studio.inspect_tree.insert("", tk.END, values=("1", "sw", "m", "oid", "t", "v", "s"))
        studio.action_inspect_sinks()
        assert len(studio.inspect_tree.get_children()) >= 0

        # 12. Actions: template load, validate editor, save editor JSON
        studio.tpl_var.set("demo")
        studio.action_load_template()
        assert len(studio.json_editor.get("1.0", tk.END)) > 10

        studio.action_validate_editor()
        assert "PASSED" in studio.json_status_var.get()

        studio.json_editor.delete("1.0", tk.END)
        studio.json_editor.insert("1.0", "{bad json")
        studio.action_validate_editor()
        assert "FAILED" in studio.json_status_var.get()

        # Save editor invalid JSON syntax (with mocked messagebox)
        monkeypatch.setattr("tkinter.messagebox.showerror", lambda *a, **k: None)
        studio.action_save_editor_json()

        # Save editor valid JSON
        studio.action_load_template()
        studio.action_save_editor_json()
        assert "Saved JSON" in studio.json_status_var.get()

        # Save editor valid JSON with validation errors returned from CLI
        monkeypatch.setattr(
            studio.bridge,
            "save_config_text",
            lambda *a, **k: {
                "ok": False,
                "data": {"valid": False, "errors": ["Mock schema error"]},
            },
        )
        studio.action_save_editor_json()
        assert "Saved JSON with validation errors" in studio.json_status_var.get()
        monkeypatch.undo()

        # 13. Action: show schema
        studio.action_show_schema()

        # 14. Audit banner updates
        studio._update_audit_banner(None)
        studio._update_audit_banner({"command": "test", "exit_code": None, "duration_ms": 10})
        assert "running" in studio.audit_status_var.get()
        studio._update_audit_banner({"command": "test", "exit_code": 0, "duration_ms": 10})
        assert "[0," in studio.audit_status_var.get()

        # 15. Chart drawing with various series configurations
        # Empty
        studio._draw_chart({})
        # Single point series
        single_pt = {"router::cpu": [{"value": 42.0}]}
        studio._draw_chart(single_pt)
        # Identical values (vmin == vmax branch)
        identical_pts = {"router::cpu": [{"value": 50.0}, {"value": 50.0}]}
        studio._draw_chart(identical_pts)
        # Multiple series with different values
        multi_series = {
            "sw::cpu": [{"value": 10.0}, {"value": 20.0}],
            "sw::temp": [{"value": 35.0}, {"value": 45.0}],
        }
        studio._draw_chart(multi_series)

        # Series toggle and canvas click
        studio.action_select_all_series()
        studio._on_series_toggle()
        studio.action_clear_all_series()
        # Canvas click on legend item
        if studio._legend_hitboxes:
            box = studio._legend_hitboxes[0]
            fake_event = MagicMock()
            fake_event.x = (box[0] + box[2]) // 2
            fake_event.y = (box[1] + box[3]) // 2
            studio._on_canvas_click(fake_event)

        # 16. refresh_live_state
        studio.refresh_live_state(reschedule=False)

        # 17. on_close with active after_id (and after_cancel exception)
        studio._after_id = "after#1"
        monkeypatch.setattr(
            studio.root,
            "after_cancel",
            lambda aid: (_ for _ in ()).throw(RuntimeError("cancel failed")),
        )
        studio.on_close()
        monkeypatch.undo()
    finally:
        pass

    # 18. launch_desktop_gui with mocked mainloop and header icon exception
    monkeypatch.setattr(
        "tkinter.PhotoImage",
        lambda file=None: (_ for _ in ()).throw(RuntimeError("header icon fail")),
    )
    studio_no_icon = DesktopLoggerStudio(bridge, schedule_refresh=False)
    studio_no_icon.on_close()
    monkeypatch.undo()

    monkeypatch.setattr(
        "ezsnmp_data_logger.gui.desktop_tk.DesktopLoggerStudio.on_close", lambda self: None
    )
    monkeypatch.setattr("tkinter.Tk.mainloop", lambda self: None)
    rc_gui = launch_desktop_gui(bridge)
    assert rc_gui == 0
