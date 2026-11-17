===================
EzSnmp Data Logger
===================

|Tests C++ Native| |Tests Python + GUI| |SWIG 4 Languages| |Black| |Clang-format| |Sphinx Docs|

|Codecov| |Quality Gate Status| |Security Rating| |Maintainability Rating|

|PyPI Version| |PyPI Downloads| |npm Version| |npm Downloads| |NuGet Version| |PyPI Distributions| |TestPyPI Distributions|

|Python Versions| |License|

.. |Tests C++ Native| image:: https://img.shields.io/github/actions/workflow/status/carlkidcrypto/ezsnmp_data_logger/tests_cpp_native.yml?style=flat-square&label=Tests+C%2B%2B+Native
    :target: https://github.com/carlkidcrypto/ezsnmp_data_logger/actions/workflows/tests_cpp_native.yml
.. |Tests Python + GUI| image:: https://img.shields.io/github/actions/workflow/status/carlkidcrypto/ezsnmp_data_logger/tests_python.yml?style=flat-square&label=Tests+Python+%2B+GUI
    :target: https://github.com/carlkidcrypto/ezsnmp_data_logger/actions/workflows/tests_python.yml
.. |SWIG 4 Languages| image:: https://img.shields.io/github/actions/workflow/status/carlkidcrypto/ezsnmp_data_logger/swig_multilang.yml?style=flat-square&label=SWIG+4+Languages
    :target: https://github.com/carlkidcrypto/ezsnmp_data_logger/actions/workflows/swig_multilang.yml
.. |Black| image:: https://img.shields.io/github/actions/workflow/status/carlkidcrypto/ezsnmp_data_logger/black.yml?style=flat-square&label=Black
    :target: https://github.com/carlkidcrypto/ezsnmp_data_logger/actions/workflows/black.yml
.. |Clang-format| image:: https://img.shields.io/github/actions/workflow/status/carlkidcrypto/ezsnmp_data_logger/clang_format.yml?style=flat-square&label=Clang-format
    :target: https://github.com/carlkidcrypto/ezsnmp_data_logger/actions/workflows/clang_format.yml
.. |Sphinx Docs| image:: https://img.shields.io/github/actions/workflow/status/carlkidcrypto/ezsnmp_data_logger/sphinx_build.yml?style=flat-square&label=Sphinx+Docs
    :target: https://github.com/carlkidcrypto/ezsnmp_data_logger/actions/workflows/sphinx_build.yml
.. |Codecov| image:: https://img.shields.io/codecov/c/github/carlkidcrypto/ezsnmp_data_logger/main?style=flat-square&label=Codecov
    :target: https://codecov.io/gh/carlkidcrypto/ezsnmp_data_logger
.. |Quality Gate Status| image:: https://sonarcloud.io/api/project_badges/measure?project=carlkidcrypto_ezsnmp_data_logger&metric=alert_status
    :target: https://sonarcloud.io/summary/new_code?id=carlkidcrypto_ezsnmp_data_logger
    :alt: Quality Gate Status
.. |Security Rating| image:: https://sonarcloud.io/api/project_badges/measure?project=carlkidcrypto_ezsnmp_data_logger&metric=security_rating
    :target: https://sonarcloud.io/summary/new_code?id=carlkidcrypto_ezsnmp_data_logger
    :alt: Security Rating
.. |Maintainability Rating| image:: https://sonarcloud.io/api/project_badges/measure?project=carlkidcrypto_ezsnmp_data_logger&metric=sqale_rating
    :target: https://sonarcloud.io/summary/new_code?id=carlkidcrypto_ezsnmp_data_logger
    :alt: Maintainability Rating
.. |PyPI Version| image:: https://img.shields.io/pypi/v/ezsnmp-data-logger?style=flat-square
    :target: https://pypi.org/project/ezsnmp-data-logger/
.. |PyPI Downloads| image:: https://img.shields.io/pypi/dm/ezsnmp-data-logger?style=flat-square
    :target: https://pypi.org/project/ezsnmp-data-logger/
.. |npm Version| image:: https://img.shields.io/npm/v/ezsnmp-data-logger?style=flat-square
    :target: https://www.npmjs.com/package/ezsnmp-data-logger
