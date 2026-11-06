=================
Development Guide
=================

This guide covers building the self-contained C++17 engine, running GoogleTest and ``pytest`` suites, generating the 4 SWIG language bindings (Python, JavaScript/Node.js N-API, C#, Go), and formatting code for **EzSnmp Data Logger**.

CalSemVer Alpha Standardization
===============================

``ezsnmp_data_logger`` follows **CalSemVer** (`YYYY.M.DaN`, currently ``2026.11.6.1``) and standardizes on `ezsnmp <https://github.com/carlkidcrypto/ezsnmp>`_'s self-contained CalSemVer alpha release stream (``ezsnmp>=2026.11.4a0``, git tag ``v2026.11.4a0``) until a stable CalSemVer release is published.

Always pass ``--pre`` when installing ``ezsnmp`` or ``ezsnmp-data-logger`` from PyPI during the alpha testing period:

.. code-block:: bash

   python3 -m pip install --pre -r requirements.txt

Provisioning Self-Contained Native Sources
==========================================

Before compiling from a fresh git clone, provision the SHA-256-verified SQLite3 C amalgamation (`third_party/sqlite3/`) and pinned ``ezsnmp`` ``v2026.11.4a0`` C++17 sources:

.. code-block:: bash

   python3 tools/prepare_sqlite3_amalgamation.py
   python3 tools/prepare_ezsnmp_source.py

(Note: ``python3 setup.py build_ext`` runs both helpers automatically when ``EZSNMP_DL_AUTO_FETCH_DEPS`` is not ``0``.)

Building & Testing C++17 Core (Meson)
=====================================

.. code-block:: bash

   meson setup build
   meson compile -C build
   meson test -C build -v

Building & Testing Python Package, CLI & Cross-Platform GUI
===========================================================

.. code-block:: bash

   python3 -m pip install --pre -e .
   pytest -v

Generating & Bundling the 4 SWIG Language Targets
=================================================

.. code-block:: bash

   # Generate wrappers for python, javascript (Node.js N-API), csharp, and go
   python3 tools/generate_swig_bindings.py --out-dir build/swig_generated

   # Relocate and bundle non-OS runtime libraries for any compiled native artifact
   python3 tools/bundle_native_package.py <path-to-compiled-extension> --json

Code Formatting & Workflow Linting
==================================

* **C++17 Formatting:** ``clang-format -i include/ezsnmp_data_logger/*.h src/*.cpp cpp_tests/*.h cpp_tests/*.cpp`` (excluding generated ``src/ezsnmp_data_logger_wrap.cxx``)
* **Python Formatting:** ``black .`` (configured in ``pyproject.toml`` to skip SWIG-generated ``ezsnmp_data_logger/ezsnmp_data_logger.py`` and ``third_party/``)
* **GitHub Actions Workflows:** ``yamllint -c .yamllint .github/workflows/*.yml`` and ``actionlint``
* **Automated Dependency Maintenance (Dependabot):** ``.github/dependabot.yml`` configures weekly version updates for GitHub Actions, Python, npm, NuGet, and Go ecosystems.
* **Code Quality & Static Analysis (SonarQube Cloud):** Root ``.sonarcloud.properties`` configures Automatic Analysis scope to achieve an A rating across security, reliability, and maintainability.

Software Requirements Governance
================================

Every architectural feature or behavior change must trace to a requirement in :doc:`software_requirements`. Update ``sphinx_docs_build/source/software_requirements.rst`` first before modifying implementation code.
