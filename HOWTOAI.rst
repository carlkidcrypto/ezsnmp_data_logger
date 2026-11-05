============================================
How to Use AI with ezsnmp_data_logger
============================================

*A practical guide for contributing to ezsnmp_data_logger using AI coding assistants*

``ezsnmp_data_logger`` combines a portable **C++17** telemetry engine, `ezsnmp <https://github.com/carlkidcrypto/ezsnmp>`_'s Net-SNMP ``SessionBase`` layer (`v2026.10.28a0` CalSemVer alpha baseline), **self-contained SWIG 4.x packages** across **Python, JavaScript (Node.js N-API), C#, and Go**, **100% JSON configuration**, a structured **CLI**, and **cross-platform GUIs** for **Linux, macOS, and Windows**. Contributors using GitHub Copilot, Cursor, Claude, Antigravity, or other AI tools must uphold the architectural and quality standards below.

----

Core Architectural Invariants
=============================

1. **Software Requirements First (``software_requirements.rst``):**
   ``sphinx_docs_build/source/software_requirements.rst`` is the authoritative specification. Every requirement uses a bracketed ``[PREFIX-nn]`` identifier and RFC-style ``Shall`` statement. Always update ``software_requirements.rst`` **before** changing behavior or architecture.
2. **Self-Contained Packages & CalSemVer Alpha Baseline:**
   All 4 distributed language packages (``python``, ``javascript``, ``csharp``, ``go``) must remain **self-contained** (embedding SQLite3 via ``tools/prepare_sqlite3_amalgamation.py`` and bundling Net-SNMP 5.10 / OpenSSL via ``cibuildwheel`` or ``tools/bundle_native_package.py``) and standardized on **CalSemVer** alpha versioning (`2026.11.6` + ``ezsnmp>=2026.10.28a0`` with ``pip install --pre``) until a stable release is published.
3. **EzSnmp Network Layer:**
   All live SNMPv1, SNMPv2c, and SNMPv3 ``get``, ``getnext``, ``walk``, ``bulkget``, ``bulkwalk``, and ``set`` operations must flow through ``ezsnmp``'s C++17 ``SessionBase`` and ``Result`` classes (``EzSnmpPoller``). Never duplicate raw Net-SNMP PDU construction in the logger layer.
4. **100% JSON Configuration:**
   Every engine, DUT, SNMPv3 USM, storage sink, CLI, and GUI setting must be stored in RFC-8259 JSON and validated by ``config.cpp``. Do not introduce YAML, TOML, INI, or separate GUI config formats.
5. **Supported SWIG 4.x Target Languages:**
   Keep public C++17 headers in ``include/ezsnmp_data_logger/`` SWIG-clean and verify all 4 supported targets (``python``, ``javascript``, ``csharp``, ``go``) via ``python3 tools/generate_swig_bindings.py``.
6. **GUI Strictly Drives the CLI Behind the Scenes (Linux / macOS / Windows):**
   Both ``DesktopLoggerStudio`` (``tkinter.ttk``) and ``StudioHttpServer`` (stdlib HTTP + browser SPA) must invoke the CLI via ``CliSubprocessBridge`` + ``--json`` rather than calling C++ polling internals directly, and must work out-of-the-box on **Linux, macOS, and Windows**.
7. **Pure ``__init__.py``:**
   ``ezsnmp_data_logger/__init__.py`` must contain only module docstrings, imports, ``__version__``, and ``__all__`` exports—no executable functions or classes.

----

Testing & Verification Checklist
================================

Before opening a pull request, confirm every check passes locally:

1. **Provision Embedded Dependencies & Native C++17 GoogleTest Suite:**

   .. code-block:: bash

      python3 tools/prepare_sqlite3_amalgamation.py
      python3 tools/prepare_ezsnmp_source.py
      meson setup build
      meson compile -C build
      meson test -C build -v

2. **Python Extension, Self-Containment, CLI & Cross-Platform GUI Tests:**

   .. code-block:: bash

      python3 setup.py build_ext --inplace
      pytest -v

3. **All 4 SWIG Language Targets (Python, Node.js, C#, Go):**

   .. code-block:: bash

      python3 tools/generate_swig_bindings.py --out-dir build/swig_generated

4. **Doxygen + Sphinx Documentation (Warnings as Errors):**

   .. code-block:: bash

      mkdir -p doxygen_docs_build
      doxygen .doxygen
      make -C sphinx_docs_build clean && make -C sphinx_docs_build html SPHINXOPTS="-W"
      make -C sphinx_docs_build/landing html SPHINXOPTS="-W"

5. **Formatting & Workflow Linting:**

   * Run ``clang-format-20`` on hand-written C++17 headers/sources/tests (excluding generated ``src/ezsnmp_data_logger_wrap.cxx`` and ``third_party/``).
   * Run ``black .`` on hand-written Python modules/tests.
   * Validate ``.github/workflows/*.yml`` with ``yamllint -c .yamllint`` and ``actionlint``.
