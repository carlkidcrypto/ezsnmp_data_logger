======================================================
Self-Contained Multi-Language SWIG Bindings & Packages
======================================================

``ezsnmp_data_logger`` distributes its **C++17** SNMP telemetry engine, ``ezsnmp`` ``SessionBase`` network layer (`v2026.11.4a0` CalSemVer alpha baseline), embedded **SQLite3** C amalgamation, JSON configuration validator, and CLI runner as **self-contained packages** across **4 SWIG 4.x target languages**:

1. **Python** (``-python``, self-contained PyPI wheels built via ``cibuildwheel`` + ``auditwheel`` / ``delocate-wheel`` / ``delvewheel``)
2. **JavaScript / Node.js N-API** (``-javascript -napi``, self-contained addon in ``bindings/javascript/``)
3. **C# / .NET 8+** (``-csharp``, self-contained NuGet package with ``runtimes/<rid>/native/`` in ``bindings/csharp/``)
4. **Go** (``-go -cgo -intgosize 64``, self-contained ``cgo`` library bundle in ``bindings/go/``)

Why Self-Contained?
===================

End users installing any ``ezsnmp_data_logger`` package **never** have to manually install Net-SNMP, OpenSSL, SQLite3, or SWIG:

* **SQLite3** (`3.49.1` public-domain amalgamation) is provisioned via ``tools/prepare_sqlite3_amalgamation.py`` and compiled statically directly into every binary (`EZSNMP_DL_EMBEDDED_SQLITE3=1`).
* **Net-SNMP 5.10 + OpenSSL + EzSnmp (`v2026.11.4a0`)** are provisioned via ``tools/prepare_ezsnmp_source.py`` and ``tools/prepare_{linux,macos,windows}_netsnmp.*``, then bundled into each language package and dynamic-loader-relocated (`$ORIGIN` on Linux, `@loader_path` on macOS, side-by-side DLLs on Windows) by ``tools/bundle_native_package.py`` (or ``cibuildwheel`` for Python wheels).

Generating All 4 SWIG Target Wrappers
=====================================

.. code-block:: bash

   python3 tools/generate_swig_bindings.py --languages all --out-dir build/swig_generated
