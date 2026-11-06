"""Tests for the Python SWIG 4.x C++17 bindings and self-containment of ezsnmp_data_logger."""

from __future__ import annotations

import ast
import json
import re
import shutil
import tomllib
from pathlib import Path

import pytest

import ezsnmp_data_logger as edl
from tools import bundle_native_package


def test_init_py_contains_only_imports_and_metadata() -> None:
    """Verify [GEN-04]: __init__.py has no executable logic or import-time side effects."""
    init_path = Path(edl.__file__).resolve()
    tree = ast.parse(init_path.read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, ast.Expr) and isinstance(node.value, ast.Constant):
            continue
        assert isinstance(
            node, (ast.Import, ast.ImportFrom, ast.Assign)
        ), f"Disallowed statement in __init__.py: {type(node).__name__}"


def test_calsemver_version_and_capabilities() -> None:
    assert edl.__version__ == "2026.11.5.1"
    assert edl.version() == "2026.11.5.1"
    assert edl.ezsnmp_target_version() == "2026.10.28a0"
    assert edl.is_valid_calsemver(edl.__version__) is True
    assert edl.is_valid_calsemver(edl.ezsnmp_target_version()) is True
    assert edl.is_valid_calsemver("2026.11.1") is True
    assert edl.is_valid_calsemver("v2026.11.5.1") is True
    assert edl.is_valid_calsemver("2026.11.5.1-alpha.0") is True
    assert edl.is_valid_calsemver("2026.01.05") is False
    assert edl.is_valid_calsemver("0.1.0") is False
    assert edl.has_sqlite3_support() is True
    assert edl.has_embedded_sqlite3() is True

    langs = list(edl.supported_swig_languages())
    assert langs == ["python", "javascript", "csharp", "go"]

    info = json.loads(edl.build_info_json(2))
    assert info["version"] == "2026.11.5.1"
    assert info["versioning_scheme"] == "CalSemVer"
    assert info["calsemver_valid"] is True
    assert info["ezsnmp_target_version"] == "2026.10.28a0"
    assert info["self_contained"] is True
    assert info["capabilities"]["sqlite3_sink"] is True
    assert info["capabilities"]["embedded_sqlite3"] is True


def test_all_project_manifests_synchronized_on_calsemver() -> None:
    """Verify [VER-01]-[VER-04]: every project manifest uses synchronized CalSemVer."""
    repo_root = Path(__file__).resolve().parent.parent
    pyproject_path = repo_root / "pyproject.toml"
    if not pyproject_path.is_file():
        pytest.skip("Repository manifests unavailable outside source checkout")

    expected_pep440 = edl.version()
    assert edl.is_valid_calsemver(expected_pep440)
    m_base = re.match(r"^(\d{4}\.\d+\.\d+)(?:\.(\d+))?(?:a(\d+)|b(\d+)|rc(\d+))?$", expected_pep440)
    assert m_base is not None
    cal_core = m_base.group(1)
    if m_base.group(2) is not None:
        expected_semver = f"{cal_core}-{m_base.group(2)}"
        expected_csharp = expected_pep440
    elif m_base.group(3) is not None:
        expected_semver = f"{cal_core}-alpha.{m_base.group(3)}"
        expected_csharp = expected_semver
    elif m_base.group(4) is not None:
        expected_semver = f"{cal_core}-beta.{m_base.group(4)}"
        expected_csharp = expected_semver
    elif m_base.group(5) is not None:
        expected_semver = f"{cal_core}-rc.{m_base.group(5)}"
        expected_csharp = expected_semver
    else:
        expected_semver = cal_core
        expected_csharp = cal_core

    pyproject = tomllib.loads(pyproject_path.read_text(encoding="utf-8"))
    assert pyproject["project"]["version"] == expected_pep440

    meson_text = (repo_root / "meson.build").read_text(encoding="utf-8")
    assert f"version: '{expected_pep440}'" in meson_text

    cmake_text = (repo_root / "CMakeLists.txt").read_text(encoding="utf-8")
    assert f"VERSION {cal_core}" in cmake_text
    assert f'EZSNMP_DL_VERSION_STRING "{expected_pep440}"' in cmake_text

    doxy_text = (repo_root / ".doxygen").read_text(encoding="utf-8")
    assert f'PROJECT_NUMBER         = "{expected_pep440}"' in doxy_text

    conf_text = (repo_root / "sphinx_docs_build" / "source" / "conf.py").read_text(encoding="utf-8")
    assert f'release = "v{expected_pep440}"' in conf_text

    npm_pkg = json.loads(
        (repo_root / "bindings" / "javascript" / "package.json").read_text(encoding="utf-8")
    )
    assert npm_pkg["version"] == expected_semver
    assert edl.is_valid_calsemver(npm_pkg["version"])

    csproj_text = (repo_root / "bindings" / "csharp" / "EzSnmpDataLogger.csproj").read_text(
        encoding="utf-8"
    )
    assert f"<Version>{expected_csharp}</Version>" in csproj_text