.. |npm Downloads| image:: https://img.shields.io/npm/dm/ezsnmp-data-logger?style=flat-square
    :target: https://www.npmjs.com/package/ezsnmp-data-logger
.. |NuGet Version| image:: https://img.shields.io/nuget/v/EzSnmpDataLogger?style=flat-square
    :target: https://www.nuget.org/packages/EzSnmpDataLogger
.. |PyPI Distributions| image:: https://img.shields.io/github/actions/workflow/status/carlkidcrypto/ezsnmp_data_logger/build_and_publish_to_pypi.yml?style=flat-square&label=PyPI+Distributions
    :target: https://github.com/carlkidcrypto/ezsnmp_data_logger/actions/workflows/build_and_publish_to_pypi.yml
.. |TestPyPI Distributions| image:: https://img.shields.io/github/actions/workflow/status/carlkidcrypto/ezsnmp_data_logger/build_and_publish_to_test_pypi.yml?style=flat-square&label=TestPyPI+Distributions
    :target: https://github.com/carlkidcrypto/ezsnmp_data_logger/actions/workflows/build_and_publish_to_test_pypi.yml
.. |Python Versions| image:: https://img.shields.io/pypi/pyversions/ezsnmp-data-logger?style=flat-square
    :target: https://pypi.org/project/ezsnmp-data-logger/
.. |License| image:: https://img.shields.io/github/license/carlkidcrypto/ezsnmp_data_logger?style=flat-square
    :target: https://github.com/carlkidcrypto/ezsnmp_data_logger/blob/main/LICENSE

Introduction
------------

**EzSnmp Data Logger** (``ezsnmp_data_logger``) is a **self-contained**, portable **C++17** SNMP telemetry collection and storage package built directly on top of `EzSnmp <https://github.com/carlkidcrypto/ezsnmp>`_ (standardized on the **CalSemVer** release stream ``ezsnmp>=2026.11.15b0``).

Every distributed language package—**Python**, **JavaScript (Node.js N-API)**, **C# (.NET 8+)**, and **Go (cgo)**—is built to be **100% self-contained** so end users never have to install extra native libraries (Net-SNMP, OpenSSL, SQLite3, or SWIG).

Key Highlights
--------------

1. **Powered by EzSnmp ("net-snmp" Layer — CalSemVer Standardized):**
   Uses ``ezsnmp``'s C++17 ``SessionBase`` and ``Result`` classes (`v2026.11.15b0`) to execute SNMPv1, SNMPv2c, and SNMPv3 (``noAuthNoPriv``, ``authNoPriv``, ``authPriv``) ``get``, ``getnext``, ``walk``, ``bulkget``, ``bulkwalk``, and ``set`` operations against Devices Under Test (DUTs), normalizing returned bindings into typed telemetry records.
2. **Self-Contained Packages Across All 4 SWIG Target Languages:**
   Statically embeds the public-domain **SQLite3** C amalgamation directly into every binary and bundles/relocates **Net-SNMP 5.10 + OpenSSL** runtime libraries inside **Python** (`cibuildwheel`), **JavaScript / Node.js N-API** (`bindings/javascript/`), **C# / .NET** (`bindings/csharp/`), and **Go** (`bindings/go/`) packages—zero external system libraries required by users!
3. **Multi-Sink Telemetry Persistence:**
   Decouples network polling from storage via a bounded queue and fans out batches to embedded **SQLite3 (WAL)**, **CSV**, **JSONL**, **PostgreSQL / TimescaleDB**, and a built-in **Prometheus HTTP** exporter (``/metrics``).
4. **100% JSON Configuration:**
   Every engine setting, DUT target, ``ezsnmp`` ``SessionBase`` parameter, storage sink, CLI option, and GUI preference lives in RFC-8259 JSON (see ``configs/``).
5. **CLI + Behind-the-Scenes Cross-Platform GUI (Linux / macOS / Windows):**
   Ships both ``ezsnmp-data-logger`` for terminal/automation workflows and two zero-external-dependency cross-platform GUIs (**Native ``tkinter.ttk`` Desktop Studio** and **Browser Web Studio**) that interact strictly with the CLI behind the scenes via subprocess + JSON, complete with a live **CLI Command Audit Log**.

