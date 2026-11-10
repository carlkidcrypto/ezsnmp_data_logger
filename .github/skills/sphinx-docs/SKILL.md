---
name: sphinx-docs
description: Use when building, updating, or verifying Sphinx documentation, software requirements specifications, or versioned documentation landing pages.
---

# Sphinx Documentation Guide (ezsnmp_data_logger)

Use this skill when authoring, building, or validating Sphinx documentation and software requirements specifications in `ezsnmp_data_logger`.

## 1. Standards & Runtimes

- **Standard Runtime**: Standardized across CI workflows on **Python 3.14** (`actions/setup-python@v7` with `python-version: "3.14"`).
- **Zero-Warning Requirement**: Builds must pass with `-W` (treating all warnings as errors):
  ```bash
  sphinx-build -b html -W sphinx_docs_build/source sphinx_docs_build/build/html
  ```

---

## 2. Software Requirements Specification Standards

All requirements in `sphinx_docs_build/source/software_requirements.rst` must strictly comply with:
1. **80 Characters Line Length Limit**: Every line must be $\le 80$ characters long.
   Verify with:
   ```bash
   awk 'length > 80 { print NR, length, $0 }' sphinx_docs_build/source/software_requirements.rst
   ```
2. **Blank Lines Between Requirements**: Separate every distinct requirement block with a blank newline.
3. **Single Responsibility**: Each requirement Shall test exactly one capability or feature.

---

## 3. Versioned Documentation & Landing Page

- **Directory Naming**: Always lowercase `docs/html_v<version>` (e.g., `docs/html_v2026.11.9/`).
- **Prerelease Replacement**: When a stable release (e.g. `2026.11.9`) is published, it replaces and retires all prerelease directories for that series.
- **Landing Page Sentinel**: Update `sphinx_docs_build/landing/source/index.rst` between `VERSION_LIST_START` and `VERSION_LIST_END` with active versions in reverse chronological order.
- **`.nojekyll`**: Always ensure `.nojekyll` is present in deployment outputs so underscore-prefixed directories (`_static/`, `_sources/`) are preserved.
