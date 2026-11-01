"""Tests for the Python SWIG 4.x C++17 bindings of ezsnmp_data_logger."""

from __future__ import annotations

import ast
import json
from pathlib import Path

import pytest

import ezsnmp_data_logger as edl


def test_init_py_contains_only_imports_and_metadata() -> None:
    """Verify [GEN-004]: __init__.py has no executable logic or import-time side effects."""
    init_path = Path(edl.__file__).resolve()
    tree = ast.parse(init_path.read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, ast.Expr) and isinstance(node.value, ast.Constant):
            continue
        assert isinstance(
            node, (ast.Import, ast.ImportFrom, ast.Assign)
        ), f"Disallowed statement in __init__.py: {type(node).__name__}"


def test_version_and_capabilities() -> None:
    assert edl.version() == "0.2.0"
    assert edl.has_sqlite3_support() is True
    langs = list(edl.supported_swig_languages())
    assert "python" in langs
    assert "javascript" in langs
    assert "java" in langs
    assert "csharp" in langs
    assert "go" in langs

    info = json.loads(edl.build_info_json(2))
    assert info["version"] == "0.2.0"
    assert info["capabilities"]["sqlite3_sink"] is True
    assert info["capabilities"]["ezsnmp"] is True


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
    assert json.loads(cli_res.stdout_text)["version"] == "0.2.0"
