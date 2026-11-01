"""Verify multi-language SWIG codegen and repository JSON config templates."""

from __future__ import annotations

import shutil
from pathlib import Path

import pytest

import ezsnmp_data_logger as edl
from tools.generate_swig_bindings import SWIG_LANGUAGE_TARGETS, generate_for_language

REPO_ROOT = Path(__file__).resolve().parent.parent


def test_all_repository_json_configs_are_valid() -> None:
    cfg_dir = REPO_ROOT / "configs"
    json_files = sorted(cfg_dir.glob("*.json"))
    assert len(json_files) >= 4
    for jf in json_files:
        rep = edl.validate_config_file(str(jf))
        assert rep.valid is True, f"{jf.name} failed validation: {list(rep.errors)}"


@pytest.mark.skipif(shutil.which("swig") is None, reason="SWIG CLI not installed on PATH")
def test_swig_generates_wrappers_for_all_16_targets(tmp_path: Path) -> None:
    swig_bin = shutil.which("swig")
    assert swig_bin is not None
    for lang in SWIG_LANGUAGE_TARGETS:
        res = generate_for_language(swig_bin, lang, tmp_path)
        assert res["ok"] is True, f"SWIG codegen failed for {lang}: {res['stderr']}"
        assert len(res["generated_files"]) >= 1
