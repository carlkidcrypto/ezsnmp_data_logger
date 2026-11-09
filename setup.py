#!/usr/bin/env python3
"""Setuptools + SWIG 4.x C++17 self-contained build script for ezsnmp_data_logger."""

from __future__ import annotations

import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

from setuptools import Extension, find_packages, setup
from setuptools.command.build_ext import build_ext
from setuptools.command.build_py import build_py as _build_py

ROOT_DIR = Path(__file__).resolve().parent
if str(ROOT_DIR / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT_DIR / "tools"))

from build_utils import (
    collect_files_with_suffixes,
    compute_object_cache_key,
    is_python_independent_source,
    is_up_to_date,
    resolve_build_jobs,
)

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


def _parse_netsnmp_version(version_str: str) -> tuple[int, ...] | None:
    """Parse major and minor version numbers from a Net-SNMP version string."""
    m = re.search(r"(\d+)\.(\d+)(?:\.(\d+))?", version_str.strip())
    if not m:
        return None
    parts = [int(p) for p in m.groups() if p is not None]
    return tuple(parts)


def _is_netsnmp_5_10_or_newer(version_str: str) -> bool:
    """Check if Net-SNMP version is at least 5.10 (as required by ezsnmp)."""
    parsed = _parse_netsnmp_version(version_str)
    if parsed is None or len(parsed) < 2:
        return False
    return parsed[:2] >= (5, 10)


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
    if os.environ.get("EZSNMP_DL_DISABLE_EZSNMP_LINK", "").strip() == "1":
        return None
    env_dir = os.environ.get("EZSNMP_SOURCE_DIR", "").strip()
    candidates = []
    if env_dir:
        candidates.append(Path(env_dir))
    candidates.extend(
        [
            ROOT_DIR / "third_party" / "ezsnmp",
            ROOT_DIR.parent / "ezsnmp",
        ]
    )
    for cand in candidates:
        if (cand / "ezsnmp" / "include" / "sessionbase.h").is_file():
            return cand.resolve()

    if os.environ.get("EZSNMP_DL_AUTO_FETCH_DEPS", "1").strip() != "0":
        try:
            import prepare_ezsnmp_source

            return prepare_ezsnmp_source.ensure_ezsnmp_source()
        except Exception:
            return None
    return None


