"""Cross-platform application launcher installer with custom branding icons.

Supports:
- macOS: `.app` application bundle (`EzSnmp Data Logger Studio.app`) with Retina `.icns` icon.
- Linux: `.desktop` application menu entry (`ezsnmp-data-logger.desktop`) with PNG icon.
- Windows: Application shortcut / `.bat` launcher with `.ico` icon.
"""

from __future__ import annotations

import os
import shutil
import sys
from pathlib import Path

STATIC_DIR = Path(__file__).resolve().parent / "static"
if not STATIC_DIR.is_dir() and getattr(sys, "_MEIPASS", None):
    _cand = Path(sys._MEIPASS) / "ezsnmp_data_logger" / "gui" / "static"
    if _cand.is_dir():
        STATIC_DIR = _cand


def get_icon_path(kind: str = "png") -> Path:
    """Return the absolute path to the requested icon asset ('png', 'ico', 'icns', 'svg')."""
    kind_lower = kind.strip().lower().lstrip(".")
    mapping = {
        "png": STATIC_DIR / "app_icon.png",
        "png_32": STATIC_DIR / "app_icon_32.png",
        "png_64": STATIC_DIR / "app_icon_64.png",
        "png_128": STATIC_DIR / "app_icon_128.png",
        "ico": STATIC_DIR / "app_icon.ico",
        "icns": STATIC_DIR / "app_icon.icns",
        "svg": STATIC_DIR / "favicon.svg",
        "favicon_ico": STATIC_DIR / "favicon.ico",
    }
    return mapping.get(kind_lower, STATIC_DIR / "app_icon.png")


def install_desktop_launcher(target_dir: Path | None = None) -> Path:
    """Install a native desktop launcher with custom icon for the current platform."""
    if sys.platform == "darwin":
        return _install_macos_app_bundle(target_dir)
    if sys.platform == "win32":
        return _install_windows_shortcut(target_dir)
    return _install_linux_desktop_entry(target_dir)


def _install_macos_app_bundle(target_dir: Path | None = None) -> Path:
    """Create a macOS .app bundle with Retina icon."""
    base_dir = target_dir or (Path.home() / "Applications")
    base_dir.mkdir(parents=True, exist_ok=True)

    app_name = "EzSnmp Data Logger Studio.app"
    app_dir = base_dir / app_name
    contents_dir = app_dir / "Contents"
    macos_dir = contents_dir / "MacOS"
    resources_dir = contents_dir / "Resources"

    macos_dir.mkdir(parents=True, exist_ok=True)
    resources_dir.mkdir(parents=True, exist_ok=True)

    # 1. Copy Retina ICNS icon
    icns_src = STATIC_DIR / "app_icon.icns"
    if icns_src.is_file():
        shutil.copy2(icns_src, resources_dir / "app_icon.icns")

    # 2. Write Info.plist
    plist_content = """<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>EzSnmp Data Logger Studio</string>
    <key>CFBundleDisplayName</key>
    <string>EzSnmp Data Logger Studio</string>
    <key>CFBundleIdentifier</key>
    <string>com.carlkidcrypto.ezsnmp-data-logger-studio</string>
    <key>CFBundleVersion</key>
    <string>2026.11.7</string>
    <key>CFBundleShortVersionString</key>
    <string>2026.11.7</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleExecutable</key>
    <string>EzSnmp Data Logger Studio</string>
    <key>CFBundleIconFile</key>
    <string>app_icon.icns</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
"""
    (contents_dir / "Info.plist").write_text(plist_content, encoding="utf-8")

    # 3. Write executable script
    launcher_exec = macos_dir / "EzSnmp Data Logger Studio"
    script_content = f"""#!/bin/bash
# EzSnmp Data Logger Studio Launcher
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
exec "{sys.executable}" -m ezsnmp_data_logger.gui "$@"
"""
    launcher_exec.write_text(script_content, encoding="utf-8")
    launcher_exec.chmod(0o755)

    return app_dir


def _install_linux_desktop_entry(target_dir: Path | None = None) -> Path:
    """Create a freedesktop.org .desktop entry with custom PNG icon."""
    base_dir = target_dir or (Path.home() / ".local" / "share" / "applications")
    base_dir.mkdir(parents=True, exist_ok=True)

    desktop_file = base_dir / "ezsnmp-data-logger.desktop"
    icon_path = STATIC_DIR / "app_icon.png"

    content = f"""[Desktop Entry]
Version=1.0
Type=Application
Name=EzSnmp Data Logger Studio
GenericName=SNMP Data Logger
Comment=Cross-Platform SNMP Data Logger GUI Studio
Exec="{sys.executable}" -m ezsnmp_data_logger.gui %F
Icon={icon_path}
Terminal=false
Categories=Development;Network;System;
Keywords=snmp;telemetry;ezsnmp;data-logger;
StartupNotify=true
StartupWMClass=ezsnmp_data_logger
"""
    desktop_file.write_text(content, encoding="utf-8")
    desktop_file.chmod(0o755)
    return desktop_file


def _install_windows_shortcut(target_dir: Path | None = None) -> Path:
    """Create a Windows launcher batch / shortcut with custom ICO icon."""
    base_dir = target_dir or (Path.home() / "Desktop")
    base_dir.mkdir(parents=True, exist_ok=True)

    bat_file = base_dir / "EzSnmp Data Logger Studio.bat"
    ico_path = STATIC_DIR / "app_icon.ico"

    content = f"""@echo off
rem EzSnmp Data Logger Studio Launcher
start "" "{sys.executable}" -m ezsnmp_data_logger.gui %*
"""
    bat_file.write_text(content, encoding="utf-8")
    return bat_file
