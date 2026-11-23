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
    package_windows_artifacts,
    read_version,
    resolve_platform,
)


def _flush_libc() -> None:
    try:
        import ctypes

        if sys.platform == "win32":
            for lib in ("ucrtbase", "msvcrt"):
                try:
                    ctypes.CDLL(lib).fflush(None)
                except Exception:
                    pass
        else:
            ctypes.CDLL(None).fflush(None)
    except Exception:
        pass
    try:
        sys.stdout.flush()
        sys.stderr.flush()
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
        version="2026.11.22b0",
        arch="x64",
        skip_deb=False,
        skip_rpm=True,
    )

    names = [a.name for a in artifacts]
    assert "EzSnmpDataLogger-Linux-x64" in names
    assert "EzSnmpDataLogger-Linux-x64.tar.gz" in names
    assert "EzSnmpDataLogger-Linux-x64.deb" in names
    assert "ezsnmp-data-logger_2026.11.22b0_amd64.deb" in names

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
    assert '#define AppArch "x64"' in content
    assert "OutputBaseFilename=EzSnmpDataLogger-Windows-{#AppArch}-Setup" in content
    assert "ChangesEnvironment=yes" in content
    assert 'WorkingDir: "{userappdata}\\EzSnmpDataLogger"' in content
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
    assert "types: [created, published]" in content
    assert "pull_request:" in content
    assert "workflow_dispatch:" in content
    assert (
        "tj-actions/changed-files" in content
        or "carlkidcrypto/shared-workflows/actions/check-source-changes" in content
    )
    assert "carlkidcrypto/os-specific-runner" in content
    assert "package-desktop-apps:" in content
    assert "ubuntu-latest" in content
    assert "macos-latest" in content
    assert "windows-latest" in content
    assert "dist/*.deb" in content
    assert "dist/*.rpm" in content
    assert "dist/*.tar.gz" in content
    assert "gh release upload" in content
    assert "$ErrorActionPreference = 'Stop'" in content
    assert "pip install --pre" in content
    assert "C:\\Windows\\System32;C:\\Windows" in content
    assert "cli init-config" in content
    assert "cli validate" in content
    assert "cli run --once" in content
    assert "/api/version" in content


