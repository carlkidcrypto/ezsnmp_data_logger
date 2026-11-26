"""ezsnmp_data_logger: C++17 SNMP Data Logger powered by ezsnmp with SWIG bindings."""

import os
import sys
from pathlib import Path

_DLL_DIR_HANDLES = []


def _register_windows_dll_directories() -> None:  # pragma: no cover
    """Register bundled native DLL directories on Windows before loading the extension."""
    if sys.platform != "win32" or not hasattr(os, "add_dll_directory"):
        return

    pkg_dir = Path(__file__).resolve().parent
    candidates = [
        pkg_dir,
        pkg_dir / ".libs",
        pkg_dir.parent / f"{pkg_dir.name}.libs",
    ]
    meipass = getattr(sys, "_MEIPASS", None)
    if meipass:
        meipass_dir = Path(meipass)
        candidates.extend(
            [
                meipass_dir,
                meipass_dir / "ezsnmp_data_logger",
                meipass_dir / "ezsnmp_data_logger" / ".libs",
            ]
        )

    seen = set()
    for candidate in candidates:
        try:
            resolved = candidate.resolve()
        except OSError:
            resolved = candidate
        if resolved in seen or not candidate.is_dir():
            continue
        seen.add(resolved)
        try:
            _DLL_DIR_HANDLES.append(os.add_dll_directory(str(resolved)))
        except OSError:
            pass


_register_windows_dll_directories()

from .ezsnmp_data_logger import (
    Batch,
    BatchVector,
    CliResult,
    CliRunner,
    DataLogger,
    DutConfig,
    DutConfigVector,
    DutProbeResult,
    DutProbeResultVector,
    GuiConfig,
    InspectRow,
    InspectRowVector,
    LoggerConfig,
    LoggerRuntimeConfig,
    LoggerStats,
    MetricConfig,
    MetricConfigVector,
    Sample,
    SampleVector,
    SinkConfig,
    SinkConfigVector,
    SnmpConfig,
    StringVector,
    TypeTag_Float64,
    TypeTag_Int64,
    TypeTag_Null,
    TypeTag_Str,
    ValidationReport,
    build_info_json,
    config_json_schema,
    ezsnmp_target_version,
    generate_template_config_json,
    has_embedded_sqlite3,
    has_ezsnmp_support,
    has_psql_support,
    has_sqlite3_support,
    is_valid_calsemver,
    parse_config,
    parse_config_file,
    supported_swig_languages,
    type_tag_to_string,
    validate_config,
    validate_config_file,
    version,
)

__version__ = "2026.11.24b0"

__all__ = [
    "__version__",
    "Batch",
    "BatchVector",
    "CliResult",
    "CliRunner",
    "DataLogger",
    "DutConfig",
    "DutConfigVector",
    "DutProbeResult",
    "DutProbeResultVector",
    "GuiConfig",
    "InspectRow",
    "InspectRowVector",
    "LoggerConfig",
    "LoggerRuntimeConfig",
    "LoggerStats",
    "MetricConfig",
    "MetricConfigVector",
    "Sample",
    "SampleVector",
    "SinkConfig",
    "SinkConfigVector",
    "SnmpConfig",
    "StringVector",
    "TypeTag_Float64",
    "TypeTag_Int64",
    "TypeTag_Null",
    "TypeTag_Str",
    "ValidationReport",
    "build_info_json",
    "config_json_schema",
    "ezsnmp_target_version",
    "generate_template_config_json",
    "has_embedded_sqlite3",
    "has_ezsnmp_support",
    "has_psql_support",
    "has_sqlite3_support",
    "is_valid_calsemver",
    "parse_config",
    "parse_config_file",
    "supported_swig_languages",
    "type_tag_to_string",
    "validate_config",
    "validate_config_file",
    "version",
]
