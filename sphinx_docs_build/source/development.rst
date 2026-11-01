=================
Development Guide
=================

This guide covers building the C++17 engine, running the GoogleTest and ``pytest`` suites, generating multi-language SWIG bindings, and formatting code for **EzSnmp Data Logger**.

Prerequisites
=============

* **C++17 Compiler:** GCC 9+, Clang 11+, Apple Clang 13+, or MSVC 2019+
* **Net-SNMP & OpenSSL:** Required by the ``ezsnmp`` C++17 ``SessionBase`` layer
* **SQLite3:** Required for the embedded SQLite3 WAL sink (``libpq`` optional for PostgreSQL/TimescaleDB)
* **SWIG 4.x:** Required to generate target-language bindings
* **Build Tools:** Meson (>= 1.1) + Ninja, or CMake (>= 3.16), plus Python 3.9+

Installing System Dependencies
------------------------------

**macOS (Homebrew):**

.. code-block:: bash

   brew install net-snmp openssl@3 sqlite swig meson ninja cmake doxygen

**Ubuntu / Debian:**

.. code-block:: bash

   sudo apt-get update
   sudo apt-get install -y build-essential libsnmp-dev libssl-dev libsqlite3-dev \
       libpq-dev swig meson ninja-build cmake libgtest-dev doxygen

Building & Testing C++17 Core (Meson)
=====================================

Meson automatically discovers a sibling ``../ezsnmp`` checkout (or resolves ``subprojects/ezsnmp.wrap``), compiles ``libezsnmp_data_logger``, the native ``ezsnmp-data-logger`` CLI binary, and all 5 GoogleTest suites:

.. code-block:: bash

   meson setup build
   meson compile -C build
   meson test -C build -v

Building & Testing C++17 Core (CMake)
=====================================

.. code-block:: bash

   cmake -B build-cmake -DEZSNMP_SOURCE_DIR=../ezsnmp/ezsnmp
   cmake --build build-cmake -j

Building Python Package, CLI & Cross-Platform GUI
=================================================

.. code-block:: bash

   python3 -m pip install -e .
   pytest -v

Generating All 16 SWIG Language Targets
=======================================

.. code-block:: bash

   python3 tools/generate_swig_bindings.py --out-dir generated_swig

Code Formatting & Workflow Linting
==================================

* **C++17 Formatting:** ``clang-format -i include/ezsnmp_data_logger/*.h src/*.cpp cpp_tests/*.h cpp_tests/*.cpp`` (never format generated ``src/ezsnmp_data_logger_wrap.cxx``)
* **Python Formatting:** ``black --line-length 100 setup.py tools/ ezsnmp_data_logger/ python_tests/`` (excluding SWIG-generated ``ezsnmp_data_logger/ezsnmp_data_logger.py``)
* **GitHub Actions Workflows:** ``yamllint -c .yamllint .github/workflows/*.yml`` and ``actionlint``

Software Requirements Governance
================================

Every architectural feature or behavior change must trace to a requirement in :doc:`software_requirements`. Update ``sphinx_docs_build/source/software_requirements.rst`` first before modifying implementation code.
