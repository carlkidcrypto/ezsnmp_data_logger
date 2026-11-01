============================================
How to Use AI with ezsnmp_data_logger
============================================

*A practical guide for contributing to ezsnmp_data_logger using AI coding assistants*

``ezsnmp_data_logger`` combines a portable **C++17** telemetry engine, `ezsnmp <https://github.com/carlkidcrypto/ezsnmp>`_'s Net-SNMP ``SessionBase`` layer, **SWIG 4.x** bindings across 16 target languages, **100% JSON configuration**, a structured **CLI**, and **cross-platform GUIs** for **Linux, macOS, and Windows**. Contributors using GitHub Copilot, Cursor, Claude, Antigravity, or other AI tools must uphold the architectural and quality standards below.

----

Core Architectural Invariants
=============================

1. **Software Requirements First (``software_requirements.rst``):**
   ``sphinx_docs_build/source/software_requirements.rst`` is the authoritative specification. Every requirement uses a bracketed ``[PREFIX-nn]`` identifier and RFC-style ``Shall`` statement. Always update ``software_requirements.rst`` **before** changing behavior or architecture.
2. **EzSnmp Network Layer:**
   All live SNMPv1, SNMPv2c, and SNMPv3 ``get``, ``getnext``, ``walk``, ``bulkget``, ``bulkwalk``, and ``set`` operations must flow through ``ezsnmp``'s C++17 ``SessionBase`` and ``Result`` classes (``EzSnmpPoller``). Never duplicate raw Net-SNMP PDU construction in the logger layer.
3. **100% JSON Configuration:**
   Every engine, DUT, SNMPv3 USM, storage sink, CLI, and GUI setting must be stored in RFC-8259 JSON and validated by ``config.cpp``. Do not introduce YAML, TOML, INI, or separate GUI config formats.
4. **Universal SWIG 4.x Compatibility:**
   Keep public C++17 headers in ``include/ezsnmp_data_logger/`` SWIG-clean (guard C++-only callbacks or operators with ``#ifndef SWIG``) and verify all 16 language targets via ``python3 tools/generate_swig_bindings.py``.
5. **GUI Strictly Drives the CLI Behind the Scenes (Linux / macOS / Windows):**
   Both ``DesktopLoggerStudio`` (``tkinter.ttk``) and ``StudioHttpServer`` (stdlib HTTP + browser SPA) must invoke the CLI via ``CliSubprocessBridge`` + ``--json`` rather than calling C++ polling internals directly, and must work out-of-the-box on **Linux, macOS, and Windows**.
6. **Pure ``__init__.py``:**
   ``ezsnmp_data_logger/__init__.py`` must contain only module docstrings, imports, ``__version__``, and ``__all__`` exports—no executable functions or classes.

----

Testing & Verification Checklist
================================

Before opening a pull request, confirm every check passes locally:

1. **Native C++17 Build & GoogleTest Suite:**

   .. code-block:: bash

      meson setup build
      meson compile -C build
      meson test -C build -v

2. **Python Extension, CLI & Cross-Platform GUI Tests:**

   .. code-block:: bash

      python3 setup.py build_ext --inplace
      pytest -v

3. **All 16 SWIG Language Targets:**

   .. code-block:: bash

      python3 tools/generate_swig_bindings.py

4. **Doxygen + Sphinx Documentation (Warnings as Errors):**

   .. code-block:: bash

      mkdir -p doxygen_docs_build
      doxygen .doxygen
      make -C sphinx_docs_build clean && make -C sphinx_docs_build html SPHINXOPTS="-W"
      make -C sphinx_docs_build/landing html SPHINXOPTS="-W"

5. **Formatting & Workflow Linting:**

   * Run ``clang-format`` on hand-written C++17 headers/sources/tests (excluding generated ``src/ezsnmp_data_logger_wrap.cxx``).
   * Run ``black`` on hand-written Python modules/tests (excluding SWIG-generated ``ezsnmp_data_logger/ezsnmp_data_logger.py``).
   * Validate ``.github/workflows/*.yml`` with ``yamllint -c .yamllint`` and ``actionlint``.

----

Security Considerations
=======================

* Never commit real SNMP community strings, SNMPv3 authentication/privacy passphrases, or ``psql_conninfo`` passwords into ``configs/``, tests, or prompts.
* Review any changes touching ``EzSnmpPoller``, SQLite3/PostgreSQL parameterized statements, or ``StudioHttpServer`` localhost bindings carefully.
