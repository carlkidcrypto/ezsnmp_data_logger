"""Tests for CLI subprocess bridge and streaming process lifecycle."""

from __future__ import annotations

import json
import os
import subprocess
import sys
import threading
from pathlib import Path
from typing import Any
from unittest.mock import MagicMock

import pytest

from ezsnmp_data_logger.gui.cli_bridge import (
    CliSubprocessBridge,
    resolve_cli_command,
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
    }
    cfg.write_text(json.dumps(doc), encoding="utf-8")
    return cfg


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
    def make_fake_proc():
        p = MagicMock()
        p.pid = 1234
        p.poll.return_value = None
        p.wait.return_value = 0
        p.returncode = 0
        p.stdout = iter([])
        p.stderr = iter([])
        return p

    bridge_win = CliSubprocessBridge(cfg)
    bridge_win.cli_base_command = lambda: [
        "ezsnmp-data-logger"
    ]  # Avoid WindowsPath instantiation on macOS
    monkeypatch.setattr(os, "name", "nt")
    monkeypatch.setattr("signal.CTRL_BREAK_EVENT", 99, raising=False)

    # Windows start_run with CREATE_NEW_PROCESS_GROUP creationflag
    p_spawn = make_fake_proc()
    monkeypatch.setattr("subprocess.Popen", lambda *args, **kwargs: p_spawn)
    bridge_win.start_run()
    bridge_win.stop_run()

    # Windows CTRL_BREAK_EVENT
    p_ctrl = make_fake_proc()
    bridge_win._run_proc = p_ctrl
    bridge_win.stop_run()
    p_ctrl.send_signal.assert_called_with(99)

    # Windows send_signal OSError fallback to terminate()
    p_err = make_fake_proc()
    p_err.send_signal.side_effect = OSError("Failed to send CTRL_BREAK")
    bridge_win._run_proc = p_err
    bridge_win.stop_run()
    p_err.terminate.assert_called()

    # Windows without CTRL_BREAK_EVENT fallback to terminate()
    p_no_break = make_fake_proc()
    monkeypatch.setattr("signal.CTRL_BREAK_EVENT", None, raising=False)
    bridge_win._run_proc = p_no_break
    bridge_win.stop_run()
    p_no_break.terminate.assert_called()

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
