#!/usr/bin/env python3
"""Setuptools + SWIG 4.x C++17 build script for ezsnmp_data_logger."""

from __future__ import annotations

import os
import platform
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

from setuptools import Extension, find_packages, setup
from setuptools.command.build_ext import build_ext
from setuptools.command.build_py import build_py as _build_py

ROOT_DIR = Path(__file__).resolve().parent

if sys.platform == "darwin":
    os.environ.setdefault("ARCHFLAGS", f"-arch {platform.machine()}")
    mac_ver = platform.mac_ver()[0]
    if mac_ver:
        major = mac_ver.split(".")[0]
        if major.isdigit() and int(major) >= 11:
            os.environ.setdefault("MACOSX_DEPLOYMENT_TARGET", f"{major}.0")


def _run_cmd_output(cmd: list[str]) -> str:
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, check=True)
        return res.stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def _parse_flags(flag_str: str) -> tuple[list[str], list[str], list[str], list[str]]:
    inc_dirs: list[str] = []
    lib_dirs: list[str] = []
    libs: list[str] = []
    extra: list[str] = []
    for tok in shlex.split(flag_str):
        if tok.startswith("-I") and len(tok) > 2:
            inc_dirs.append(tok[2:])
        elif tok.startswith("-L") and len(tok) > 2:
            lib_dirs.append(tok[2:])
        elif tok.startswith("-l") and len(tok) > 2:
            libs.append(tok[2:])
        else:
            extra.append(tok)
    return inc_dirs, lib_dirs, libs, extra


def _locate_ezsnmp_source_dir() -> Path | None:
    env_dir = os.environ.get("EZSNMP_SOURCE_DIR", "").strip()
    candidates = []
    if env_dir:
        candidates.append(Path(env_dir))
    candidates.extend(
        [
            ROOT_DIR.parent / "ezsnmp",
            ROOT_DIR / "subprojects" / "ezsnmp",
        ]
    )
    for cand in candidates:
        if (cand / "ezsnmp" / "include" / "sessionbase.h").is_file():
            return cand.resolve()
    return None


def _stage_ezsnmp_sources(ezsnmp_dir: Path) -> tuple[list[str], list[str]]:
    """Stage ezsnmp C++17 sources under build/ezsnmp_vendor with unique relative paths."""
    stage_root = ROOT_DIR / "build" / "ezsnmp_vendor"
    stage_inc = stage_root / "ezsnmp" / "include"
    stage_src = stage_root / "ezsnmp" / "src"
    stage_patched = stage_src / "net-snmp-5.10-final-patched"

    stage_inc.mkdir(parents=True, exist_ok=True)
    stage_patched.mkdir(parents=True, exist_ok=True)

    for hdr in (ezsnmp_dir / "ezsnmp" / "include").glob("*.h"):
        shutil.copy2(hdr, stage_inc / hdr.name)

    core_map = {
        "datatypes.cpp": "ezsnmp_core_datatypes.cpp",
        "exceptionsbase.cpp": "ezsnmp_core_exceptionsbase.cpp",
        "helpers.cpp": "ezsnmp_core_helpers.cpp",
        "thread_safety.cpp": "ezsnmp_core_thread_safety.cpp",
        "sessionbase.cpp": "ezsnmp_core_sessionbase.cpp",
    }
    staged_sources: list[str] = []
    for orig_name, target_name in core_map.items():
        dst = stage_src / target_name
        shutil.copy2(ezsnmp_dir / "ezsnmp" / "src" / orig_name, dst)
        staged_sources.append(str(dst.relative_to(ROOT_DIR)))

    patched_files = [
        "snmpbulkget.cpp",
        "snmpbulkwalk.cpp",
        "snmpget.cpp",
        "snmpgetnext.cpp",
        "snmpset.cpp",
        "snmpwalk.cpp",
    ]
    for p_name in patched_files:
        dst = stage_patched / p_name
        shutil.copy2(
            ezsnmp_dir / "ezsnmp" / "src" / "net-snmp-5.10-final-patched" / p_name,
            dst,
        )
        staged_sources.append(str(dst.relative_to(ROOT_DIR)))

    for p_hdr in (ezsnmp_dir / "ezsnmp" / "src" / "net-snmp-5.10-final-patched").glob("*.h"):
        shutil.copy2(p_hdr, stage_patched / p_hdr.name)

    staged_incs = [
        str(stage_root.relative_to(ROOT_DIR)),
        str(stage_inc.relative_to(ROOT_DIR)),
    ]
    return staged_sources, staged_incs


