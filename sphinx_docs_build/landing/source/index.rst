.. meta::
   :description: EzSnmp Data Logger is a C++17 SNMP telemetry engine powered by ezsnmp with SWIG multi-language bindings, CLI, and cross-platform Linux/macOS/Windows GUI.
   :keywords: ezsnmp, snmp data logger, net-snmp, c++17, swig, snmpv3, sqlite3, timescaledb, prometheus, cross-platform gui, carlkidcrypto
   :author: Carlos (@carlkidcrypto)
   :property=og:title: EzSnmp Data Logger — C++17 Multi-Language SNMP Telemetry Engine
   :property=og:description: High-throughput C++17 SNMP data logger powered by ezsnmp with SWIG bindings, JSON configuration, CLI, and cross-platform GUI.
   :property=og:type: website
   :property=og:url: https://carlkidcrypto.github.io/ezsnmp_data_logger/

EzSnmp Data Logger Documentation
================================

**EzSnmp Data Logger** is a high-throughput C++17 SNMP telemetry collection and storage package built on top of `EzSnmp <https://github.com/carlkidcrypto/ezsnmp>`_. It delegates SNMPv1, SNMPv2c, and SNMPv3 ``get``, ``getnext``, ``walk``, ``bulkget``, ``bulkwalk``, and ``set`` operations to ``ezsnmp``'s ``SessionBase`` layer, stores returned telemetry across SQLite3 (WAL), CSV, JSONL, PostgreSQL/TimescaleDB, and Prometheus HTTP sinks, and exposes universal **SWIG 4.x** bindings across 15+ target languages alongside a **CLI** and a **cross-platform GUI** (Linux, macOS, and Windows).

Key Features
------------

* **C++17 Core + EzSnmp Layer:** Uses ``ezsnmp`` ``SessionBase`` and ``Result`` for thread-safe SNMPv1/v2c/v3 requests and SET operations, decoupled from storage via a bounded queue.
* **Multi-Language SWIG 4.x Distribution:** Single SWIG interface suite generating bindings for Python, JavaScript (Node.js N-API), Java, C#, Go, Ruby, Perl 5, PHP, Lua, R, Tcl 8, Octave, D, Guile, and Scilab.
* **100% JSON Configuration:** Every engine, DUT, SNMPv3 USM, storage sink, and GUI setting lives in a validated JSON document.
* **CLI + Behind-the-Scenes Cross-Platform GUI:** Full-featured CLI plus native ``tkinter.ttk`` Desktop Studio and Browser Web Studio across Linux, macOS, and Windows that drive the CLI behind the scenes.

Select a Version
----------------

Select a version of the documentation to browse:

.. VERSION_LIST_START

* `Latest Alpha/Beta Version <./html/index.html>`_

.. VERSION_LIST_END

Project Links
-------------

* **Source Code:** `GitHub Repository <https://github.com/carlkidcrypto/ezsnmp_data_logger>`_
* **EzSnmp Engine:** `carlkidcrypto/ezsnmp <https://github.com/carlkidcrypto/ezsnmp>`_
* **Maintainer:** `Carlos (@carlkidcrypto) <https://carlkidcrypto.github.io/>`_
