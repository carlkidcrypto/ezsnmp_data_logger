#!/usr/bin/env python3
"""Provision the pinned CalSemVer alpha ezsnmp C++17 SessionBase source tree."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
EZSNMP_REPO_URL = "https://github.com/carlkidcrypto/ezsnmp.git"
EZSNMP_CALSEMVER_TAG = "v2026.11.9b0"


def ensure_ezsnmp_source(target_dir: Path | None = None, tag: str = EZSNMP_CALSEMVER_TAG) -> Path:
    """Locate third_party/ezsnmp or ../ezsnmp, or clone pinned tag into third_party/ezsnmp."""
    env_dir = os.environ.get("EZSNMP_SOURCE_DIR", "").strip()
    if env_dir:
        cand = Path(env_dir).resolve()
        if (cand / "ezsnmp" / "include" / "sessionbase.h").is_file():
            return cand

    vendored = (REPO_ROOT / "third_party" / "ezsnmp").resolve()
    if target_dir is None and (vendored / "ezsnmp" / "include" / "sessionbase.h").is_file():
        return vendored

    sibling = (REPO_ROOT.parent / "ezsnmp").resolve()
    if target_dir is None and (sibling / "ezsnmp" / "include" / "sessionbase.h").is_file():
        return sibling

    dest = (target_dir or vendored).resolve()
    if (dest / "ezsnmp" / "include" / "sessionbase.h").is_file():
        return dest

    git_bin = shutil.which("git")
    if not git_bin:
        raise RuntimeError("git is required to fetch carlkidcrypto/ezsnmp sources")

    dest.parent.mkdir(parents=True, exist_ok=True)
    max_retries = 3
    for attempt in range(1, max_retries + 1):
        try:
            if dest.exists():
                shutil.rmtree(dest, ignore_errors=True)
            subprocess.run(
                [git_bin, "clone", "--depth", "1", "--branch", tag, EZSNMP_REPO_URL, str(dest)],
                check=True,
            )
            return dest
        except subprocess.CalledProcessError:
            if attempt == max_retries:
                raise
            time.sleep(2 * attempt)
    return dest


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Ensure pinned CalSemVer alpha ezsnmp C++17 sources are available."
    )
    parser.add_argument("--tag", default=EZSNMP_CALSEMVER_TAG)
    parser.add_argument("--target-dir", default="")
    args = parser.parse_args(argv)

    target = Path(args.target_dir).resolve() if args.target_dir else None
    resolved = ensure_ezsnmp_source(target_dir=target, tag=args.tag)
    print(f"ezsnmp C++17 source ready ({args.tag}): {resolved}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
