.. meta::
   :description: Documentation for EzSnmp Data Logger — self-contained C++17 SNMP telemetry engine powered by ezsnmp with SWIG bindings (Python, Node.js, C#, Go), CLI, and cross-platform GUI.
   :keywords: ezsnmp_data_logger, ezsnmp, snmp, net-snmp, c++17, swig, calsemver, cli, gui, json, sqlite3, prometheus

========================================
Welcome to EzSnmp Data Logger's Docs!
========================================

**EzSnmp Data Logger** (``ezsnmp_data_logger``) is a self-contained, portable **C++17** SNMP telemetry engine built on top of `EzSnmp <https://github.com/carlkidcrypto/ezsnmp>`_ (`v2026.10.28a0` CalSemVer alpha baseline).

1. **EzSnmp Network Layer:** Delegates all SNMPv1, SNMPv2c, and SNMPv3 ``get``, ``getnext``, ``walk``, ``bulkget``, ``bulkwalk``, and ``set`` requests to ``ezsnmp``'s C++17 ``SessionBase`` and ``Result`` classes.
2. **Multi-Sink Storage Engine:** Decouples network polling from disk/database I/O with a bounded MPMC queue and persists telemetry to **SQLite3 (WAL)**, **CSV**, **JSONL**, **PostgreSQL / TimescaleDB**, and **Prometheus HTTP** (``/metrics``).
3. **100% Self-Contained Packages + CalSemVer Alpha:** Embeds the public-domain SQLite3 C amalgamation directly into every binary and bundles/relocates Net-SNMP 5.10 + OpenSSL runtime libraries so end users never install external system libraries. Standardized on **CalSemVer** alpha releases (``2026.11.6``) alongside ``ezsnmp>=2026.10.28a0``.
4. **SWIG 4.x Distribution (4 Target Languages):** Exposes the C++17 API across **Python**, **JavaScript (Node.js N-API)**, **C# (.NET)**, and **Go (cgo)**.
5. **CLI + Behind-the-Scenes Cross-Platform GUI:** Ships both ``ezsnmp-data-logger`` CLI and two cross-platform GUIs for **Linux, macOS, and Windows** (Native ``tkinter.ttk`` Desktop Studio and Browser Web Studio) that drive the CLI behind the scenes via JSON.

.. toctree::
   :maxdepth: 2
   :caption: User & Architecture Guides:

   installation
   architecture
   configuration
   cli_guide
   gui_guide
   swig_languages

.. toctree::
   :maxdepth: 2
   :caption: API & Reference:

   cpp_api
   swig_interface_files
   python_api
   modules

.. toctree::
   :maxdepth: 2
   :caption: Engineering & Governance:

   software_requirements
   development
   documentation_guide

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
