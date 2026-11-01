===================
EzSnmp Data Logger
===================

.. list-table::
   :widths: auto

   * - |Python Code Style|
     - |Clang-format Code Style|
     - |Black|
   * - |Clang-format|
     - |Sphinx Docs Check|
     - |Tests C++ Native|
   * - |Tests Python|
     - |SWIG Multi-Language|
     - |License|

.. |Python Code Style| image:: https://img.shields.io/badge/code%20style-black-000000.svg?style=flat-square
    :target: https://github.com/psf/black
.. |Clang-format Code Style| image:: https://img.shields.io/badge/code%20style-clang--format-brightgreen.svg?style=flat-square
    :target: https://clang.llvm.org/docs/ClangFormat.html
.. |Black| image:: https://img.shields.io/github/actions/workflow/status/carlkidcrypto/ezsnmp_data_logger/black.yml?style=flat-square&label=Black
    :target: https://github.com/carlkidcrypto/ezsnmp_data_logger/actions/workflows/black.yml
.. |Clang-format| image:: https://img.shields.io/github/actions/workflow/status/carlkidcrypto/ezsnmp_data_logger/clang_format.yml?style=flat-square&label=Clang-format
    :target: https://github.com/carlkidcrypto/ezsnmp_data_logger/actions/workflows/clang_format.yml
.. |Sphinx Docs Check| image:: https://img.shields.io/github/actions/workflow/status/carlkidcrypto/ezsnmp_data_logger/sphinx_build.yml?style=flat-square&label=Sphinx+Docs+Check
    :target: https://github.com/carlkidcrypto/ezsnmp_data_logger/actions/workflows/sphinx_build.yml
.. |Tests C++ Native| image:: https://img.shields.io/github/actions/workflow/status/carlkidcrypto/ezsnmp_data_logger/tests_cpp_native.yml?style=flat-square&label=Tests+C%2B%2B+Native
    :target: https://github.com/carlkidcrypto/ezsnmp_data_logger/actions/workflows/tests_cpp_native.yml
.. |Tests Python| image:: https://img.shields.io/github/actions/workflow/status/carlkidcrypto/ezsnmp_data_logger/tests_python.yml?style=flat-square&label=Tests+Python+%2B+GUI
    :target: https://github.com/carlkidcrypto/ezsnmp_data_logger/actions/workflows/tests_python.yml
.. |SWIG Multi-Language| image:: https://img.shields.io/github/actions/workflow/status/carlkidcrypto/ezsnmp_data_logger/swig_multilang.yml?style=flat-square&label=SWIG+16+Languages
    :target: https://github.com/carlkidcrypto/ezsnmp_data_logger/actions/workflows/swig_multilang.yml
.. |License| image:: https://img.shields.io/badge/license-MIT-blue.svg?style=flat-square
    :target: https://github.com/carlkidcrypto/ezsnmp_data_logger/blob/main/LICENSE

Introduction
------------

**EzSnmp Data Logger** (``ezsnmp_data_logger``) is a portable **C++17** SNMP telemetry collection and storage package built directly on top of `EzSnmp <https://github.com/carlkidcrypto/ezsnmp>`_.

It combines ``ezsnmp``'s Net-SNMP ``SessionBase`` / ``Result`` network layer with a bounded C++17 queue, multi-sink persistence, universal **SWIG 4.x** bindings across **16 target languages**, a structured **CLI**, and **cross-platform GUIs** for **Linux, macOS, and Windows**.

Key Highlights
--------------

1. **Powered by EzSnmp ("net-snmp" Layer):**
   Uses ``ezsnmp``'s C++17 ``SessionBase`` and ``Result`` classes to execute SNMPv1, SNMPv2c, and SNMPv3 (``noAuthNoPriv``, ``authNoPriv``, ``authPriv``) ``get``, ``getnext``, ``walk``, ``bulkget``, ``bulkwalk``, and ``set`` operations against Devices Under Test (DUTs), normalizing returned bindings into typed telemetry records.
