================================================
Self-Contained SWIG Multi-Language Distribution
================================================

**EzSnmp Data Logger** distributes its **C++17** backend (powered by ``ezsnmp``'s ``SessionBase`` and ``Result`` layers on the ``v2026.11.4a0`` CalSemVer alpha line) as **self-contained packages** across **4 SWIG 4.x target languages** so end users never need to install external system libraries (Net-SNMP, OpenSSL, SQLite3, or SWIG).

Supported SWIG Target Languages
===============================

.. list-table::
   :header-rows: 1
   :widths: 22 28 50

   * - Language
     - SWIG Invocation
     - Self-Contained Packaging Mechanism
   * - **Python**
     - ``swig -c++ -python``
     - ``pyproject.toml`` + ``cibuildwheel`` (``auditwheel`` on Linux, ``delocate-wheel`` on macOS, ``delvewheel`` on Windows) + embedded SQLite3 amalgamation
   * - **JavaScript (Node.js N-API)**
     - ``swig -c++ -javascript -napi``
     - ``bindings/javascript/`` (``node-gyp`` compiling C++17 core + embedded SQLite3 + ``bundle_native_package.py`` relocating ``libnetsnmp`` / ``libcrypto``)
   * - **C# (.NET 8+)**
     - ``swig -c++ -csharp -namespace EzSnmpDataLogger``
     - ``bindings/csharp/`` (``EzSnmpDataLogger.csproj`` + ``build_self_contained.py`` populating NuGet ``runtimes/<rid>/native/``)
   * - **Go (cgo)**
     - ``swig -c++ -go -cgo -intgosize 64``
     - ``bindings/go/`` (``go.mod`` + ``build_self_contained.py`` staging bundled native runtime libraries in ``bindings/go/lib/``)

How Self-Containment Works
==========================

1. **Embedded SQLite3 Amalgamation (``tools/prepare_sqlite3_amalgamation.py``):**
   Downloads and SHA-256-verifies the official public-domain SQLite3 C amalgamation (`3.49.1`) into ``third_party/sqlite3/`` and compiles ``sqlite3.c`` statically into every binary (`EZSNMP_DL_EMBEDDED_SQLITE3=1`), eliminating ``libsqlite3`` shared-library dependencies on Linux, macOS, and Windows.
2. **Standalone Net-SNMP 5.10 + OpenSSL + EzSnmp ``v2026.11.4a0``:**
   ``tools/prepare_ezsnmp_source.py`` stages ``ezsnmp`` at CalSemVer alpha tag ``v2026.11.4a0``, while ``tools/prepare_{linux,macos,windows}_netsnmp.*`` builds minimal SHA-256-verified Net-SNMP ``5.10.pre2`` libraries.
3. **Dynamic Loader Relocation (``tools/bundle_native_package.py``):**
   Copies all non-OS runtime shared libraries into each package's runtime folder, rewrites RPATH/install-name entries (``$ORIGIN`` via ``patchelf`` on Linux, ``@loader_path`` via ``install_name_tool`` + ``codesign`` on macOS, side-by-side DLLs on Windows), and verifies that zero external non-OS dependencies remain.

Generating Wrappers
===================

.. code-block:: bash

   python3 tools/generate_swig_bindings.py --languages all --out-dir build/swig_generated

Cross-Language Idiomatic Usage
==============================

Python (CalSemVer Alpha)
------------------------

.. code-block:: bash

   pip install --pre "ezsnmp-data-logger>=2026.11.7"

.. code-block:: python

   import ezsnmp_data_logger as edl

   dl = edl.DataLogger.from_file("configs/example.json")
   batches = dl.run_once()
   for batch in batches:
       print(batch.dut_name, len(batch.samples), batch.to_json())

JavaScript / Node.js (N-API)
----------------------------

.. code-block:: javascript

   const edl = require('ezsnmp-data-logger');

   const dl = edl.DataLogger.from_file('configs/example.json');
   const batches = dl.run_once();
   console.log(dl.stats_json(2));

C# (.NET 8+)
------------

.. code-block:: csharp

   using System;
   using EzSnmpDataLogger;

   using var dl = DataLogger.from_file("configs/example.json");
   var batches = dl.run_once();
   Console.WriteLine(dl.stats_json(2));

Go (cgo)
--------

.. code-block:: go

   package main

   import (
       "fmt"
       edl "github.com/carlkidcrypto/ezsnmp_data_logger/bindings/go"
   )

   func main() {
       dl := edl.DataLoggerFrom_file("configs/example.json")
       defer edl.DeleteDataLogger(dl)
       dl.Run_once()
       fmt.Println(dl.Stats_json(2))
   }