def _ensure_sqlite_amalgamation() -> Path | None:
    """Provision embedded SQLite3 C amalgamation so zero runtime libsqlite3 is needed."""
    if os.environ.get("EZSNMP_DL_USE_SYSTEM_SQLITE", "").strip() == "1":
        return None
    amalg_dir = ROOT_DIR / "third_party" / "sqlite3"
    if (amalg_dir / "sqlite3.c").is_file() and (amalg_dir / "sqlite3.h").is_file():
        return amalg_dir
    if os.environ.get("EZSNMP_DL_AUTO_FETCH_DEPS", "1").strip() != "0":
        try:
            import prepare_sqlite3_amalgamation

            return prepare_sqlite3_amalgamation.ensure_sqlite3_amalgamation(amalg_dir)
        except Exception:
            return None
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

    homebrew_prefixes = [Path("/opt/homebrew/opt"), Path("/usr/local/opt")]

    # 1. Self-contained SQLite3 amalgamation (preferred) with system fallback
    have_sqlite3 = 0
    embedded_sqlite3 = 0
    amalg_dir = _ensure_sqlite_amalgamation()
    if amalg_dir is not None:
        have_sqlite3 = 1
        embedded_sqlite3 = 1
        include_dirs.insert(0, str(amalg_dir.relative_to(ROOT_DIR)))
    else:
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

    # 2. ezsnmp + Net-SNMP detection (supports standalone cibuildwheel env vars + Net-SNMP 5.10 auto-discovery)
    have_ezsnmp = 0
    ezsnmp_dir = _locate_ezsnmp_source_dir()
    netsnmp_cflags = ""
    netsnmp_libs = ""

    win_inc = os.environ.get("EZSNMP_NETSNMP_INCLUDE_DIR", "").strip()
    win_lib = os.environ.get("EZSNMP_NETSNMP_LIB_DIR", "").strip()
    explicit_cfg = os.environ.get("EZSNMP_NETSNMP_CONFIG", "").strip()

    if sys.platform == "win32" and (not win_inc or not win_lib):
        # Auto-discover standalone Net-SNMP 5.10 distribution (e.g. prepared by prepare-windows-netsnmp)
        for cand_arch in ("x64", "AMD64", "x86", "ARM64"):
            cand_root = Path(f"C:/ezsnmp-netsnmp-dist/{cand_arch}")
            cand_h = cand_root / "include" / "net-snmp" / "net-snmp-config.h"
            cand_l = cand_root / "lib" / "release" / "netsnmp.lib"
            if cand_h.is_file() and cand_l.is_file():
                hdr_text = cand_h.read_text(encoding="utf-8", errors="ignore")
                vmatch = re.search(r'#define\s+PACKAGE_VERSION\s+"([^"]+)"', hdr_text)
                if vmatch and _is_netsnmp_5_10_or_newer(vmatch.group(1)):
                    win_inc = str(cand_root / "include")
                    win_lib = str(cand_root / "lib" / "release")
                    break

    if sys.platform == "win32" and win_inc and win_lib:
        netsnmp_libs = "netsnmp"
    elif explicit_cfg and Path(explicit_cfg).is_file():
        cfg_ver = _run_cmd_output([explicit_cfg, "--version"])
        if _is_netsnmp_5_10_or_newer(cfg_ver):
            netsnmp_cflags = _run_cmd_output([explicit_cfg, "--cflags"])
            netsnmp_libs = _run_cmd_output([explicit_cfg, "--libs"])
        else:
            print(
                f"[WARNING] Configured Net-SNMP ({explicit_cfg}) version '{cfg_ver}' is < 5.10; "
                "ezsnmp requires Net-SNMP >= 5.10. Skipping."
            )
    else:
        # Check standard Net-SNMP 5.10 locations first (/usr/local/bin/net-snmp-config)
        cfg_candidates = []
        if Path("/usr/local/bin/net-snmp-config").is_file():
            cfg_candidates.append("/usr/local/bin/net-snmp-config")
        if sys.platform == "darwin":
            for hp in homebrew_prefixes:
                ns_cfg = hp / "net-snmp" / "bin" / "net-snmp-config"
                if ns_cfg.is_file():
                    cfg_candidates.append(str(ns_cfg))

        for cfg_cand in cfg_candidates:
            cand_ver = _run_cmd_output([cfg_cand, "--version"])
            if _is_netsnmp_5_10_or_newer(cand_ver):
                netsnmp_cflags = _run_cmd_output([cfg_cand, "--cflags"])
                netsnmp_libs = _run_cmd_output([cfg_cand, "--libs"])
                break
            else:
                print(
                    f"[INFO] Net-SNMP at {cfg_cand} version '{cand_ver}' is < 5.10; "
                    "skipping in favor of Net-SNMP >= 5.10."
                )

        if not netsnmp_libs:
            pkg_ver = _run_cmd_output(["pkg-config", "--modversion", "netsnmp"])
            if pkg_ver and _is_netsnmp_5_10_or_newer(pkg_ver):
                netsnmp_cflags = _run_cmd_output(["pkg-config", "--cflags", "netsnmp"])
                netsnmp_libs = _run_cmd_output(["pkg-config", "--libs", "netsnmp"])

        if not netsnmp_libs and sys.platform not in ("darwin", "win32"):
            sys_ver = _run_cmd_output(["net-snmp-config", "--version"])
            if sys_ver and _is_netsnmp_5_10_or_newer(sys_ver):
                netsnmp_cflags = _run_cmd_output(["net-snmp-config", "--cflags"])
                netsnmp_libs = _run_cmd_output(["net-snmp-config", "--libs"])

    if ezsnmp_dir is not None and netsnmp_libs:
        have_ezsnmp = 1
        staged_srcs, staged_incs = _stage_ezsnmp_sources(ezsnmp_dir)
        sources.extend(staged_srcs)
        include_dirs.extend(staged_incs)

        if sys.platform == "win32" and win_inc and win_lib:
            include_dirs.extend([p for p in win_inc.split(os.pathsep) if p])
            library_dirs.extend([p for p in win_lib.split(os.pathsep) if p])
            libraries.extend(["netsnmp", "advapi32", "iphlpapi", "crypt32", "user32"])
            ssl_inc = os.environ.get("EZSNMP_OPENSSL_INCLUDE_DIR", "").strip()
            ssl_lib = os.environ.get("EZSNMP_OPENSSL_LIB_DIR", "").strip()
            if not ssl_inc or not ssl_lib:
                for vcpkg_cand in (
                    Path("C:/vcpkg/installed/x64-windows"),
                    Path("C:/vcpkg/installed/arm64-windows"),
                ):
                    if (vcpkg_cand / "include" / "openssl" / "ssl.h").is_file():
                        ssl_inc = ssl_inc or str(vcpkg_cand / "include")
                        ssl_lib = ssl_lib or str(vcpkg_cand / "lib")
                        break
            if ssl_inc:
                include_dirs.extend([p for p in ssl_inc.split(os.pathsep) if p])
            if ssl_lib:
                library_dirs.extend([p for p in ssl_lib.split(os.pathsep) if p])
        else:
            inc, _, _, _ = _parse_flags(netsnmp_cflags)
            _, ldir, lnames, _ = _parse_flags(netsnmp_libs)
            include_dirs.extend(inc)
            library_dirs.extend(ldir)
            libraries.extend(lnames)

        ssl_inc_env = os.environ.get("EZSNMP_OPENSSL_INCLUDE_DIR", "").strip()
        ssl_lib_env = os.environ.get("EZSNMP_OPENSSL_LIB_DIR", "").strip()
        if ssl_inc_env:
            include_dirs.extend(
                [p for p in ssl_inc_env.split(os.pathsep) if p and os.path.isdir(p)]
            )
        if ssl_lib_env:
            library_dirs.extend(
                [p for p in ssl_lib_env.split(os.pathsep) if p and os.path.isdir(p)]
            )

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
            ("EZSNMP_DL_EMBEDDED_SQLITE3", str(embedded_sqlite3)),
            ("EZSNMP_DL_HAVE_LIBPQ", "0"),
        ]
    )

    include_dirs = list(dict.fromkeys(include_dirs))
    library_dirs = list(dict.fromkeys(library_dirs))
    libraries = list(dict.fromkeys(libraries))

    ext = Extension(
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
    ext._embedded_sqlite_c = (  # type: ignore[attr-defined]
        str((amalg_dir / "sqlite3.c").relative_to(ROOT_DIR)) if amalg_dir else None
    )
    return ext


class SwigBuildExt(build_ext):
    """Run SWIG 4.x, compile embedded SQLite3 C amalgamation, and build C++17 extension in parallel."""

    def _get_compiler_identity(self) -> dict:
        compiler_type = getattr(self.compiler, "compiler_type", "")
        if compiler_type == "msvc":
            cc_bin = getattr(self.compiler, "cc", None)
            opts_attr = "compile_options_debug" if self.debug else "compile_options"
            opts = list(getattr(self.compiler, opts_attr, []) or [])
            cmd = ([cc_bin] if cc_bin else []) + opts
            resolved_bin = (
                cc_bin
                if (cc_bin and os.path.isfile(cc_bin))
                else (shutil.which(cc_bin) if cc_bin else None)
            )
        else:
            raw_cmd = (
                getattr(self.compiler, "compiler_so_cxx", None)
                or getattr(self.compiler, "compiler_cxx", None)
                or getattr(self.compiler, "compiler_so", None)
                or []
            )
            cmd = [str(part) for part in raw_cmd]
            resolved_bin = shutil.which(cmd[0]) if cmd else None

        bin_meta = None
        if resolved_bin and os.path.isfile(resolved_bin):
            try:
                stat = os.stat(resolved_bin)
                bin_meta = [resolved_bin, stat.st_size, int(stat.st_mtime)]
            except OSError:
                bin_meta = [resolved_bin]

        return {
            "compiler_type": compiler_type,
            "cmd": [str(part) for part in cmd],
            "bin_meta": bin_meta,
        }

    def _compile_unit(self, unit: dict) -> None:
        start_time = time.perf_counter()
        self.compiler.compile(
            [unit["src"]],
            output_dir=unit["output_dir"],
            macros=list(unit["macros"]),
            include_dirs=list(unit["include_dirs"]),
            debug=self.debug,
            extra_postargs=list(unit["extra_args"]),
            depends=list(unit.get("depends") or []),
        )
        delta = time.perf_counter() - start_time
        print(f"\t[PROFILE] Compiled {unit['src']} in {delta:.3f}s")

        cache_path = unit.get("cache_path")
        if cache_path and os.path.isfile(unit["obj"]):
            try:
                os.makedirs(os.path.dirname(cache_path), exist_ok=True)
                tmp_cache = f"{cache_path}.{os.getpid()}.{threading.get_ident()}.tmp"
                shutil.copy2(unit["obj"], tmp_cache)
                os.replace(tmp_cache, cache_path)
            except OSError:
                pass

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
        py_wrapper = py_outdir / "ezsnmp_data_logger.py"
        iface = ROOT_DIR / "interface" / "ezsnmp_data_logger.i"

        swig_inputs = [str(iface)] + collect_files_with_suffixes(
            [ROOT_DIR / "include"], (".h", ".hpp")
        )
        swig_outputs = [str(wrap_out), str(py_wrapper)]

        if (
            not self.force
            and wrap_out.is_file()
            and py_wrapper.is_file()
            and is_up_to_date(swig_outputs, swig_inputs)
        ):
            print("\t[PROFILE] SWIG generated wrappers are up-to-date, skipping SWIG step.")
        else:
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

        should_bundle = (
            os.environ.get("EZSNMP_DL_BUNDLE_INPLACE", "1").strip() != "0"
            and os.environ.get("CIBUILDWHEEL", "").strip() != "1"
        )
        if should_bundle:
            import bundle_native_package

            seen: set[Path] = set()
            for ext in self.extensions:
                candidates = [Path(self.get_ext_fullpath(ext.name)).resolve()]
                if self.inplace:
                    inplace_name = Path(self.get_ext_filename(ext.name)).name
                    candidates.append((ROOT_DIR / "ezsnmp_data_logger" / inplace_name).resolve())
                for cand in candidates:
                    if cand.is_file() and cand not in seen:
                        seen.add(cand)
                        bundle_native_package.bundle_artifact(cand)

    def build_extensions(self) -> None:
        if not self.extensions:
            return

        is_msvc = getattr(self.compiler, "compiler_type", "") == "msvc"
        build_temp = self.build_temp or "build/temp"
        os.makedirs(build_temp, exist_ok=True)

        if hasattr(self.compiler, "initialize") and not getattr(self.compiler, "initialized", True):
            self.compiler.initialize()

        # Strip -Wstrict-prototypes if present on POSIX compilers when building C++
        compiler_so = getattr(self.compiler, "compiler_so", None)
        if isinstance(compiler_so, list) and "-Wstrict-prototypes" in compiler_so:
            compiler_so.remove("-Wstrict-prototypes")

        jobs = resolve_build_jobs(self.parallel)
        compiler_identity = self._get_compiler_identity()
        compiler_macros = list(getattr(self.compiler, "macros", []) or [])
        header_files = collect_files_with_suffixes(
            [
                ROOT_DIR / "include",
                ROOT_DIR / "build" / "ezsnmp_vendor",
                ROOT_DIR / "third_party" / "sqlite3",
            ],
            (".h", ".hpp"),
        )
        object_cache_dir = os.environ.get("EZSNMP_OBJECT_CACHE_DIR", "").strip()

        units: list[dict] = []
        ext_objects: dict[str, list[str]] = {}

        for ext in self.extensions:
            ext_objs: list[str] = []
            macros = list(ext.define_macros or [])
            for undef in ext.undef_macros or []:
                macros.append((undef,))

            extra_args = list(ext.extra_compile_args or [])
            if is_msvc and (
                self.debug or any(f in extra_args for f in ("/Zi", "/ZI", "-Zi", "-ZI"))
            ):
                if "/FS" not in extra_args and "-FS" not in extra_args:
                    extra_args.append("/FS")

            # 1. Embedded SQLite amalgamation
            sqlite_c = getattr(ext, "_embedded_sqlite_c", None)
            if sqlite_c and os.path.isfile(sqlite_c):
                c_macros = [
                    ("SQLITE_THREADSAFE", "1"),
                    ("SQLITE_ENABLE_FTS5", "1"),
                    ("SQLITE_ENABLE_JSON1", "1"),
                    ("SQLITE_OMIT_LOAD_EXTENSION", "1"),
                ]
                c_postargs = ["/O2"] if is_msvc else ["-O2", "-fPIC", "-w"]
                if is_msvc and "/FS" not in c_postargs:
                    c_postargs.append("/FS")
                sqlite_obj = self.compiler.object_filenames(
                    [sqlite_c], strip_dir=False, output_dir=build_temp
                )[0]
                unit_sq = {
                    "src": sqlite_c,
                    "obj": sqlite_obj,
                    "output_dir": build_temp,
                    "macros": c_macros,
                    "include_dirs": [os.path.dirname(sqlite_c)],
                    "extra_args": c_postargs,
                    "depends": [sqlite_c],
                    "cache_path": None,
                }
                units.append(unit_sq)
                ext_objs.append(sqlite_obj)

            # 2. Extension C++ sources
            for src in ext.sources:
                default_obj = self.compiler.object_filenames(
                    [src], strip_dir=False, output_dir=build_temp
                )[0]
                unit = {
                    "src": src,
                    "obj": default_obj,
                    "output_dir": build_temp,
                    "macros": macros,
                    "include_dirs": list(ext.include_dirs or []),
                    "extra_args": extra_args,
                    "depends": list(ext.depends or []),
                    "cache_path": None,
                }
                units.append(unit)
                ext_objs.append(default_obj)

            ext_objects[ext.name] = ext_objs

        # Pre-create all object output directories on the main thread
        for unit in units:
            obj_dir = os.path.dirname(unit["obj"])
            if obj_dir:
                os.makedirs(obj_dir, exist_ok=True)

        units_to_compile: list[dict] = []
        reused_local = 0
        reused_cache = 0

        for unit in units:
            unit_deps = [unit["src"]] + unit["depends"] + header_files
            if not self.force and is_up_to_date([unit["obj"]], unit_deps):
                reused_local += 1
                continue

            if (
                not self.force
                and object_cache_dir
                and is_python_independent_source(unit["src"])
                and os.path.isfile(unit["src"])
            ):
                effective_macros = compiler_macros + list(unit["macros"])
                cache_key = compute_object_cache_key(
                    src_path=unit["src"],
                    header_files=header_files,
                    compiler_identity=compiler_identity,
                    macros=effective_macros,
                    non_python_incdirs=unit["include_dirs"],
                    extra_args=unit["extra_args"],
                    debug=bool(self.debug),
                    plat_name=getattr(self, "plat_name", None),
                )
                obj_ext = os.path.splitext(unit["obj"])[1]
                cache_path = os.path.join(object_cache_dir, f"{cache_key}{obj_ext}")
                unit["cache_path"] = cache_path
                if os.path.isfile(cache_path):
                    shutil.copy2(cache_path, unit["obj"])
                    reused_cache += 1
                    continue

            units_to_compile.append(unit)

        # Schedule largest C/C++ sources first (LPT)
        units_to_compile.sort(
            key=lambda u: os.path.getsize(u["src"]) if os.path.isfile(u["src"]) else 0,
            reverse=True,
        )

        print(
            f"------- Compiling {len(units_to_compile)}/{len(units)} sources using "
            f"{min(max(len(units_to_compile), 1), jobs)} worker(s) "
            f"(max_jobs={jobs}, local_hits={reused_local}, cache_hits={reused_cache}) -------"
        )

        if units_to_compile:
            worker_count = min(len(units_to_compile), jobs)
            if worker_count == 1:
                for unit in units_to_compile:
                    self._compile_unit(unit)
            else:
                with ThreadPoolExecutor(max_workers=worker_count) as executor:
                    futures = [
                        executor.submit(self._compile_unit, unit) for unit in units_to_compile
                    ]
                    for f in as_completed(futures):
                        f.result()

        # Link each extension sequentially
        for ext in self.extensions:
            ext_path = self.get_ext_fullpath(ext.name)
            os.makedirs(os.path.dirname(ext_path), exist_ok=True)
            objects = list(ext_objects[ext.name])
            self._built_objects = objects[:]
            self.compiler.link_shared_object(
                objects,
                ext_path,
                libraries=self.get_libraries(ext),
                library_dirs=ext.library_dirs,
                runtime_library_dirs=ext.runtime_library_dirs,
                extra_postargs=list(ext.extra_link_args or []),
                export_symbols=self.get_export_symbols(ext),
                debug=self.debug,
                build_temp=build_temp,
                target_lang="c++",
            )


class BuildPy(_build_py):
    """Ensure SWIG-generated ezsnmp_data_logger.py exists prior to packaging Python modules."""

    def run(self) -> None:
        self.run_command("build_ext")
        super().run()


setup(
    packages=find_packages(include=["ezsnmp_data_logger", "ezsnmp_data_logger.*"]),
    package_data={
        "ezsnmp_data_logger.gui": ["static/*", "locale.json"],
        "ezsnmp_data_logger": [".dylibs/*", "libs/*", "*.dll"],
    },
    include_package_data=True,
    ext_modules=[_configure_extension()],
    cmdclass={
        "build_ext": SwigBuildExt,
        "build_py": BuildPy,
    },
)
