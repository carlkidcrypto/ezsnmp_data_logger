.. meta::
   :description: EzSnmp Data Logger is a self-contained C++17 SNMP telemetry engine powered by ezsnmp with SWIG bindings (Python, Node.js, C#, Go), CLI, and cross-platform Linux/macOS/Windows GUI.
   :keywords: ezsnmp, snmp data logger, net-snmp, c++17, swig, calsemver, snmpv3, sqlite3, timescaledb, prometheus, cross-platform gui, carlkidcrypto
   :author: Carlos (@carlkidcrypto)
   :property=og:title: EzSnmp Data Logger — Self-Contained C++17 SNMP Telemetry Engine
   :property=og:description: Self-contained C++17 SNMP data logger powered by ezsnmp with SWIG bindings for Python, Node.js, C#, and Go, JSON configuration, CLI, and cross-platform GUI.
   :property=og:type: website
   :property=og:url: https://carlkidcrypto.github.io/ezsnmp_data_logger/

EzSnmp Data Logger Documentation
================================

**EzSnmp Data Logger** is a self-contained, high-throughput C++17 SNMP telemetry collection and storage package built on top of `EzSnmp <https://github.com/carlkidcrypto/ezsnmp>`_. It delegates SNMPv1, SNMPv2c, and SNMPv3 ``get``, ``getnext``, ``walk``, ``bulkget``, ``bulkwalk``, and ``set`` operations to ``ezsnmp``'s ``SessionBase`` layer (`v2026.10.28a0` CalSemVer alpha baseline), stores returned telemetry across embedded SQLite3 (WAL), CSV, JSONL, PostgreSQL/TimescaleDB, and Prometheus HTTP sinks, and ships self-contained **SWIG 4.x** packages for **Python, JavaScript (Node.js N-API), C#, and Go** alongside a **CLI** and **cross-platform GUI** (Linux, macOS, and Windows).

Key Features
------------

* **C++17 Core + EzSnmp Layer:** Uses ``ezsnmp`` ``SessionBase`` and ``Result`` for thread-safe SNMPv1/v2c/v3 requests and SET operations, decoupled from storage via a bounded queue.
* **Self-Contained Packages (Zero Extra Libraries):** Statically embeds the SQLite3 C amalgamation and bundles Net-SNMP 5.10 + OpenSSL runtime libraries inside each language package, standardized on **CalSemVer** alpha releases (``2026.11.3``).
* **4 SWIG 4.x Target Languages:** Python, JavaScript (Node.js N-API), C# (.NET 8+), and Go (cgo).
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