def _configure_extension() -> Extension:
    sources = [
        "src/ezsnmp_data_logger_wrap.cxx",
        "src/version.cpp",
        "src/json_utils.cpp",
        "src/datatypes.cpp",
        "src/config.cpp",
        "src/snmp_poller.cpp",
        "src/csv_sink.cpp",
        "src/jsonl_sink.cpp",
        "src/sqlite_sink.cpp",
        "src/psql_sink.cpp",
        "src/prometheus_sink.cpp",
        "src/sink_inspector.cpp",
        "src/data_logger.cpp",
        "src/cli_runner.cpp",
    ]
    include_dirs = ["include"]
    library_dirs: list[str] = []
    libraries: list[str] = []
    define_macros: list[tuple[str, str | None]] = []

    if sys.platform == "win32":
        extra_compile_args = ["/std:c++17", "/EHsc"]
        extra_link_args: list[str] = []
        libraries.append("ws2_32")
    else:
        extra_compile_args = ["-std=c++17", "-O3"]
        extra_link_args = []

    # Homebrew prefix auto-discovery on macOS
    homebrew_prefixes = [Path("/opt/homebrew/opt"), Path("/usr/local/opt")]

    # 1. SQLite3 detection
    have_sqlite3 = 0
    sq_cflags = _run_cmd_output(["pkg-config", "--cflags", "sqlite3"])
    sq_libs = _run_cmd_output(["pkg-config", "--libs", "sqlite3"])
    if sq_libs:
        have_sqlite3 = 1
        inc, _, _, _ = _parse_flags(sq_cflags)
        _, ldir, lnames, _ = _parse_flags(sq_libs)
        include_dirs.extend(inc)
        library_dirs.extend(ldir)
        libraries.extend(lnames)
    else:
        for hp in homebrew_prefixes:
            if (hp / "sqlite" / "include" / "sqlite3.h").is_file():
                have_sqlite3 = 1
                include_dirs.append(str(hp / "sqlite" / "include"))
                library_dirs.append(str(hp / "sqlite" / "lib"))
                libraries.append("sqlite3")
                break
        if not have_sqlite3 and sys.platform != "win32":
            if Path("/usr/include/sqlite3.h").is_file() or sys.platform == "darwin":
                have_sqlite3 = 1
                libraries.append("sqlite3")

    # 2. ezsnmp + Net-SNMP detection (prefer Homebrew / pkg-config over macOS system SDK 5.6)
    have_ezsnmp = 0
    ezsnmp_dir = _locate_ezsnmp_source_dir()
    netsnmp_cflags = ""
    netsnmp_libs = ""

    if sys.platform == "darwin":
        for hp in homebrew_prefixes:
            ns_cfg = hp / "net-snmp" / "bin" / "net-snmp-config"
            if ns_cfg.is_file():
                netsnmp_cflags = _run_cmd_output([str(ns_cfg), "--cflags"])
                netsnmp_libs = _run_cmd_output([str(ns_cfg), "--libs"])
                break

    if not netsnmp_libs:
        netsnmp_cflags = _run_cmd_output(["pkg-config", "--cflags", "netsnmp"])
        netsnmp_libs = _run_cmd_output(["pkg-config", "--libs", "netsnmp"])

    if not netsnmp_libs and sys.platform != "darwin":
        netsnmp_cflags = _run_cmd_output(["net-snmp-config", "--cflags"])
        netsnmp_libs = _run_cmd_output(["net-snmp-config", "--libs"])

    if ezsnmp_dir is not None and netsnmp_libs:
        have_ezsnmp = 1
        staged_srcs, staged_incs = _stage_ezsnmp_sources(ezsnmp_dir)
        sources.extend(staged_srcs)
        include_dirs.extend(staged_incs)

        inc, _, _, _ = _parse_flags(netsnmp_cflags)
        _, ldir, lnames, _ = _parse_flags(netsnmp_libs)
        include_dirs.extend(inc)
        library_dirs.extend(ldir)
        libraries.extend(lnames)

        if sys.platform == "darwin":
            for hp in homebrew_prefixes:
                if (hp / "openssl@3" / "include").is_dir():
                    include_dirs.append(str(hp / "openssl@3" / "include"))
                    library_dirs.append(str(hp / "openssl@3" / "lib"))
                    break

    define_macros.extend(
        [
            ("EZSNMP_DL_HAVE_EZSNMP", str(have_ezsnmp)),
            ("EZSNMP_DL_HAVE_SQLITE3", str(have_sqlite3)),
            ("EZSNMP_DL_HAVE_LIBPQ", "0"),
        ]
    )

    # Deduplicate while preserving order
    include_dirs = list(dict.fromkeys(include_dirs))
    library_dirs = list(dict.fromkeys(library_dirs))
    libraries = list(dict.fromkeys(libraries))

    return Extension(
        name="ezsnmp_data_logger._ezsnmp_data_logger",
        sources=sources,
        include_dirs=include_dirs,
        library_dirs=library_dirs,
        libraries=libraries,
        define_macros=define_macros,
        extra_compile_args=extra_compile_args,
        extra_link_args=extra_link_args,
        language="c++",
    )


class SwigBuildExt(build_ext):
    """Run SWIG 4.x before compiling the C++17 Python extension."""

    def run(self) -> None:
        swig_bin = shutil.which("swig")
        if not swig_bin:
            for cand in ("/opt/homebrew/bin/swig", "/usr/local/bin/swig"):
                if os.path.isfile(cand):
                    swig_bin = cand
                    break
        if not swig_bin:
            raise RuntimeError("SWIG 4.x executable ('swig') was not found on PATH.")

        wrap_out = ROOT_DIR / "src" / "ezsnmp_data_logger_wrap.cxx"
        py_outdir = ROOT_DIR / "ezsnmp_data_logger"
        iface = ROOT_DIR / "interface" / "ezsnmp_data_logger.i"

        cmd = [
            swig_bin,
            "-c++",
            "-std=c++17",
            "-python",
            f"-I{ROOT_DIR / 'include'}",
            f"-I{ROOT_DIR / 'interface'}",
            "-outdir",
            str(py_outdir),
            "-o",
            str(wrap_out),
            str(iface),
        ]
        subprocess.run(cmd, check=True)
        super().run()


class BuildPy(_build_py):
    """Ensure SWIG-generated ezsnmp_data_logger.py exists prior to packaging Python modules."""

    def run(self) -> None:
        self.run_command("build_ext")
        super().run()


setup(
    packages=find_packages(include=["ezsnmp_data_logger", "ezsnmp_data_logger.*"]),
    package_data={
        "ezsnmp_data_logger.gui": ["static/*"],
    },
    include_package_data=True,
    ext_modules=[_configure_extension()],
    cmdclass={
        "build_ext": SwigBuildExt,
        "build_py": BuildPy,
    },
)