def test_self_contained_native_extension_bundling(tmp_path: Path) -> None:
    """Verify [PKG-02]/[PKG-03]/[PKG-06]: zero external libsqlite3 dep & clean native bundling."""
    pkg_dir = Path(edl.__file__).resolve().parent
    ext_candidates = list(pkg_dir.glob("_ezsnmp_data_logger*.so")) + list(
        pkg_dir.glob("_ezsnmp_data_logger*.pyd")
    )
    assert len(ext_candidates) >= 1
    src_ext = ext_candidates[0]

    staged_ext = tmp_path / src_ext.name
    shutil.copy2(src_ext, staged_ext)

    report = bundle_native_package.bundle_artifact(staged_ext)
    assert report["ok"] is True, f"External deps remained after bundling: {report}"
    assert report["external_dependencies"] == []

    # Verify SQLite3 was statically embedded (not copied as an external shared library)
    bundled_names = [name.lower() for name in report.get("bundled_libraries", [])]
    assert not any("sqlite" in name for name in bundled_names)


def test_datatypes_and_ezsnmp_result_ingestion() -> None:
    s_int = edl.Sample.make_int("ifInOctets", "1.3.6.1.2.1.2.2.1.10.1", 424242)
    assert s_int.type_tag == edl.TypeTag_Int64
    assert s_int.display_value() == "424242"
    assert "ifInOctets" in repr(s_int)

    # Ingest ezsnmp Result.to_dict() JSON payload directly
    ezsnmp_dict = {
        "oid": "iso.3.6.1.2.1.1.5",
        "index": "0",
        "type": "OCTETSTR",
        "value": "router-west-01",
        "converted_value": "router-west-01",
    }
    s_from_ez = edl.Sample.from_json(json.dumps(ezsnmp_dict))
    assert s_from_ez.oid_index == "0"
    assert s_from_ez.snmp_type == "OCTETSTR"
    assert s_from_ez.str_val == "router-west-01"


def test_data_logger_lifecycle_and_cli_runner(tmp_path: Path) -> None:
    cfg_dict = {
        "logger": {
            "poll_interval_ms": 25,
            "queue_depth": 1000,
            "worker_threads": 2,
            "max_cycles": 2,
        },
        "duts": [
            {
                "name": "py-dut",
                "dut_ip": "127.0.0.1",
                "enabled": True,
                "snmp": {"mode": "stub", "version": "2c", "community": "public"},
                "metrics": [
                    {"name": "sysUpTime", "oid": "1.3.6.1.2.1.1.3.0", "operation": "get"},
                    {"name": "cpuTemp", "oid": "1.3.6.1.4.1.99.1.0", "operation": "get"},
                ],
            }
        ],
        "sinks": [
            {
                "type": "sqlite3",
                "enabled": True,
                "path": str(tmp_path / "py_test.sqlite"),
                "commit_every_n": 1,
                "commit_every_ms": 100,
            }
        ],
        "gui": {"frontend": "auto", "theme": "dark"},
    }
    raw_cfg = json.dumps(cfg_dict, indent=2)

    rep = edl.validate_config(raw_cfg)
    assert rep.valid is True
    assert rep.enabled_duts == 1

    logger = edl.DataLogger(raw_cfg)
    batches = logger.run_once()
    assert len(batches) == 1
    assert len(batches[0].samples) == 2

    inspected = json.loads(edl.DataLogger.inspect_json(raw_cfg, 10, "py-dut", "", 2))
    assert inspected["ok"] is True
    assert inspected["count"] == 2

    # Verify SWIG exception translation (ConfigError -> ValueError)
    with pytest.raises(ValueError):
        edl.parse_config("{invalid json")

    # Verify programmatic CliRunner
    cli_res = edl.CliRunner.execute(edl.StringVector(["version", "--json"]))
    assert cli_res.exit_code == 0
    assert json.loads(cli_res.stdout_text)["version"] == "2026.11.5.1"
