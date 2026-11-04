"""Unit tests for parallel compilation, build utilities, and object caching."""

from __future__ import annotations

import os
import sys
import time
from pathlib import Path

import pytest

ROOT_DIR = Path(__file__).resolve().parent.parent
if str(ROOT_DIR / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT_DIR / "tools"))

import build_utils


def test_resolve_build_jobs_defaults_to_cpus_minus_one(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("EZSNMP_BUILD_JOBS", raising=False)
    assert build_utils.resolve_build_jobs(cpu_count=8) == 7
    assert build_utils.resolve_build_jobs(cpu_count=4) == 3
    assert build_utils.resolve_build_jobs(cpu_count=1) == 1


def test_resolve_build_jobs_honors_explicit_parallel(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("EZSNMP_BUILD_JOBS", raising=False)
    assert build_utils.resolve_build_jobs(explicit_jobs=True, cpu_count=6) == 5
    assert build_utils.resolve_build_jobs(explicit_jobs=2, cpu_count=6) == 2


def test_resolve_build_jobs_env_override_and_validation(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("EZSNMP_BUILD_JOBS", "4")
    assert build_utils.resolve_build_jobs(explicit_jobs=2, cpu_count=8) == 4

    monkeypatch.setenv("EZSNMP_BUILD_JOBS", "auto")
    assert build_utils.resolve_build_jobs(explicit_jobs=2, cpu_count=6) == 6

    monkeypatch.setenv("EZSNMP_BUILD_JOBS", "0")
    with pytest.raises(ValueError, match="must be >= 1"):
        build_utils.resolve_build_jobs(cpu_count=4)

    monkeypatch.setenv("EZSNMP_BUILD_JOBS", "not-an-int")
    with pytest.raises(ValueError, match="expected a positive integer"):
        build_utils.resolve_build_jobs(cpu_count=4)


def test_is_up_to_date_tracks_timestamps_and_missing_files(tmp_path: Path) -> None:
    inp = tmp_path / "input.cpp"
    out = tmp_path / "output.o"

    assert not build_utils.is_up_to_date([], [str(inp)])
    inp.write_text("int x = 1;\n", encoding="utf-8")
    assert not build_utils.is_up_to_date([str(out)], [str(inp)])

    out.write_text("obj", encoding="utf-8")
    now = time.time()
    os.utime(inp, (now - 10, now - 10))
    os.utime(out, (now, now))
    assert build_utils.is_up_to_date([str(out)], [str(inp)])

    os.utime(inp, (now + 5, now + 5))
    assert not build_utils.is_up_to_date([str(out)], [str(inp)])


def test_is_python_independent_source() -> None:
    assert build_utils.is_python_independent_source("src/version.cpp")
    assert build_utils.is_python_independent_source("src/data_logger.cpp")
    assert build_utils.is_python_independent_source("third_party/sqlite3/sqlite3.c")
    assert not build_utils.is_python_independent_source("src/ezsnmp_data_logger_wrap.cxx")


def test_collect_files_with_suffixes(tmp_path: Path) -> None:
    (tmp_path / "a.h").write_text("// a", encoding="utf-8")
    (tmp_path / "b.hpp").write_text("// b", encoding="utf-8")
    (tmp_path / "c.cpp").write_text("// c", encoding="utf-8")
    sub = tmp_path / "nested"
    sub.mkdir()
    (sub / "d.h").write_text("// d", encoding="utf-8")

    headers = build_utils.collect_files_with_suffixes([tmp_path], (".h", ".hpp"))
    assert len(headers) == 3
    assert any(h.endswith("a.h") for h in headers)
    assert any(h.endswith("b.hpp") for h in headers)
    assert any(h.endswith("d.h") for h in headers)


def test_compute_object_cache_key_deterministic_and_sensitive(tmp_path: Path) -> None:
    src = tmp_path / "sample.cpp"
    src.write_text("int f() { return 42; }\n", encoding="utf-8")
    hdr = tmp_path / "sample.h"
    hdr.write_text("#define VAL 42\n", encoding="utf-8")

    key1 = build_utils.compute_object_cache_key(
        src_path=str(src),
        header_files=[str(hdr)],
        compiler_identity={"compiler_type": "clang", "cmd": ["clang++"]},
        macros=[("FOO", "1")],
        non_python_incdirs=[str(tmp_path)],
        extra_args=["-O3"],
        debug=False,
        plat_name="darwin",
    )
    key2 = build_utils.compute_object_cache_key(
        src_path=str(src),
        header_files=[str(hdr)],
        compiler_identity={"compiler_type": "clang", "cmd": ["clang++"]},
        macros=[("FOO", "1")],
        non_python_incdirs=[str(tmp_path)],
        extra_args=["-O3"],
        debug=False,
        plat_name="darwin",
    )
    assert key1 == key2

    # Modifying header changes the cache key
    hdr.write_text("#define VAL 99\n", encoding="utf-8")
    key3 = build_utils.compute_object_cache_key(
        src_path=str(src),
        header_files=[str(hdr)],
        compiler_identity={"compiler_type": "clang", "cmd": ["clang++"]},
        macros=[("FOO", "1")],
        non_python_incdirs=[str(tmp_path)],
        extra_args=["-O3"],
        debug=False,
        plat_name="darwin",
    )
    assert key1 != key3