2. **Multi-Sink Telemetry Persistence:**
   Decouples network polling from storage via a bounded queue and fans out batches to **SQLite3 (WAL)**, **CSV**, **JSONL**, **PostgreSQL / TimescaleDB**, and a built-in **Prometheus HTTP** exporter (``/metrics``).
3. **100% JSON Configuration:**
   Every engine setting, DUT target, ``ezsnmp`` ``SessionBase`` parameter, storage sink, CLI option, and GUI preference lives in RFC-8259 JSON (see ``configs/``).
4. **Universal SWIG 4.x Multi-Language Distribution:**
   Wraps the C++17 core for **Python**, **JavaScript (Node.js N-API)**, **Java**, **C#**, **Go**, **Ruby**, **Perl 5**, **PHP**, **Lua**, **R**, **Tcl 8**, **GNU Octave**, **D**, **GNU Guile**, **Scilab**, and **XML AST** inspection via ``tools/generate_swig_bindings.py`` and ``bindings/``.
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
   | Cross-Platform   |   |  ezsnmp-data-logger CLI |   | 16 SWIG Targets  |
   | GUIs (Tk & Web)  |-->|  init/validate/run/once |   | Python, Node.js, |
   | Linux/macOS/Win  |   |  probe/request/set/...  |   | Java, C#, Go,    |
   | (via CLI --json) |   +------------+------------+   | Ruby, Perl5, R.. |
   +------------------+                |                +--------+---------+
                                       +------------+------------+
                                                    |
                                                    v
                                  +----------------------------------+
                                  | C++17 DataLogger Engine          |
                                  |  EzSnmpPoller (ezsnmp::Session)  |
                                  |  -> BoundedQueue<Batch>          |
                                  |  -> SQLite3 / CSV / JSONL /      |
                                  |     PostgreSQL / Prometheus HTTP |
                                  +----------------------------------+

Quick Start
-----------

Install Python Package + CLI + Cross-Platform GUI
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   python3 -m pip install -e .

CLI Usage
~~~~~~~~~

.. code-block:: bash

   # Generate a starter JSON configuration
   ezsnmp-data-logger init --template minimal --output my_logger.json

   # Validate the JSON configuration
   ezsnmp-data-logger validate --config my_logger.json --json

   # Run a single polling cycle across all configured DUTs
   ezsnmp-data-logger run-once --config configs/minimal_stub.json --json

   # Run continuous polling (streams JSON events when --json is passed)
   ezsnmp-data-logger run --config configs/example.json --max-cycles 10 --json

   # On-demand SNMP GET / WALK / SET via ezsnmp SessionBase
   ezsnmp-data-logger probe --config configs/example.json --json
   ezsnmp-data-logger request --config configs/example.json --dut edge-router-1 --op walk --oid 1.3.6.1.2.1.1 --json
   ezsnmp-data-logger set --config configs/example.json --dut edge-router-1 --oid 1.3.6.1.2.1.1.6.0 --type s --value "Lab Rack 4" --json

   # Inspect stored telemetry across SQLite3 / JSONL / CSV sinks
   ezsnmp-data-logger inspect-sink --config configs/minimal_stub.json --limit 25 --json

Cross-Platform GUI Usage (Linux, macOS, Windows)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Launch native cross-platform tkinter.ttk Desktop Studio (falls back to Browser Studio headless)
   ezsnmp-data-logger-gui --config configs/example.json

   # Or launch the cross-platform Browser Studio explicitly
   ezsnmp-data-logger gui --config configs/example.json --frontend web --port 8765

Build Native C++17 Engine & Tests (Meson)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   meson setup build
   meson compile -C build
   meson test -C build -v

Generate All 16 SWIG Language Bindings
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   python3 tools/generate_swig_bindings.py --out-dir generated_swig

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
