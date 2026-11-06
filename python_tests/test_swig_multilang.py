"""Verify 4-language SWIG codegen (python, javascript, csharp, go) and JSON config templates."""

from __future__ import annotations

import shutil
from pathlib import Path

import pytest

import ezsnmp_data_logger as edl
from tools.generate_swig_bindings import SWIG_LANGUAGE_TARGETS, generate_for_language
from tools.generate_winget_manifest import generate_manifests

REPO_ROOT = Path(__file__).resolve().parent.parent


def test_all_repository_json_configs_are_valid() -> None:
    cfg_dir = REPO_ROOT / "configs"
    json_files = sorted(cfg_dir.glob("*.json"))
    assert len(json_files) >= 4
    for jf in json_files:
        rep = edl.validate_config_file(str(jf))
        assert rep.valid is True, f"{jf.name} failed validation: {list(rep.errors)}"


def test_supported_swig_targets_match_bindings_scaffolds() -> None:
    assert list(SWIG_LANGUAGE_TARGETS.keys()) == ["python", "javascript", "csharp", "go"]
    bindings_dirs = sorted(p.name for p in (REPO_ROOT / "bindings").iterdir() if p.is_dir())
    assert bindings_dirs == ["csharp", "go", "javascript"]


@pytest.mark.skipif(shutil.which("swig") is None, reason="SWIG CLI not installed on PATH")
def test_swig_generates_wrappers_for_all_4_targets(tmp_path: Path) -> None:
    swig_bin = shutil.which("swig")
    assert swig_bin is not None
    for lang in SWIG_LANGUAGE_TARGETS:
        res = generate_for_language(swig_bin, lang, tmp_path)
        assert res["ok"] is True, f"SWIG codegen failed for {lang}: {res['stderr']}"
        assert len(res["generated_files"]) >= 1


def test_winget_manifest_generation(tmp_path: Path) -> None:
    # Test 1: Portable zip manifest generation
    zip_manifests = generate_manifests(
        package_id="carlkidcrypto.EzSnmpDataLogger",
        package_version="2026.11.6",
        installer_url="https://github.com/carlkidcrypto/ezsnmp_data_logger/releases/download/v2026.11.6/EzSnmpDataLogger-Windows-x64.zip",
        installer_sha256="aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899",
        out_dir=tmp_path / "zip_out",
        architecture="x64",
        installer_type="zip",
    )
    assert set(zip_manifests.keys()) == {"version", "locale", "installer"}
    canonical_zip_dir = (
        tmp_path
        / "zip_out"
        / "manifests"
        / "c"
        / "carlkidcrypto"
        / "EzSnmpDataLogger"
        / "2026.11.6"
    )
    for p in zip_manifests.values():
        assert p.is_file()
        assert p.parent == canonical_zip_dir
        content = p.read_text(encoding="utf-8")
        assert "carlkidcrypto.EzSnmpDataLogger" in content
        assert "2026.11.6" in content

    zip_installer_content = zip_manifests["installer"].read_text(encoding="utf-8")
    assert "InstallerType: zip" in zip_installer_content
    assert "NestedInstallerType: portable" in zip_installer_content
    assert "RelativeFilePath: EzSnmpDataLogger.exe" in zip_installer_content
    assert "PortableCommandAlias: ezsnmp-data-logger" in zip_installer_content

    # Test 2: Inno Setup executable manifest generation (primary Windows distribution)
    inno_manifests = generate_manifests(
        package_id="carlkidcrypto.EzSnmpDataLogger",
        package_version="2026.11.6",
        installer_url="https://github.com/carlkidcrypto/ezsnmp_data_logger/releases/download/v2026.11.6/EzSnmpDataLogger-Windows-x64-Setup.exe",
        installer_sha256="11223344556677889900aabbccddeeff11223344556677889900aabbccddeeff",
        out_dir=tmp_path / "inno_out",
        architecture="x64",
        installer_type="inno",
    )
    canonical_inno_dir = (
        tmp_path
        / "inno_out"
        / "manifests"
        / "c"
        / "carlkidcrypto"
        / "EzSnmpDataLogger"
        / "2026.11.6"
    )
    for p in inno_manifests.values():
        assert p.is_file()
        assert p.parent == canonical_inno_dir

    inno_installer_content = inno_manifests["installer"].read_text(encoding="utf-8")
    assert "InstallerType: inno" in inno_installer_content
    assert "Scope: user" in inno_installer_content
    assert "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART" in inno_installer_content
    assert "AppsAndFeaturesEntries:" in inno_installer_content
    assert "{E5D4B692-7E5A-483A-9E86-443E838F5C1B}_is1" in inno_installer_content

    locale_content = inno_manifests["locale"].read_text(encoding="utf-8")
    assert "Moniker: ezsnmp-data-logger" in locale_content
    assert "License: MIT" in locale_content
    assert "Publisher: carlkidcrypto" in locale_content


