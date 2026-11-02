#!/usr/bin/env python3
"""Download and verify the public-domain SQLite3 C amalgamation for self-contained builds."""

from __future__ import annotations

import argparse
import hashlib
import shutil
import subprocess
import tempfile
import urllib.request
import zipfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUT_DIR = REPO_ROOT / "third_party" / "sqlite3"

SQLITE_VERSION = "3.49.1"
SQLITE_URL = "https://www.sqlite.org/2025/sqlite-amalgamation-3490100.zip"
SQLITE_SHA256 = "6cebd1d8403fc58c30e93939b246f3e6e58d0765a5cd50546f16c00fd805d2c3"
SQLITE_SUBDIR = "sqlite-amalgamation-3490100"


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _download_archive(url: str, dest: Path) -> None:
    curl_bin = shutil.which("curl")
    if curl_bin:
        res = subprocess.run(
            [curl_bin, "--proto", "=https", "-fsSL", "-o", str(dest), url],
            capture_output=True,
            text=True,
            check=False,
        )
        if res.returncode == 0 and dest.is_file():
            return
    with urllib.request.urlopen(url, timeout=60) as resp:
        dest.write_bytes(resp.read())


def ensure_sqlite3_amalgamation(out_dir: Path = DEFAULT_OUT_DIR, force: bool = False) -> Path:
    """Ensure verified sqlite3.c and sqlite3.h exist in out_dir and return out_dir."""
    c_file = out_dir / "sqlite3.c"
    h_file = out_dir / "sqlite3.h"
    stamp = out_dir / ".sqlite_version"

    if (
        not force
        and c_file.is_file()
        and h_file.is_file()
        and stamp.is_file()
        and stamp.read_text(encoding="utf-8").strip() == SQLITE_VERSION
    ):
        return out_dir

    out_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ezsnmp-dl-sqlite-") as tmp:
        archive_path = Path(tmp) / "sqlite-amalgamation.zip"
        _download_archive(SQLITE_URL, archive_path)
        actual_sha = _sha256_file(archive_path)
        if actual_sha.lower() != SQLITE_SHA256.lower():
            raise RuntimeError(
                f"SQLite amalgamation SHA-256 mismatch: expected {SQLITE_SHA256}, got {actual_sha}"
            )
        with zipfile.ZipFile(archive_path) as zf:
            for filename in ("sqlite3.c", "sqlite3.h", "sqlite3ext.h"):
                member = f"{SQLITE_SUBDIR}/{filename}"
                target = out_dir / filename
                target.write_bytes(zf.read(member))

    stamp.write_text(f"{SQLITE_VERSION}\n", encoding="utf-8")
    return out_dir


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Provision SHA-256-verified SQLite3 C amalgamation for self-contained builds."
    )
    parser.add_argument("--output-dir", default=str(DEFAULT_OUT_DIR))
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args(argv)

    staged = ensure_sqlite3_amalgamation(Path(args.output_dir).resolve(), force=args.force)
    print(f"SQLite {SQLITE_VERSION} amalgamation ready at: {staged}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
