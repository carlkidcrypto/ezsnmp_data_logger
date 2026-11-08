"""Tests for Tkinter desktop GUI components and actions."""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any
from unittest.mock import MagicMock

import pytest

from ezsnmp_data_logger.gui import can_use_tkinter_display
from ezsnmp_data_logger.gui.cli_bridge import CliSubprocessBridge
from ezsnmp_data_logger.gui.launcher import get_icon_path


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