Architecture Overview
---------------------

.. code-block:: text

   +-----------------------------------------------------------------------+
   |  JSON Configuration (configs/*.json)                                  |
   |  logger + duts (ezsnmp SessionBase params) + sinks + gui              |
   +-----------------------------------+-----------------------------------+
                                       |
      +--------------------------------+--------------------------------+
      |                                |                                |
      v                                v                                v
   +------------------+   +-------------------------+   +------------------+
   | Cross-Platform   |   |  ezsnmp-data-logger CLI |   | 4 Self-Contained |
   | GUIs (Tk & Web)  |-->|  init/validate/run/once |   | SWIG Packages:   |
   | Linux/macOS/Win  |   |  probe/request/set/...  |   | Python, Node.js, |
   | (via CLI --json) |   +------------+------------+   | C#, Go           |
   +------------------+                |                +--------+---------+
                                       +------------+------------+
                                                    |
                                                    v
                                  +----------------------------------+
                                  | Self-Contained C++17 Engine      |
                                  |  EzSnmpPoller (ezsnmp::Session)  |
                                  |  -> BoundedQueue<Batch>          |
                                  |  -> Embedded SQLite3 / CSV /     |
                                  |     JSONL / PostgreSQL / Prom    |
                                  +----------------------------------+

Installation
------------

``ezsnmp_data_logger`` distributes **100% self-contained** desktop application bundles, installers, and binary wheels for **macOS**, **Windows**, and **Linux** (both ``manylinux`` and ``musllinux``). Users **never need to install external Net-SNMP, OpenSSL, or SQLite3 libraries**.

Standalone Desktop Applications (No Python Required)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- **macOS Disk Image (.dmg):** Download ``EzSnmpDataLogger-macOS-arm64.dmg`` from `GitHub Releases <https://github.com/carlkidcrypto/ezsnmp_data_logger/releases>`_, mount, and drag **EzSnmpDataLogger.app** to ``/Applications``.
- **Windows Setup Installer (.exe):** Download ``EzSnmpDataLogger-Windows-x64-Setup.exe`` from `GitHub Releases <https://github.com/carlkidcrypto/ezsnmp_data_logger/releases>`_ (WinGet community package distribution via ``winget install carlkidcrypto.EzSnmpDataLogger`` is currently marked as **FUTURE / WIP** pending credentials setup).

- **Windows Portable (.exe / .zip):** Download portable ``EzSnmpDataLogger-Windows-x64.exe`` or ``EzSnmpDataLogger-Windows-x64.zip``.
- **Linux Packages (.deb & .rpm):** Download ``EzSnmpDataLogger-Linux-x86_64.deb`` (Ubuntu/Debian) or ``EzSnmpDataLogger-Linux-x86_64.rpm`` (Fedora/RHEL/openSUSE) from `GitHub Releases <https://github.com/carlkidcrypto/ezsnmp_data_logger/releases>`_, or extract portable ``EzSnmpDataLogger-Linux-x86_64.tar.gz``.

pip Package Installation
~~~~~~~~~~~~~~~~~~~~~~~~

macOS (Apple Silicon & Intel)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   python3 -m pip install --upgrade pip
   python3 -m pip install --pre "ezsnmp-data-logger>=2026.11.9"

   # Install native macOS Application Launcher in ~/Applications with Retina icon:
   ezsnmp-data-logger-gui --install-launcher

Windows (Windows 10 / 11 / Server)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: powershell

   py -m pip install --upgrade pip
   py -m pip install --pre "ezsnmp-data-logger>=2026.11.9"

   # Install Windows desktop launcher shortcut with custom icon:
   ezsnmp-data-logger-gui --install-launcher

Linux (Ubuntu, Debian, Fedora, RHEL, Arch, Alpine)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   python3 -m pip install --upgrade pip
   python3 -m pip install --pre "ezsnmp-data-logger>=2026.11.9"

   # Install desktop launcher (.desktop) in ~/.local/share/applications/:
   ezsnmp-data-logger-gui --install-launcher

CLI Usage Reference
-------------------

The ``ezsnmp-data-logger`` CLI provides unified access to the C++17 telemetry engine. Passing ``--json`` outputs structured JSON for scripting and automation.

.. code-block:: bash

   # 1. Generate starter JSON configuration template ('demo', 'minimal', or 'all_sinks')
   ezsnmp-data-logger init-config --template demo --output configs/my_lab.json

   # 2. Validate configuration syntax, schema, and SNMP credentials
   ezsnmp-data-logger validate --config configs/my_lab.json --json

   # 3. Probe all configured DUTs for network reachability (non-destructive)
   ezsnmp-data-logger probe --config configs/my_lab.json --json

   # 4. Run an on-demand SNMP GET / WALK / BULKWALK query
   ezsnmp-data-logger request --config configs/my_lab.json \
       --dut edge-router-01 --op walk --oid 1.3.6.1.2.1.1 --json

   # 5. Run an on-demand SNMP SET operation (ASN.1 types: i, u, s, x, d, n, o, t, a)
   ezsnmp-data-logger set --config configs/my_lab.json \
       --dut edge-router-01 --oid 1.3.6.1.2.1.1.4.0 --type s --value "admin@example.com" --json

   # 6. Run a single polling cycle across all DUTs and exit
   ezsnmp-data-logger run --config configs/my_lab.json --once --json

   # 7. Run continuous polling loop with streaming NDJSON event output
   ezsnmp-data-logger run --config configs/my_lab.json --interval-ms 1000 --json

   # 8. Query recent stored telemetry rows from SQLite3, CSV, or JSONL sinks
   ezsnmp-data-logger inspect --config configs/my_lab.json --limit 25 --json

   # 9. Inspect compiled capabilities and version
   ezsnmp-data-logger version --json

Cross-Platform GUI Usage (Desktop & Web Studio)
-----------------------------------------------

``ezsnmp_data_logger`` includes two zero-external-dependency GUI frontends that interact strictly with the CLI behind the scenes:

- **Native Desktop Studio (**``tkinter.ttk``**):** Native Aqua (macOS), Win32 (Windows), and X11/Wayland (Linux) application with custom window titlebar icons, dynamic macOS Dock icon, real-time Canvas charts, and **interactive selectable series filtering (Select All, Clear All, and auto-rescaling Y-axis)**.
- **Browser Web Studio (**``HTML5/SVG``**):** Lightweight browser-based studio running on standard-library HTTP server, featuring custom vector SVG favicons and responsive series filter chips.

.. code-block:: bash

   # Auto-detects display: launches native Desktop Tkinter window or Browser Studio
   ezsnmp-data-logger gui --config configs/example.json

   # Force Native Desktop Tkinter Studio
   ezsnmp-data-logger-gui --config configs/example.json --frontend tk

   # Force Browser Web Studio on http://127.0.0.1:8765
   ezsnmp-data-logger-gui --config configs/example.json --frontend web --port 8765

   # Run on a headless remote server (bind to all interfaces without popping browser)
   ezsnmp-data-logger-gui --config configs/example.json --frontend web --host 0.0.0.0 --port 8765 --no-browser

   # Switch language (e.g. Spanish 'es' or custom locale JSON)
   ezsnmp-data-logger-gui --config configs/example.json --locale es

Generate SWIG Bindings (Python, Node.js, C#, Go)
------------------------------------------------

.. code-block:: bash

   python3 tools/generate_swig_bindings.py --out-dir build/swig_generated

How to Support This Project?
----------------------------

.. image:: https://raw.githubusercontent.com/carlkidcrypto/ezsnmp/main/images/buy_me_a_coffee.png
    :alt: Buy Me A Coffee

`Use this link to buy me a coffee! <https://www.buymeacoffee.com/carlkidcrypto>`__

Documentation & Contributing
----------------------------

* **Full Documentation:** `EzSnmp Data Logger Docs <https://carlkidcrypto.github.io/ezsnmp_data_logger/>`_
* **Software Requirements:** ``sphinx_docs_build/source/software_requirements.rst``
* **AI Contribution Guide:** Please read `HOWTOAI.rst <HOWTOAI.rst>`_ before contributing with AI coding assistants.

License
-------

EzSnmp Data Logger is released under the **MIT** license. Please see `LICENSE <LICENSE>`_ for details.
