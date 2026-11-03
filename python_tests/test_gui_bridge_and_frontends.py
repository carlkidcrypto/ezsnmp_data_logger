"""Tests for the behind-the-scenes CLI subprocess bridge and cross-platform GUIs."""

from __future__ import annotations

import json
import threading
import time
import urllib.request
from pathlib import Path

from ezsnmp_data_logger.gui import can_use_tkinter_display, select_frontend
from ezsnmp_data_logger.gui.cli_bridge import CliSubprocessBridge
from ezsnmp_data_logger.gui.locale_manager import LocaleManager, get_default_locale_manager
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

        # Locale API: GET /api/locale
        with urllib.request.urlopen(f"{base_url}/api/locale") as resp:
            loc_obj = json.loads(resp.read().decode("utf-8"))
            assert loc_obj["ok"] is True
            assert "en" in loc_obj["supported"]
            assert loc_obj["strings"]["buttons.start_logger"] == "Start CLI Logger"

        # Favicon and icon static assets
        with urllib.request.urlopen(f"{base_url}/favicon.ico") as resp:
            assert resp.status == 200
            assert resp.headers.get("Content-Type") == "image/x-icon"
            assert len(resp.read()) > 0

        with urllib.request.urlopen(f"{base_url}/favicon.svg") as resp:
            assert resp.status == 200
            assert resp.headers.get("Content-Type") == "image/svg+xml"
            assert b"<svg" in resp.read()
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

        # Interactive series selection controls on telemetry chart
        series_data = {
            "router1::cpu": [{"value": 12.5}, {"value": 15.0}],
            "router1::temp": [{"value": 42.0}, {"value": 43.5}],
            "router1::uptime": [{"value": 900000.0}, {"value": 900010.0}],
        }
        studio._draw_chart(series_data)
        assert len(studio._series_vars) >= 3
        assert studio._series_vars["router1::cpu"].get() is True

        # Clear all series
        studio.action_clear_all_series()
        assert all(v.get() is False for v in studio._series_vars.values())

        # Select only one series
        studio._series_vars["router1::temp"].set(True)
        studio._draw_chart(series_data)
        active = [k for k, v in studio._series_vars.items() if v.get()]
        assert active == ["router1::temp"]

        # Select all series
        studio.action_select_all_series()
        assert all(v.get() is True for v in studio._series_vars.values())
    finally:
        studio.on_close()

    # Localized studio with custom translation file
    custom_doc = {
        "buttons.start_logger": {
            "en": "Start CLI Logger",
            "es": "Iniciar Logger CLI",
        },
        "tabs.live_dashboard": {
            "en": "1. Live Dashboard",
            "es": "1. Panel en Vivo",
        },
    }
    custom_path = tmp_path / "custom_locale.json"
    custom_path.write_text(json.dumps(custom_doc), encoding="utf-8")

    es_mgr = LocaleManager(locale="es", locale_file=custom_path)
    studio_es = DesktopLoggerStudio(bridge, schedule_refresh=False, locale_mgr=es_mgr)
    try:
        studio_es.root.withdraw()
        assert "Panel en Vivo" in studio_es.locale.t("tabs.live_dashboard")
        assert studio_es.locale.t("buttons.start_logger") == "Iniciar Logger CLI"
    finally:
        studio_es.on_close()


def test_locale_manager_and_locale_json(tmp_path: Path) -> None:
    mgr = get_default_locale_manager()
    assert "en" in mgr.supported_locales()
    assert mgr.t("buttons.start_logger") == "Start CLI Logger"
    assert mgr.t("tabs.live_dashboard") == "1. Live Dashboard"
    assert mgr.get_locale_dict("en")["buttons.start_logger"] == "Start CLI Logger"

    # Formatting interpolation
    assert (
        mgr.t("app.window_title", filename="custom.json")
        == "EzSnmp Data Logger Studio — custom.json"
    )

    # Missing key falls back to default or key
    assert mgr.t("nonexistent.key.xyz", default="fallback") == "fallback"
    assert mgr.t("nonexistent.key.xyz") == "nonexistent.key.xyz"

    # Custom multi-language locale JSON file with format {"KEY": {"en": "...", "fr": "..."}}
    custom_doc = {
        "buttons.start_logger": {
            "en": "Start CLI Logger",
            "fr": "Démarrer Logger CLI",
        },
        "status_bar.ready": {
            "en": "Ready.",
            "fr": "Prêt.",
        },
    }
    custom_path = tmp_path / "custom_key_centric_locale.json"
    custom_path.write_text(json.dumps(custom_doc), encoding="utf-8")

    custom_mgr = LocaleManager(locale="fr", locale_file=custom_path)
    assert custom_mgr.supported_locales() == ["en", "fr"]
    assert custom_mgr.t("buttons.start_logger") == "Démarrer Logger CLI"
    assert custom_mgr.t("status_bar.ready") == "Prêt."

    # When requesting an unsupported language, falls back to English
    custom_mgr.set_locale("de")
    assert custom_mgr.t("buttons.start_logger") == "Start CLI Logger"


def test_icon_assets_and_desktop_launcher(tmp_path: Path) -> None:
    from ezsnmp_data_logger.gui import main as gui_main
    from ezsnmp_data_logger.gui.launcher import (
        _install_linux_desktop_entry,
        _install_macos_app_bundle,
        _install_windows_shortcut,
        get_icon_path,
        install_desktop_launcher,
    )

    # 1. Verify existence and readability of all key icon assets
    for kind in ("png", "png_32", "png_64", "ico", "icns", "svg", "favicon_ico"):
        icon_file = get_icon_path(kind)
        assert icon_file.is_file(), f"Icon asset for '{kind}' not found: {icon_file}"
        assert icon_file.stat().st_size > 0

    # 2. Verify platform desktop launcher installers
    # macOS .app bundle
    app_dir = _install_macos_app_bundle(tmp_path / "mac_apps")
    assert app_dir.is_dir()
    assert (app_dir / "Contents" / "Info.plist").is_file()
    assert (app_dir / "Contents" / "MacOS" / "EzSnmp Data Logger Studio").is_file()
    assert (app_dir / "Contents" / "Resources" / "app_icon.icns").is_file()

    # Linux .desktop entry
    desk_dir = tmp_path / "linux_apps"
    desktop_file = _install_linux_desktop_entry(desk_dir)
    assert desktop_file.is_file()
    assert "EzSnmp Data Logger Studio" in desktop_file.read_text(encoding="utf-8")
    assert "Icon=" in desktop_file.read_text(encoding="utf-8")

    # Windows .bat shortcut
    win_dir = tmp_path / "win_apps"
    bat_file = _install_windows_shortcut(win_dir)
    assert bat_file.is_file()
    assert "ezsnmp_data_logger.gui" in bat_file.read_text(encoding="utf-8")

    # Default platform installer
    launcher_path = install_desktop_launcher(target_dir=tmp_path / "default_launcher")
    assert launcher_path.exists()

    # CLI option --install-launcher
    cli_target = tmp_path / "cli_launcher"
    rc = gui_main(["--install-launcher", "--launcher-dir", str(cli_target)])
    assert rc == 0
    assert len(list(cli_target.iterdir())) > 0
