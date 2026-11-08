---
name: release-management
description: Use when preparing a new release, bumping version numbers across ezsnmp_data_logger build/doc files, running release verification, or opening release PRs.
---

# Release Management Guide (ezsnmp_data_logger)

Use this skill when preparing a new version release for `ezsnmp_data_logger`, bumping project version strings, verifying build and test integrity, and creating the release pull request.

## 1. Governance & Strict Maintainer-Only Policy

> [!CRITICAL]
> **MAINTAINER-ONLY PR MERGING & PUBLISHING**:
> - **NEVER** merge pull requests into `main` (`gh pr merge`).
> - **NEVER** publish GitHub releases, create git release tags, or trigger automated package publishing workflows (`workflow_dispatch`).
> - Merging PRs and creating/publishing formal releases are strictly reserved for the maintainer (@carlkidcrypto).
> - Use targeted staging only (`git add <file1> <file2>`, never blanket `git add .` or `-A`).

---

## 2. Release Tools Overview

`ezsnmp_data_logger` provides a standardized CalSemVer release bumping script in `release_tools/`:
- **Main Script**: `release_tools/bump_version.sh`
- **Synchronized Files**: Atomically updates the version string across:
  - `pyproject.toml`
  - `ezsnmp_data_logger/__init__.py`
  - `ezsnmp_data_logger/gui/launcher.py`
  - `bindings/javascript/package.json` and `package-lock.json`
  - `bindings/csharp/EzSnmpDataLogger.csproj`
  - `src/version.cpp` and `include/ezsnmp_data_logger/version.h`
  - `CMakeLists.txt` and `meson.build`
  - `tools/package_desktop_app.py`
  - `tools/generate_winget_manifest.py`
  - `tools/windows_installer.iss`
  - `.github/workflows/build_and_publish_desktop_apps.yml`
  - `.github/workflows/build_and_publish_to_winget.yml`
  - Sphinx documentation (`conf.py`, `software_requirements.rst`, guides)
  - Unit tests in `python_tests/` and `cpp_tests/`
- **Excluded**:
  - `CHANGELOG.md` (managed automatically by `auto_change_log.md` workflow).
  - Versioned docs in `docs/html_v*` and landing page history (`sphinx_docs_build/landing/**`).

---

## 3. Release Bumping Workflow

### Step 1: Create a Release Branch
Always branch off the latest `main`:
```bash
git checkout main
git pull origin main
git checkout -b chore/release-v<version>
```

### Step 2: Bump and Verify
Run the bump script:
```bash
bash release_tools/bump_version.sh <version>
```
Preview changes beforehand if needed:
```bash
bash release_tools/bump_version.sh --dry-run <version>
```

### Step 3: Run Local Validation
1. **Software Requirements Line Length**:
   ```bash
   awk 'length > 80 { print NR, length, $0 }' sphinx_docs_build/source/software_requirements.rst
   ```
   Must return 0 lines exceeding 80 characters.
2. **Rebuild Native Extension**:
   ```bash
   python setup.py build_ext --inplace
   ```
3. **Run Unit Tests**:
   ```bash
   pytest python_tests/
   ```
4. **Sphinx Documentation Zero-Warning Build**:
   ```bash
   sphinx-build -b html -W sphinx_docs_build/source sphinx_docs_build/build/html
   ```
5. **Python Code Formatting**:
   ```bash
   black --check setup.py ezsnmp_data_logger tools python_tests
   ```

### Step 4: Targeted Staging and PR
Stage each bumped file explicitly (`git add <file1> <file2> ...`). Open a PR against `main` and hand off to maintainer.
