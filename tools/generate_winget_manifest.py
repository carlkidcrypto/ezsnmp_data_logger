#!/usr/bin/env python3
"""Generate WinGet (Windows Package Manager) community repository manifests.

Produces valid multi-file manifests (version, installer, defaultLocale) conforming to
WinGet manifest specification 1.6.0 for submission to microsoft/winget-pkgs.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import urllib.request
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

DEFAULT_PACKAGE_ID = "carlkidcrypto.EzSnmpDataLogger"
DEFAULT_PACKAGE_NAME = "EzSnmp Data Logger"
DEFAULT_PUBLISHER = "carlkidcrypto"
DEFAULT_PUBLISHER_URL = "https://github.com/carlkidcrypto"
MANIFEST_VERSION = "1.6.0"


def _read_current_calsemver() -> str:
    version_file = REPO_ROOT / "src" / "version.cpp"
    if version_file.is_file():
        text = version_file.read_text(encoding="utf-8")
        for line in text.splitlines():
            if "PROJECT_VERSION_STRING" in line and '"' in line:
                raw = line.split('"')[1].strip()
                # If it's a PEP-440 alpha (e.g. 2026.11.1a0), convert to SemVer-2.0
                if "a" in raw and "-" not in raw:
                    prefix, num = raw.split("a", 1)
                    return f"{prefix}-alpha.{num}"
                return raw
    return "2026.11.1-alpha.0"


def compute_sha256(path_or_url: str) -> str:
    """Compute the lowercase SHA256 hex digest of a local file or remote URL."""
    p = Path(path_or_url)
    h = hashlib.sha256()
    if p.is_file():
        with open(p, "rb") as f:
            for chunk in iter(lambda: f.read(65536), b""):
                h.update(chunk)
        return h.hexdigest().lower()

    if path_or_url.startswith("http://") or path_or_url.startswith("https://"):
        import ssl

        ctx = ssl.create_default_context()
        try:
            import certifi

            ctx.load_verify_locations(certifi.where())
        except Exception:
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE

        try:
            req = urllib.request.Request(
                path_or_url, headers={"User-Agent": "winget-manifest-gen/1.0"}
            )
            with urllib.request.urlopen(req, context=ctx) as resp:
                while chunk := resp.read(65536):
                    h.update(chunk)
            return h.hexdigest().lower()
        except Exception as exc:
            # If asset is not yet published remotely, return placeholder
            print(f"Warning: Could not fetch remote URL for SHA256 ({exc}); using zero digest.")
            return "0000000000000000000000000000000000000000000000000000000000000000"

    raise FileNotFoundError(f"Cannot compute hash for: {path_or_url}")


def generate_manifests(
    package_id: str,
    package_version: str,
    installer_url: str,
    installer_sha256: str,
    out_dir: Path,
    architecture: str = "x64",
    installer_type: str = "zip",
) -> dict[str, Path]:
    """Generate the 3 canonical WinGet manifest YAML files."""
    dest = out_dir / package_id.replace(".", "/") / package_version
    dest.mkdir(parents=True, exist_ok=True)

    version_yaml = (
        f"PackageIdentifier: {package_id}\n"
        f"PackageVersion: {package_version}\n"
        "DefaultLocale: en-US\n"
        "ManifestType: version\n"
        f"ManifestVersion: {MANIFEST_VERSION}\n"
    )

    locale_yaml = (
        f"PackageIdentifier: {package_id}\n"
        f"PackageVersion: {package_version}\n"
        "PackageLocale: en-US\n"
        f"Publisher: {DEFAULT_PUBLISHER}\n"
        f"PublisherUrl: {DEFAULT_PUBLISHER_URL}\n"
        "PublisherSupportUrl: https://github.com/carlkidcrypto/ezsnmp_data_logger/issues\n"
        f"Author: {DEFAULT_PUBLISHER}\n"
        f"PackageName: {DEFAULT_PACKAGE_NAME}\n"
        "PackageUrl: https://github.com/carlkidcrypto/ezsnmp_data_logger\n"
        "License: MIT\n"
        "LicenseUrl: https://raw.githubusercontent.com/carlkidcrypto/ezsnmp_data_logger/main/LICENSE\n"
        "Copyright: Copyright (c) 2024-2026 carlkidcrypto\n"
        "ShortDescription: Self-contained C++17 SNMP Data Logger with SWIG bindings, CLI, and GUI Studio.\n"
        "Description: |-\n"
        "  EzSnmp Data Logger provides high-performance SNMP telemetry ingestion,\n"
        "  local storage sinks (SQLite3, JSONL, CSV, Prometheus, PostgreSQL), and\n"
        "  unified CLI/GUI operation across Windows, Linux, and macOS.\n"
        "Tags:\n"
        "  - snmp\n"
        "  - telemetry\n"
        "  - data-logger\n"
        "  - networking\n"
        "  - monitoring\n"
        "ManifestType: defaultLocale\n"
        f"ManifestVersion: {MANIFEST_VERSION}\n"
    )

    installer_yaml = (
        f"PackageIdentifier: {package_id}\n"
        f"PackageVersion: {package_version}\n"
        "Installers:\n"
        f"  - Architecture: {architecture}\n"
        f"    InstallerType: {installer_type}\n"
        f"    InstallerUrl: {installer_url}\n"
        f"    InstallerSha256: {installer_sha256.upper()}\n"
        "    NestedInstallerType: portable\n"
        "    NestedInstallerFiles:\n"
        "      - RelativeFilePath: ezsnmp-data-logger.exe\n"
        "        PortableCommandAlias: ezsnmp-data-logger\n"
        "ManifestType: installer\n"
        f"ManifestVersion: {MANIFEST_VERSION}\n"
    )

    version_path = dest / f"{package_id}.yaml"
    locale_path = dest / f"{package_id}.locale.en-US.yaml"
    installer_path = dest / f"{package_id}.installer.yaml"

    version_path.write_text(version_yaml, encoding="utf-8")
    locale_path.write_text(locale_yaml, encoding="utf-8")
    installer_path.write_text(installer_yaml, encoding="utf-8")

    return {
        "version": version_path,
        "locale": locale_path,
        "installer": installer_path,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate WinGet manifests.")
    parser.add_argument("--package-id", default=DEFAULT_PACKAGE_ID)
    parser.add_argument("--version", default="")
    parser.add_argument(
        "--installer-url",
        default="https://github.com/carlkidcrypto/ezsnmp_data_logger/releases/download/v2026.11.1-alpha.0/ezsnmp_data_logger-windows-x64.zip",
    )
    parser.add_argument("--sha256", default="")
    parser.add_argument("--out-dir", default=str(REPO_ROOT / "dist-winget"))
    parser.add_argument("--arch", default="x64")
    parser.add_argument("--installer-type", default="zip")
    args = parser.parse_args(argv)

    ver = args.version or _read_current_calsemver()
    sha = args.sha256 or (
        "0000000000000000000000000000000000000000000000000000000000000000"
        if not args.installer_url.startswith("http")
        else compute_sha256(args.installer_url)
    )

    out = Path(args.out_dir)
    res = generate_manifests(
        package_id=args.package_id,
        package_version=ver,
        installer_url=args.installer_url,
        installer_sha256=sha,
        out_dir=out,
        architecture=args.arch,
        installer_type=args.installer_type,
    )

    print(f"Generated WinGet manifests for {args.package_id} v{ver}:")
    for k, p in res.items():
        print(f"  {k}: {p}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
