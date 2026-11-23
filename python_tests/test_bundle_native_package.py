"""Unit tests for tools/bundle_native_package.py Windows DLL bundling and verification."""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT))

from tools import bundle_native_package as bnp


def _write_fake_pe(path: Path, imports: list[str]) -> None:
    """Create a synthetic binary containing ASCII DLL import names for _pe_extract_dll_imports."""
    payload = b"MZ\x90\x00" + b"\x00".join(name.encode("ascii") for name in imports) + b"\x00"
    path.write_bytes(payload)


def test_bundle_windows_transitive_bfs(tmp_path: Path) -> None:
    """_bundle_windows() must transitively resolve netsnmp.dll -> libcrypto/libssl via BFS."""
    art_dir = tmp_path / "pkg"
    art_dir.mkdir()
    artifact = art_dir / "_ezsnmp_data_logger.pyd"
    _write_fake_pe(artifact, ["KERNEL32.dll", "netsnmp.dll", "vcruntime140.dll"])

    netsnmp_dir = tmp_path / "netsnmp_bin"
    netsnmp_dir.mkdir()
    _write_fake_pe(
        netsnmp_dir / "netsnmp.dll",
        ["WS2_32.dll", "libcrypto-3-x64.dll", "libssl-3-x64.dll"],
    )

    openssl_dir = tmp_path / "openssl_bin"
    openssl_dir.mkdir()
    _write_fake_pe(openssl_dir / "libcrypto-3-x64.dll", ["KERNEL32.dll", "ucrtbase.dll"])
    _write_fake_pe(
        openssl_dir / "libssl-3-x64.dll",
        ["KERNEL32.dll", "libcrypto-3-x64.dll"],
    )

    libs_dir = tmp_path / "out_libs"
    bundled = bnp._bundle_windows(artifact, libs_dir, [netsnmp_dir, openssl_dir])

    assert bundled == ["libcrypto-3-x64.dll", "libssl-3-x64.dll", "netsnmp.dll"]
    assert (libs_dir / "netsnmp.dll").is_file()
    assert (libs_dir / "libcrypto-3-x64.dll").is_file()
    assert (libs_dir / "libssl-3-x64.dll").is_file()


def test_verify_self_contained_windows_flags_missing_dlls(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """verify_self_contained() on Windows must return ok=False when non-OS DLLs are missing."""
    monkeypatch.setattr(bnp.sys, "platform", "win32")

    art_dir = tmp_path / "pkg"
    art_dir.mkdir()
    artifact = art_dir / "_ezsnmp_data_logger.pyd"
    _write_fake_pe(artifact, ["KERNEL32.dll", "netsnmp.dll"])

    libs_dir = tmp_path / "libs"
    libs_dir.mkdir()
    # Provide netsnmp.dll, but omit its transitive OpenSSL dependency libcrypto-3-x64.dll
    _write_fake_pe(libs_dir / "netsnmp.dll", ["WS2_32.dll", "libcrypto-3-x64.dll"])

    report = bnp.verify_self_contained(artifact, libs_dir)
    assert report["ok"] is False
    assert {"binary": "netsnmp.dll", "dependency": "libcrypto-3-x64.dll"} in report[
        "external_dependencies"
    ]

    # Once libcrypto-3-x64.dll is present, verification succeeds
    _write_fake_pe(libs_dir / "libcrypto-3-x64.dll", ["KERNEL32.dll"])
    report_ok = bnp.verify_self_contained(artifact, libs_dir)
    assert report_ok["ok"] is True
    assert report_ok["external_dependencies"] == []


def test_bundle_artifact_windows_uses_env_and_vcpkg_search_dirs(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """bundle_artifact() on Windows discovers DLLs via EZSNMP_NETSNMP_BIN_DIR and VCPKG_ROOT."""
    monkeypatch.setattr(bnp.sys, "platform", "win32")

    art_dir = tmp_path / "pkg"
    art_dir.mkdir()
    artifact = art_dir / "_ezsnmp_data_logger.pyd"
    _write_fake_pe(artifact, ["KERNEL32.dll", "netsnmp.dll"])

    netsnmp_dir = tmp_path / "netsnmp_bin"
    netsnmp_dir.mkdir()
    _write_fake_pe(netsnmp_dir / "netsnmp.dll", ["libcrypto-3-x64.dll"])

    vcpkg_root = tmp_path / "vcpkg"
    vcpkg_bin = vcpkg_root / "installed" / "x64-windows" / "bin"
    vcpkg_bin.mkdir(parents=True)
    _write_fake_pe(vcpkg_bin / "libcrypto-3-x64.dll", ["KERNEL32.dll"])

    monkeypatch.setenv("EZSNMP_NETSNMP_BIN_DIR", str(netsnmp_dir))
    monkeypatch.delenv("EZSNMP_OPENSSL_BIN_DIR", raising=False)
    monkeypatch.setenv("VCPKG_ROOT", str(vcpkg_root))

    report = bnp.bundle_artifact(artifact)
    assert report["ok"] is True
    assert report["external_dependencies"] == []
    assert report["bundled_libraries"] == ["libcrypto-3-x64.dll", "netsnmp.dll"]
    assert (art_dir / "netsnmp.dll").is_file()
    assert (art_dir / "libcrypto-3-x64.dll").is_file()
