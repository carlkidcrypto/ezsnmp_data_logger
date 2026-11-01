.. meta::
   :description: Documentation for EzSnmp Data Logger — C++17 SNMP telemetry engine powered by ezsnmp with SWIG multi-language bindings, CLI, and cross-platform GUI.
   :keywords: ezsnmp_data_logger, ezsnmp, snmp, net-snmp, c++17, swig, cli, gui, json, sqlite3, prometheus

========================================
Welcome to EzSnmp Data Logger's Docs!
========================================

**EzSnmp Data Logger** (``ezsnmp_data_logger``) is a portable **C++17** SNMP telemetry engine built on top of `EzSnmp <https://github.com/carlkidcrypto/ezsnmp>`_.

1. **EzSnmp Network Layer:** Delegates all SNMPv1, SNMPv2c, and SNMPv3 ``get``, ``getnext``, ``walk``, ``bulkget``, ``bulkwalk``, and ``set`` requests to ``ezsnmp``'s C++17 ``SessionBase`` and ``Result`` classes.
2. **Multi-Sink Storage Engine:** Decouples network polling from disk/database I/O with a bounded MPMC queue and persists telemetry to **SQLite3 (WAL)**, **CSV**, **JSONL**, **PostgreSQL / TimescaleDB**, and **Prometheus HTTP** (``/metrics``).
3. **100% JSON Configuration:** Every engine, DUT, SNMPv3 USM, storage sink, and GUI preference lives in a single validated JSON configuration file.
4. **Universal SWIG 4.x Distribution:** Wraps the C++17 API across **15+ SWIG target languages** (Python, JavaScript/Node.js N-API, Java, C#, Go, Ruby, Perl 5, PHP, Lua, R, Tcl 8, Octave, D, Guile, and Scilab).
5. **CLI + Behind-the-Scenes Cross-Platform GUI:** Ships both a rich ``ezsnmp-data-logger`` CLI and two cross-platform GUIs for **Linux, macOS, and Windows** (Native ``tkinter.ttk`` Desktop Studio and Browser Web Studio) that drive the CLI behind the scenes via JSON.

.. toctree::
   :maxdepth: 2
   :caption: User & Architecture Guides:

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
