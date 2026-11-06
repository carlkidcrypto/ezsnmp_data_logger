"""Unit tests for standalone desktop application packaging (Windows EXE, macOS DMG, Linux DEB & RPM).

Validates:
- [PKG-07]: Windows standalone single-file executable and Inno Setup installer.
- [PKG-08]: macOS application bundle and compressed disk image (DMG).
- [PKG-09]: Linux standalone executable, Debian (.deb), Red Hat (.rpm), and portable tarball.
- [CICD-05]: WinGet community distribution workflow and Microsoft wingetcreate submission.
- [CICD-12]: Desktop release packaging workflow and GitHub Releases distribution.
"""

from __future__ import annotations

import sys
from pathlib import Path
from unittest.mock import patch

import pytest

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT))

from ezsnmp_data_logger.entry_desktop import CLI_SUBCOMMANDS, main as entry_desktop_main
from tools.package_desktop_app import (
    build_pyinstaller_cmd,
    package_linux_artifacts,
    read_version,
    resolve_platform,
)


def _flush_libc() -> None:
    try:
        import ctypes

        ctypes.CDLL(None).fflush(None)
    except Exception:
        pass


def test_desktop_entry_dispatch_cli_subcommands(capfd: pytest.CaptureFixture[str]) -> None:
    """Verify that entry_desktop.py forwards CLI subcommands directly to CliRunner."""
    # Test 'version' subcommand
    ret_version = entry_desktop_main(["version"])
    _flush_libc()
    captured_ver = capfd.readouterr()
    assert ret_version == 0
    assert "ezsnmp-data-logger" in captured_ver.out
    assert "2026.11" in captured_ver.out

    # Test explicit 'cli version'
    ret_cli_ver = entry_desktop_main(["cli", "version"])
    _flush_libc()
    captured_cli_ver = capfd.readouterr()
    assert ret_cli_ver == 0
    assert "ezsnmp-data-logger" in captured_cli_ver.out

    # Test 'help' subcommand
    ret_help = entry_desktop_main(["help"])
    _flush_libc()
    captured_help = capfd.readouterr()
    assert ret_help == 0
    assert "Usage:" in captured_help.out


def test_desktop_entry_dispatches_gui_when_empty() -> None:
    """Verify that entry_desktop.py defaults to launching the GUI when invoked without args."""
    with patch("ezsnmp_data_logger.gui.main", return_value=0) as mock_gui:
        ret = entry_desktop_main([])
        assert ret == 0
        mock_gui.assert_called_once_with([])


def test_package_desktop_pyinstaller_cmd_assembly() -> None:
    """Verify PyInstaller command construction for both macOS and Windows."""
    version = read_version()
    dist_dir = REPO_ROOT / "dist"
    build_dir = REPO_ROOT / "build_pyinstaller"

    # macOS assembly
    cmd_macos = build_pyinstaller_cmd("macos", version, dist_dir, build_dir)
    cmd_macos_str = " ".join(cmd_macos)
    assert "--onedir" in cmd_macos
    assert "--windowed" in cmd_macos
    assert "EzSnmpDataLogger" in cmd_macos
    assert "com.carlkidcrypto.ezsnmp-data-logger" in cmd_macos
    assert "app_icon.icns" in cmd_macos_str
    assert "entry_desktop.py" in cmd_macos[-1]

    # Windows assembly
    cmd_win = build_pyinstaller_cmd("windows", version, dist_dir, build_dir)
    cmd_win_str = " ".join(cmd_win)
    assert "--onefile" in cmd_win
    assert "EzSnmpDataLogger" in cmd_win
    assert "app_icon.ico" in cmd_win_str
    assert "entry_desktop.py" in cmd_win[-1]

    # Linux assembly
    cmd_linux = build_pyinstaller_cmd("linux", version, dist_dir, build_dir)
    cmd_linux_str = " ".join(cmd_linux)
    assert "--onefile" in cmd_linux
    assert "EzSnmpDataLogger" in cmd_linux
    assert "app_icon.png" in cmd_linux_str
    assert "entry_desktop.py" in cmd_linux[-1]