def test_windows_pyinstaller_dll_collection_and_deduplication(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Verify Windows DLL collection searches OpenSSL/Net-SNMP dirs and deduplicates netsnmp.dll."""
    fake_repo = tmp_path / "repo"
    pkg_dir = fake_repo / "ezsnmp_data_logger"
    static_dir = pkg_dir / "gui" / "static"
    static_dir.mkdir(parents=True)
    (pkg_dir / "entry_desktop.py").write_text("print('ok')\n", encoding="utf-8")
    (pkg_dir / "netsnmp.dll").write_bytes(b"pkg-netsnmp")

    netsnmp_bin = tmp_path / "netsnmp_bin"
    netsnmp_bin.mkdir()
    (netsnmp_bin / "netsnmp.dll").write_bytes(b"ext-netsnmp")
    (netsnmp_bin / "netsnmphelpers.dll").write_bytes(b"ext-helpers")

    openssl_bin = tmp_path / "openssl_bin"
    openssl_bin.mkdir()
    (openssl_bin / "libcrypto-3-x64.dll").write_bytes(b"crypto")
    (openssl_bin / "libssl-3-x64.dll").write_bytes(b"ssl")

    monkeypatch.setattr("tools.package_desktop_app.REPO_ROOT", fake_repo)
    monkeypatch.setenv("EZSNMP_NETSNMP_BIN_DIR", str(netsnmp_bin))
    monkeypatch.setenv("EZSNMP_OPENSSL_BIN_DIR", str(openssl_bin))

    cmd = build_pyinstaller_cmd("windows", "2026.11.18.1", tmp_path / "dist", tmp_path / "build")
    add_binaries = [cmd[i + 1] for i in range(len(cmd) - 1) if cmd[i] == "--add-binary"]
    bundled_names = [Path(entry.split(";")[0]).name.lower() for entry in add_binaries]

    assert bundled_names.count("netsnmp.dll") == 1
    assert "netsnmphelpers.dll" in bundled_names
    assert "libcrypto-3-x64.dll" in bundled_names
    assert "libssl-3-x64.dll" in bundled_names


def test_package_windows_artifacts_parameterizes_inno_arch(tmp_path: Path) -> None:
    """Verify package_windows_artifacts passes /DAppArch=<arch> and detects the arch Setup EXE."""
    exe_src = tmp_path / "EzSnmpDataLogger.exe"
    exe_src.write_bytes(b"MZfake-exe")

    captured_cmds: list[list[str]] = []

    def fake_run(cmd: list[str], check: bool = False) -> None:
        captured_cmds.append(cmd)
        (tmp_path / "EzSnmpDataLogger-Windows-arm64-Setup.exe").write_bytes(b"MZfake-setup")

    with (
        patch("tools.package_desktop_app.find_iscc_compiler", return_value="C:\\ISCC.exe"),
        patch("tools.package_desktop_app.subprocess.run", side_effect=fake_run),
    ):
        artifacts = package_windows_artifacts(
            dist_dir=tmp_path,
            version="2026.11.18.1",
            arch="arm64",
            skip_installer=False,
        )

    assert len(captured_cmds) == 1
    assert "/DAppArch=arm64" in captured_cmds[0]
    assert "/DAppVersion=2026.11.18.1" in captured_cmds[0]
    names = [a.name for a in artifacts]
    assert "EzSnmpDataLogger-Windows-arm64.exe" in names
    assert "EzSnmpDataLogger-Windows-arm64-Setup.exe" in names


def test_package_linux_artifacts_symlink_oserror_fallback(tmp_path: Path) -> None:
    """Verify Linux .deb packaging falls back cleanly when os.symlink raises OSError (WinError 1314)."""
    mock_bin = tmp_path / "EzSnmpDataLogger"
    mock_bin.write_text("#!/bin/sh\necho 'EzSnmpDataLogger'\n", encoding="utf-8")
    mock_bin.chmod(0o755)

    with patch("tools.package_desktop_app.os.symlink", side_effect=OSError(1314, "privilege")):
        artifacts = package_linux_artifacts(
            dist_dir=tmp_path,
            version="2026.11.18.1",
            arch="x64",
            skip_deb=False,
            skip_rpm=True,
        )

    names = [a.name for a in artifacts]
    assert "EzSnmpDataLogger-Linux-x64.deb" in names
    assert "ezsnmp-data-logger_2026.11.18.1_amd64.deb" in names


def test_setup_py_includes_vcpkg_windows_dll_search() -> None:
    """Verify setup.py and build_utils search vcpkg bin dirs for OpenSSL runtime DLLs on Windows."""
    setup_text = (REPO_ROOT / "setup.py").read_text(encoding="utf-8")
    assert "discover_windows_openssl_dlls" in setup_text
    build_utils_text = (REPO_ROOT / "tools" / "build_utils.py").read_text(encoding="utf-8")
    assert "C:/vcpkg/installed/{triplet}/bin" in build_utils_text
    assert "x64-windows" in build_utils_text
    assert "arm64-windows" in build_utils_text


def test_winget_distribution_workflow_configuration() -> None:
    """Verify that .github/workflows/build_and_publish_to_winget.yml conforms to [CICD-05]."""
    wf_path = REPO_ROOT / ".github" / "workflows" / "build_and_publish_to_winget.yml"
    assert wf_path.is_file(), "Missing build_and_publish_to_winget.yml"

    content = wf_path.read_text(encoding="utf-8")
    assert "release:" in content
    assert "types: [published]" in content
    assert "pull_request:" in content
    assert "workflow_dispatch:" in content
    assert "generate_winget_manifest.py" in content
    if "reusable_winget_publish.yml" in content:
        assert (
            "carlkidcrypto/shared-workflows/.github/workflows/reusable_winget_publish.yml"
            in content
        )
    else:
        assert "publish-winget:" in content
        assert "windows-latest" in content
        assert "actions/upload-artifact@v7" in content
        assert "wingetcreate.exe" in content
        assert "https://aka.ms/wingetcreate/latest" in content
        assert "wingetcreate.exe submit" in content
        assert "wingetcreate.exe update" in content
    assert "WINGET_CREATE_PAT" in content
    assert "WINGET_TOKEN" in content
    assert "WINGET_ACC_TOKEN" in content
