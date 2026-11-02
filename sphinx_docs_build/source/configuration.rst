==========================
JSON Configuration Guide
==========================

Every setting in ``ezsnmp_data_logger`` — engine scheduling, ``ezsnmp`` SNMPv1/v2c/v3 credentials, MIB formatting flags, storage sinks, and cross-platform GUI preferences — is stored in a single JSON configuration document.

Generating Starter Templates & JSON Schema
==========================================

Use the CLI to generate ready-to-edit JSON files or inspect the Draft 2020-12 JSON Schema:

.. code-block:: bash

   ezsnmp-data-logger init-config --template minimal --output my_config.json
   ezsnmp-data-logger init-config --template demo --output demo.json
   ezsnmp-data-logger init-config --template snmpv3 --output snmpv3.json
   ezsnmp-data-logger init-config --template enterprise --output enterprise.json
   ezsnmp-data-logger schema > ezsnmp_data_logger.schema.json
   ezsnmp-data-logger validate --config my_config.json

Top-Level JSON Structure
========================

A configuration document contains four top-level JSON objects/arrays:

1. ``logger`` — Polling cadence (``poll_interval_ms``), bounded queue capacity (``queue_depth``), ``worker_threads``, ``stats_interval_ms``, optional ``max_cycles`` / ``run_duration_seconds``, and optional atomic ``status_file``.
2. ``duts`` — Array of Devices Under Test, each with ``name``, ``dut_ip``, ``enabled``, optional per-DUT ``poll_interval_ms``, an ``snmp`` object mapping all 24 ``ezsnmp::SessionBase`` parameters (``mode`` ``"ezsnmp"`` or ``"stub"``, ``version`` ``"1"``/``"2c"``/``"3"``, ``community``, ``port_number``, ``timeout_seconds``, ``retries``, SNMPv3 USM ``security_level``, ``security_username``, ``auth_protocol``, ``auth_passphrase``, ``privacy_protocol``, ``privacy_passphrase``, ``context``, ``load_mibs``, ``mib_directories``, ``print_oids_numerically``, etc.), and a ``metrics`` array.
3. ``sinks`` — Array of output sinks (``sqlite3``, ``csv``, ``jsonl``, ``psql``, ``prometheus``).
4. ``gui`` — Cross-platform GUI settings stored directly in JSON (``frontend``: ``"auto"`` | ``"tk"`` | ``"web"``, ``theme``: ``"dark"`` | ``"light"``, ``locale``: ``"en"`` | ``"es"``, optional ``locale_file``, ``window_width``, ``window_height``, ``refresh_interval_ms``, ``host``, ``port``, ``open_browser``, ``cli_executable``, ``max_log_lines``).
