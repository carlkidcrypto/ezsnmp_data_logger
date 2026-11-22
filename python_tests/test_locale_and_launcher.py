"""Tests for desktop launcher shortcuts and locale manager."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import pytest

from ezsnmp_data_logger.gui.launcher import (
    get_icon_path,
    install_desktop_launcher,
)
from ezsnmp_data_logger.gui.locale_manager import (
    DEFAULT_LOCALE_PATH,
    LocaleManager,
)


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

    # 4. Test install_desktop_launcher on macOS platform uses writable WORK_DIR
    monkeypatch.setattr(sys, "platform", "darwin")
    mac_out = tmp_path / "mac_out"
    path_mac = install_desktop_launcher(mac_out)
    assert path_mac.suffix == ".app"
    launcher_script = (path_mac / "Contents" / "MacOS" / "EzSnmp Data Logger Studio").read_text(
        encoding="utf-8"
    )
    assert 'cd "$WORK_DIR"' in launcher_script
    assert str(Path(sys.executable).resolve().parent) in launcher_script

    # 5. Test resolve_default_config_path avoids read-only root '/'
    from ezsnmp_data_logger.gui import resolve_default_config_path

    monkeypatch.setattr(Path, "cwd", staticmethod(lambda: Path("/")))
    resolved = resolve_default_config_path()
    assert resolved == str(Path.home() / ".ezsnmp_data_logger" / "config.json")


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


def test_windows_shortcut_when_frozen(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    """Verify _install_windows_shortcut invokes sys.executable directly when frozen."""
    monkeypatch.setattr(sys, "platform", "win32")
    monkeypatch.setattr(sys, "frozen", True, raising=False)
    monkeypatch.setattr(
        sys, "executable", r"C:\Program Files\EzSnmpDataLogger\EzSnmpDataLogger.exe"
    )

    win_out = tmp_path / "frozen_win"
    bat_file = install_desktop_launcher(win_out)
    content = bat_file.read_text(encoding="utf-8")
    assert r'start "" "C:\Program Files\EzSnmpDataLogger\EzSnmpDataLogger.exe" %*' in content
    assert "-m ezsnmp_data_logger.gui" not in content


def test_resolve_default_config_path_frozen_and_readonly_windows(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Verify default GUI config path avoids read-only dirs on Windows / frozen builds."""
    import ezsnmp_data_logger.gui as gui_mod

    fake_appdata = tmp_path / "AppData" / "Roaming"
    monkeypatch.setenv("APPDATA", str(fake_appdata))
    monkeypatch.setattr(sys, "platform", "win32")

    # 1. Frozen executable always uses %APPDATA%\EzSnmpDataLogger\config.json
    monkeypatch.setattr(sys, "frozen", True, raising=False)
    assert Path(gui_mod.resolve_default_config_path()) == (
        fake_appdata / "EzSnmpDataLogger" / "config.json"
    )

    # 2. Non-frozen in non-writable Windows directory (e.g., Program Files) falls back to APPDATA
    monkeypatch.delattr(sys, "frozen")
    monkeypatch.setattr(gui_mod, "_is_dir_writable", lambda _p: False)
    assert Path(gui_mod.resolve_default_config_path()) == (
        fake_appdata / "EzSnmpDataLogger" / "config.json"
    )

    # 3. Non-frozen in writable dev checkout keeps ./configs/example.json unchanged
    monkeypatch.setattr(gui_mod, "_is_dir_writable", lambda _p: True)
    assert gui_mod.resolve_default_config_path() == "./configs/example.json"


def test_is_dir_writable_and_user_config_path_platforms(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Cover _is_dir_writable edge cases, _user_config_path on macOS/Linux, and cwd fallback."""
    import tempfile
    import ezsnmp_data_logger.gui as gui_mod

    # 1. _is_dir_writable walks up missing subdirectories and succeeds on writable parent
    assert gui_mod._is_dir_writable(tmp_path / "does_not_exist" / "nested") is True

    # 2. _is_dir_writable returns False for a regular file
    reg_file = tmp_path / "file.txt"
    reg_file.write_text("x", encoding="utf-8")
    assert gui_mod._is_dir_writable(reg_file) is False

    # 3. _is_dir_writable returns False when TemporaryFile raises OSError
    def _fail_temp(*_args, **_kwargs):
        raise OSError("read-only")

    monkeypatch.setattr(tempfile, "TemporaryFile", _fail_temp)
    assert gui_mod._is_dir_writable(tmp_path) is False

    # 4. _user_config_path on macOS and Linux (with and without XDG_CONFIG_HOME)
    import os

    monkeypatch.setattr(os, "name", "posix")
    monkeypatch.setattr(sys, "platform", "darwin")
    assert gui_mod._user_config_path() == (
        Path.home() / "Library" / "Application Support" / "EzSnmpDataLogger" / "config.json"
    )

    monkeypatch.setattr(sys, "platform", "linux")
    custom_xdg = tmp_path / "xdg_cfg"
    monkeypatch.setenv("XDG_CONFIG_HOME", str(custom_xdg))
    assert gui_mod._user_config_path() == custom_xdg / "EzSnmpDataLogger" / "config.json"

    monkeypatch.delenv("XDG_CONFIG_HOME", raising=False)
    assert gui_mod._user_config_path() == (
        Path.home() / ".config" / "EzSnmpDataLogger" / "config.json"
    )

    # 5. resolve_default_config_path in writable non-root cwd without configs/example.json
    monkeypatch.setattr(Path, "cwd", staticmethod(lambda: tmp_path))
    assert gui_mod.resolve_default_config_path() == gui_mod.DEFAULT_DEV_CONFIG_PATH
