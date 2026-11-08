"""Tests for Studio HTTP server API endpoints and web GUI launcher."""

from __future__ import annotations

import json
import threading
import urllib.error
import urllib.parse
import urllib.request
from http import HTTPStatus
from pathlib import Path
from typing import Any

import pytest

from ezsnmp_data_logger.gui.cli_bridge import CliSubprocessBridge
from ezsnmp_data_logger.gui.server import (
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
    }
    cfg.write_text(json.dumps(doc), encoding="utf-8")
    return cfg


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