def test_dependabot_configuration() -> None:
    dependabot_path = REPO_ROOT / ".github" / "dependabot.yml"
    assert dependabot_path.is_file(), ".github/dependabot.yml must exist"
    content = dependabot_path.read_text(encoding="utf-8")
    assert "version: 2" in content
    assert 'package-ecosystem: "github-actions"' in content
    assert 'package-ecosystem: "pip"' in content
    assert 'package-ecosystem: "npm"' in content
    assert 'package-ecosystem: "nuget"' in content
    assert 'package-ecosystem: "gomod"' in content

    try:
        import yaml

        data = yaml.safe_load(content)
        assert data.get("version") == 2
        updates = data.get("updates", [])
        assert len(updates) >= 5
        ecosystem_dirs = {(u.get("package-ecosystem"), u.get("directory")) for u in updates}
        assert ("github-actions", "/") in ecosystem_dirs
        assert ("pip", "/") in ecosystem_dirs
        assert ("npm", "/bindings/javascript") in ecosystem_dirs
        assert ("nuget", "/bindings/csharp") in ecosystem_dirs
        assert ("gomod", "/bindings/go") in ecosystem_dirs
        assert ("pip", "/sphinx_docs_build") in ecosystem_dirs
        assert ("pip", "/sphinx_docs_build/landing") in ecosystem_dirs
        for u in updates:
            d = u.get("directory", "")
            target_dir = REPO_ROOT if d == "/" else REPO_ROOT / d.lstrip("/")
            assert target_dir.is_dir(), f"Dependabot target directory {d} does not exist"
    except ImportError:
        pass


def test_sonarcloud_configuration() -> None:
    sonar_path = REPO_ROOT / ".sonarcloud.properties"
    assert sonar_path.is_file(), ".sonarcloud.properties must exist"
    content = sonar_path.read_text(encoding="utf-8")
    assert "sonar.exclusions" in content
    assert "sonar.cpd.exclusions" in content
    assert "docs/**" in content
    assert ".github/**" in content
    assert "third_party/**" in content
    assert "python_tests/**" in content
    assert "cpp_tests/**" in content


def test_smart_workflow_triggers_and_runners() -> None:
    workflows_dir = REPO_ROOT / ".github" / "workflows"
    assert workflows_dir.is_dir()

    # Workflows implementing smart changed-files preflight jobs
    smart_workflows = [
        "tests_python.yml",
        "tests_cpp_native.yml",
        "swig_multilang.yml",
        "sphinx_build.yml",
    ]
    for wf_name in smart_workflows:
        wf_path = workflows_dir / wf_name
        assert wf_path.is_file(), f"{wf_name} must exist"
        wf_content = wf_path.read_text(encoding="utf-8")
        assert (
            "tj-actions/changed-files" in wf_content
        ), f"{wf_name} missing tj-actions/changed-files"
        assert "check-source-changes:" in wf_content, f"{wf_name} missing check-source-changes job"
        assert "outputs.any_changed" in wf_content, f"{wf_name} missing any_changed output"

    # Workflows utilizing carlkidcrypto/os-specific-runner for multi-OS step consolidation
    runner_workflows = [
        "tests_python.yml",
        "tests_cpp_native.yml",
        "swig_multilang.yml",
        "build_and_publish_to_nuget.yml",
    ]
    for wf_name in runner_workflows:
        wf_path = workflows_dir / wf_name
        assert wf_path.is_file(), f"{wf_name} must exist"
        wf_content = wf_path.read_text(encoding="utf-8")
        assert (
            "carlkidcrypto/os-specific-runner" in wf_content
        ), f"{wf_name} missing carlkidcrypto/os-specific-runner"

    # PyPI and TestPyPI distribution workflows pre-provision third_party/ezsnmp
    pypi_workflows = [
        "build_and_publish_to_pypi.yml",
        "build_and_publish_to_test_pypi.yml",
    ]
    for wf_name in pypi_workflows:
        wf_path = workflows_dir / wf_name
        assert wf_path.is_file(), f"{wf_name} must exist"
        wf_content = wf_path.read_text(encoding="utf-8")
        assert (
            "repository: carlkidcrypto/ezsnmp" in wf_content
        ), f"{wf_name} must checkout carlkidcrypto/ezsnmp"
        assert "path: third_party/ezsnmp" in wf_content, f"{wf_name} must target third_party/ezsnmp"
