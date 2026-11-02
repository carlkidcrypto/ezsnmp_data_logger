"""Tests for the behind-the-scenes CLI subprocess bridge and cross-platform GUIs."""

from __future__ import annotations

import json
import threading
import time
import urllib.request
from pathlib import Path

from ezsnmp_data_logger.gui import can_use_tkinter_display, select_frontend
from ezsnmp_data_logger.gui.cli_bridge import CliSubprocessBridge
from ezsnmp_data_logger.gui.server import create_web_server


def _make_gui_test_config(tmp_path: Path) -> Path:
    cfg_path = tmp_path / "gui_config.json"
    doc = {
        "logger": {
            "poll_interval_ms": 40,
            "queue_depth": 1000,
            "worker_threads": 2,
            "stats_interval_ms": 200,
        },
        "duts": [
            {
                "name": "gui-switch-01",
                "dut_ip": "127.0.0.1",
                "enabled": True,
                "snmp": {"mode": "stub", "version": "2c", "community": "public"},
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
                "path": str(tmp_path / "gui.sqlite"),
                "commit_every_n": 1,
                "commit_every_ms": 100,
            }
        ],
        "gui": {
            "frontend": "auto",
            "host": "127.0.0.1",
            "port": 0,
            "theme": "dark",
            "window_width": 960,
            "window_height": 640,
            "refresh_interval_ms": 500,
            "open_browser": False,
        },
    }
    cfg_path.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
    return cfg_path


def test_cli_subprocess_bridge_all_commands_and_streaming(tmp_path: Path) -> None:
    cfg_path = _make_gui_test_config(tmp_path)
    bridge = CliSubprocessBridge(cfg_path)

    assert bridge.validate()["data"]["valid"] is True
    assert bridge.probe()["data"]["count"] == 1

    req = bridge.request("gui-switch-01", "get", ["1.3.6.1.2.1.1.3.0"], record=True)
    assert req["ok"] is True
    assert req["data"]["recorded_to_sinks"] is True

    set_res = bridge.set_oid("gui-switch-01", "1.3.6.1.2.1.1.5.0", "s", "lab-sw", record=True)
    assert set_res["ok"] is True

    # Streaming CLI `run --json` process start -> collect live events -> stop cleanly
    start_info = bridge.start_run()
    assert start_info["ok"] is True

    deadline = time.monotonic() + 4.0
    while time.monotonic() < deadline:
        snap = bridge.snapshot()
        if snap["stats"]["processed_batches"] >= 2 and len(snap["series"]) >= 1:
            break
        time.sleep(0.05)

    stop_info = bridge.stop_run()
    assert stop_info["ok"] is True
    assert bridge.is_running() is False

    snap = bridge.snapshot()
    assert snap["stats"]["processed_batches"] >= 1
    assert len(snap["audit"]) >= 5

    insp = bridge.inspect(limit=20, dut="gui-switch-01")
    assert insp["ok"] is True
    assert insp["data"]["count"] >= 2


def test_web_studio_http_server_endpoints(tmp_path: Path) -> None:
    cfg_path = _make_gui_test_config(tmp_path)
    bridge = CliSubprocessBridge(cfg_path)
    srv = create_web_server(bridge, host="127.0.0.1", port=0)
    host, port = srv.server_address[:2]
    base_url = f"http://{host}:{port}"

    thread = threading.Thread(target=srv.serve_forever, daemon=True)
    thread.start()
    try:
        # Static HTML
        with urllib.request.urlopen(f"{base_url}/") as resp:
            html = resp.read().decode("utf-8")
            assert "EzSnmp Data Logger Studio" in html

        # Status JSON
        with urllib.request.urlopen(f"{base_url}/api/status") as resp:
            status_obj = json.loads(resp.read().decode("utf-8"))
            assert status_obj["ok"] is True

        # Trigger single cycle via POST /api/run/once
        post_req = urllib.request.Request(
            f"{base_url}/api/run/once",
            data=b"{}",
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(post_req) as resp:
            once_obj = json.loads(resp.read().decode("utf-8"))
            assert once_obj["ok"] is True

        # Inspect stored rows via GET /api/inspect
        with urllib.request.urlopen(f"{base_url}/api/inspect?limit=10") as resp:
            insp_obj = json.loads(resp.read().decode("utf-8"))
            assert insp_obj["ok"] is True
            assert insp_obj["data"]["count"] >= 2
    finally:
        bridge.stop_run()
        srv.shutdown()
        srv.server_close()


def test_frontend_selection_and_optional_tkinter_desktop(tmp_path: Path) -> None:
    assert select_frontend("web") == "web"
    assert select_frontend("tk") == "tk"
    assert select_frontend("auto") in ("tk", "web")

    if not can_use_tkinter_display():
        return

    from ezsnmp_data_logger.gui.desktop_tk import DesktopLoggerStudio

    cfg_path = _make_gui_test_config(tmp_path)
    bridge = CliSubprocessBridge(cfg_path)
    studio = DesktopLoggerStudio(bridge, schedule_refresh=False)
    try:
        studio.root.withdraw()
        studio.action_run_once()
        studio.action_validate_saved()
        studio.action_inspect_sinks()
        studio.refresh_live_state(reschedule=False)
        assert int(studio.kpi_vars["batches"].get()) >= 1
    finally:
        studio.on_close()