def test_package_linux_artifacts_deb_and_tarball(tmp_path: Path) -> None:
    """Verify that package_linux_artifacts builds Linux binary copies, tarball, and .deb packages."""
    # Create mock PyInstaller executable
    mock_bin = tmp_path / "EzSnmpDataLogger"
    mock_bin.write_text("#!/bin/sh\necho 'EzSnmpDataLogger'\n", encoding="utf-8")
    mock_bin.chmod(0o755)

    artifacts = package_linux_artifacts(
        dist_dir=tmp_path,
        version="2026.11.6.1",
        arch="x64",
        skip_deb=False,
        skip_rpm=True,
    )

    names = [a.name for a in artifacts]
    assert "EzSnmpDataLogger-Linux-x64" in names
    assert "EzSnmpDataLogger-Linux-x64.tar.gz" in names
    assert "EzSnmpDataLogger-Linux-x64.deb" in names
    assert "ezsnmp-data-logger_2026.11.6.1_amd64.deb" in names

    # Verify tarball contents
    tar_path = tmp_path / "EzSnmpDataLogger-Linux-x64.tar.gz"
    import tarfile

    with tarfile.open(tar_path, "r:gz") as tar:
        tar_names = tar.getnames()
        assert "EzSnmpDataLogger" in tar_names
        assert "ezsnmp-data-logger.desktop" in tar_names

    # Verify Debian package structure (ar archive header)
    deb_path = tmp_path / "EzSnmpDataLogger-Linux-x64.deb"
    with open(deb_path, "rb") as f:
        magic = f.read(8)
        assert magic == b"!<arch>\n"
        rest = f.read()
        assert b"debian-binary" in rest
        assert b"control.tar" in rest
        assert b"data.tar" in rest


def test_inno_setup_script_structure() -> None:
    """Verify that tools/windows_installer.iss contains required Inno Setup keys and aliases."""
    iss_file = REPO_ROOT / "tools" / "windows_installer.iss"
    assert iss_file.is_file(), "Missing tools/windows_installer.iss"

    content = iss_file.read_text(encoding="utf-8")
    assert "AppId={{E5D4B692-7E5A-483A-9E86-443E838F5C1B}" in content
    assert "AppName=EzSnmp Data Logger" in content
    assert "OutputBaseFilename=EzSnmpDataLogger-Windows-x64-Setup" in content
    assert "EzSnmpDataLogger.exe" in content
    assert "ezsnmp-data-logger.exe" in content
    assert "desktopicon" in content
    assert "envPath" in content


def test_desktop_distribution_workflow_configuration() -> None:
    """Verify that .github/workflows/build_and_publish_desktop_apps.yml is correctly configured."""
    wf_path = REPO_ROOT / ".github" / "workflows" / "build_and_publish_desktop_apps.yml"
    assert wf_path.is_file(), "Missing build_and_publish_desktop_apps.yml"

    content = wf_path.read_text(encoding="utf-8")
    assert "release:" in content
    assert "types: [published]" in content
    assert "pull_request:" in content
    assert "workflow_dispatch:" in content
    assert "tj-actions/changed-files" in content
    assert "carlkidcrypto/os-specific-runner" in content
    assert "package-desktop-apps:" in content
    assert "ubuntu-latest" in content
    assert "macos-latest" in content
    assert "windows-latest" in content
    assert "dist/*.deb" in content
    assert "dist/*.rpm" in content
    assert "dist/*.tar.gz" in content
    assert "gh release upload" in content


def test_winget_distribution_workflow_configuration() -> None:
    """Verify that .github/workflows/build_and_publish_to_winget.yml conforms to [CICD-05]."""
    wf_path = REPO_ROOT / ".github" / "workflows" / "build_and_publish_to_winget.yml"
    assert wf_path.is_file(), "Missing build_and_publish_to_winget.yml"

    content = wf_path.read_text(encoding="utf-8")
    assert "release:" in content
    assert "types: [published]" in content
    assert "pull_request:" in content
    assert "workflow_dispatch:" in content
    assert "publish-winget:" in content
    assert "windows-latest" in content
    assert "generate_winget_manifest.py" in content
    assert "actions/upload-artifact@v7" in content
    assert "wingetcreate.exe" in content
    assert "https://aka.ms/wingetcreate/latest" in content
    assert "wingetcreate.exe submit" in content
    assert "wingetcreate.exe update" in content
    assert "WINGET_CREATE_PAT" in content
    assert "WINGET_TOKEN" in content
    assert "WINGET_ACC_TOKEN" in content
