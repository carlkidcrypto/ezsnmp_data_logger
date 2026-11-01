#!/usr/bin/env python3
"""Generate SWIG 4.x C++17 bindings for ezsnmp_data_logger across all supported languages."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
INTERFACE_FILE = REPO_ROOT / "interface" / "ezsnmp_data_logger.i"
INCLUDE_DIR = REPO_ROOT / "include"

# Complete matrix of SWIG 4.x target languages supported by ezsnmp_data_logger
SWIG_LANGUAGE_TARGETS: dict[str, list[str]] = {
    "python": ["-python"],
    "javascript": ["-javascript", "-napi"],
    "java": ["-java", "-package", "io.github.carlkidcrypto.ezsnmp_data_logger"],
    "csharp": ["-csharp", "-namespace", "EzSnmpDataLogger"],
    "go": ["-go", "-cgo", "-intgosize", "64"],
    "ruby": ["-ruby"],
    "perl5": ["-perl5"],
    "php": ["-php"],
    "lua": ["-lua"],
    "r": ["-r"],
    "tcl8": ["-tcl8"],
    "octave": ["-octave"],
    "d": ["-d", "-d2"],
    "guile": ["-guile"],
    "scilab": ["-scilab"],
    "xml": ["-xml"],
}


def generate_for_language(
    swig_bin: str,
    lang: str,
    out_root: Path,
) -> dict[str, object]:
    """Invoke SWIG for a single target language and return structured metadata."""
    if lang not in SWIG_LANGUAGE_TARGETS:
        raise ValueError(f"Unsupported SWIG target language: {lang}")

    lang_dir = out_root / lang
    if lang_dir.exists():
        shutil.rmtree(lang_dir)
    lang_dir.mkdir(parents=True, exist_ok=True)

    wrap_ext = ".xml" if lang == "xml" else ".cxx"
    wrap_file = lang_dir / f"ezsnmp_data_logger_wrap{wrap_ext}"

    cmd = [
        swig_bin,
        "-c++",
        "-std=c++17",
        *SWIG_LANGUAGE_TARGETS[lang],
        f"-I{INCLUDE_DIR}",
        f"-I{INTERFACE_FILE.parent}",
        "-outdir",
        str(lang_dir),
        "-o",
        str(wrap_file),
        str(INTERFACE_FILE),
    ]

    proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
    generated_files = sorted(
        str(p.relative_to(lang_dir)) for p in lang_dir.rglob("*") if p.is_file()
    )
    return {
        "language": lang,
        "ok": proc.returncode == 0 and wrap_file.exists(),
        "returncode": proc.returncode,
        "command": cmd,
        "output_dir": str(lang_dir),
        "generated_files": generated_files,
        "stdout": proc.stdout,
        "stderr": proc.stderr,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate C++17 SWIG bindings for ezsnmp_data_logger across target languages."
    )
    parser.add_argument(
        "--languages",
        nargs="+",
        default=["all"],
        help="Target languages to generate, or 'all' (default: all).",
    )
    parser.add_argument(
        "--output-dir",
        default=str(REPO_ROOT / "build" / "swig_generated"),
        help="Directory where per-language SWIG outputs are written.",
    )
    parser.add_argument(
        "--swig",
        default=shutil.which("swig") or "swig",
        help="Path to swig executable.",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Emit structured JSON summary.",
    )
    args = parser.parse_args(argv)

    selected = (
        list(SWIG_LANGUAGE_TARGETS.keys())
        if "all" in args.languages
        else [lang.strip() for lang in args.languages]
    )

    out_root = Path(args.output_dir).resolve()
    results = []
    failures = 0

    for lang in selected:
        res = generate_for_language(args.swig, lang, out_root)
        results.append(res)
        if not res["ok"]:
            failures += 1

    if args.json:
        print(
            json.dumps(
                {
                    "ok": failures == 0,
                    "total": len(results),
                    "succeeded": len(results) - failures,
                    "failed": failures,
                    "results": results,
                },
                indent=2,
            )
        )
    else:
        for res in results:
            status = "OK" if res["ok"] else "FAIL"
            print(
                f"[{status}] {res['language']:12s} -> {len(res['generated_files'])} file(s) in {res['output_dir']}"
            )
            if not res["ok"] and res["stderr"]:
                print(f"  stderr: {res['stderr'].strip()}", file=sys.stderr)

    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
