=========
CLI Guide
=========

The ``ezsnmp-data-logger`` command-line interface provides complete control over configuration, validation, network probing, live polling, ad-hoc SNMP queries, and telemetry inspection.

Every subcommand supports ``--json``, returning structured JSON to ``stdout`` with deterministic schemas. This JSON contract is the exact foundation used by the Graphical User Interfaces (Desktop Tkinter and Browser Studio) to drive operations behind the scenes.

.. contents:: Table of Contents
   :local:
   :depth: 2

Global CLI Synopsis
===================

.. code-block:: bash

   ezsnmp-data-logger <subcommand> [options]

Run ``ezsnmp-data-logger --help`` to see all available subcommands, or ``ezsnmp-data-logger <subcommand> --help`` for subcommand-specific flags.

Subcommand Reference
====================

1. ``init-config`` — Generate Starter Configuration
---------------------------------------------------
Generates a valid, commented JSON configuration template.

.. code-block:: bash

   # Generate default demo configuration (2 DUTs, SQLite3 + JSONL + Prometheus sinks)
   ezsnmp-data-logger init-config --template demo --output configs/my_lab.json

   # Generate minimal stub configuration for lightweight local testing
   ezsnmp-data-logger init-config --template minimal --output configs/minimal.json

   # Generate configuration with all storage sinks enabled
   ezsnmp-data-logger init-config --template all_sinks --output configs/production.json

Options:
- ``-t, --template <name>``: Template identifier (``demo``, ``minimal``, ``all_sinks``). Default is ``demo``.
- ``-o, --output <path>``: Output file path. If omitted, writes to ``stdout``.
- ``--json``: Outputs structured JSON status with the generated content.

2. ``validate`` — Verify Configuration Syntax & Semantics
---------------------------------------------------------
Validates a JSON configuration file against the C++ JSON schema and verifies SNMP protocol semantics (community strings for v1/v2c, user/auth/priv keys for v3, valid sink paths).

.. code-block:: bash

   ezsnmp-data-logger validate --config configs/my_lab.json --json

JSON Output Example:

.. code-block:: json

   {
     "ok": true,
     "valid": true,
     "enabled_duts": 2,
     "total_metrics": 6,
     "enabled_sinks": 3,
     "sink_types": ["sqlite3", "jsonl", "prometheus"],
     "errors": []
   }

If validation fails, the command exits with non-zero exit code and reports the exact line/key and error description.

3. ``probe`` — Test DUT Network Reachability
--------------------------------------------
Performs a one-time network reachability check against all enabled Devices Under Test (DUTs) defined in the configuration, executing an initial query without writing to persistent storage sinks.

.. code-block:: bash

   ezsnmp-data-logger probe --config configs/my_lab.json --json

JSON Output Example:

.. code-block:: json

   {
     "ok": true,
     "results": [
       {
         "dut_name": "edge-router-01",
         "dut_ip": "192.168.1.1",
         "latency_ms": 14.2,
         "sample_count": 3,
         "samples": [
           {
             "metric_name": "sysUpTime",
             "oid": "1.3.6.1.2.1.1.3.0",
             "raw_value": "8640000",
             "type_name": "i64",
             "status": 0
           }
         ]
       }
     ]
   }

4. ``request`` — On-Demand Ad-Hoc SNMP Query
--------------------------------------------
Executes an immediate, ad-hoc SNMP request against a specific DUT using ``ezsnmp``'s C++17 ``SessionBase``. Supports all standard SNMP operations: ``get``, ``getnext``, ``walk``, ``bulkget``, and ``bulkwalk``.

.. code-block:: bash

   # Execute SNMP GET for sysUpTime
   ezsnmp-data-logger request --config configs/my_lab.json \
       --dut edge-router-01 --op get --oid 1.3.6.1.2.1.1.3.0 --json

   # Walk the entire system MIB tree
   ezsnmp-data-logger request --config configs/my_lab.json \
       --dut edge-router-01 --op walk --oid 1.3.6.1.2.1.1 --json

   # Execute bulk walk and record samples directly into active sinks
   ezsnmp-data-logger request --config configs/my_lab.json \
       --dut edge-router-01 --op bulkwalk --oid 1.3.6.1.2.1.2.2.1.10 --record --json

Options:
- ``-d, --dut <name>``: Target DUT identifier from the JSON configuration.
- ``-o, --op <operation>``: Operation name: ``get``, ``getnext``, ``walk``, ``bulkget``, ``bulkwalk``.
- ``--oid <oids>``: Comma-separated list of OIDs to query.
- ``--record``: If specified, pushes the returned SNMP results into the configured storage sinks.
- ``--json``: Outputs structured JSON response.

5. ``set`` — Execute SNMP SET Operation
---------------------------------------
Executes an on-demand SNMP SET operation against a target DUT to modify device parameters.

.. code-block:: bash

   # Set system contact string (type 's' = string)
   ezsnmp-data-logger set --config configs/my_lab.json \
       --dut edge-router-01 \
       --oid 1.3.6.1.2.1.1.4.0 \
       --type s \
       --value "admin@example.com" \
       --json

   # Set interface admin status to Up (type 'i' = integer, value 1)
   ezsnmp-data-logger set --config configs/my_lab.json \
       --dut edge-router-01 \
       --oid 1.3.6.1.2.1.2.2.1.7.1 \
       --type i \
       --value "1" \
       --json

Supported ASN.1 Type Identifiers:
- ``i``: Integer (32-bit signed integer)
- ``u``: Unsigned32 / Gauge32
- ``s``: Octet String / ASCII text
- ``x``: Hex String (e.g. ``"DEADBEEF"``)
- ``d``: Decimal String
- ``n``: Null
- ``o``: Object Identifier (OID)
- ``t``: TimeTicks (hundredths of a second)
- ``a``: IPv4 Address (e.g. ``"192.168.1.1"``)

6. ``run`` — Start SNMP Telemetry Engine Loop
---------------------------------------------
Starts the high-performance C++17 telemetry polling engine loop. Polling threads query DUTs concurrently, queue batches in a thread-safe bounded buffer, and worker threads drain batches to all configured sinks.

.. code-block:: bash

   # Run a single polling cycle across all DUTs and terminate immediately
   ezsnmp-data-logger run --config configs/my_lab.json --once

   # Run with a cap on maximum cycles (e.g. 10 cycles)
   ezsnmp-data-logger run --config configs/my_lab.json --max-cycles 10

   # Override polling interval to 500ms
   ezsnmp-data-logger run --config configs/my_lab.json --interval-ms 500

   # Continuous execution with real-time JSON event streaming (used by GUIs)
   ezsnmp-data-logger run --config configs/my_lab.json --json

Real-Time JSON Stream Event Protocol:
When ``--json`` is passed to ``run``, the engine outputs NDJSON (newline-delimited JSON) events to ``stdout``:

.. code-block:: json

   {"event": "engine_started", "interval_ms": 1000, "duts": 2, "sinks": 3}
   {"event": "cycle_start", "cycle_index": 1, "ts_unix_ns": 1730560000000000000}
   {"event": "batch", "batch": {"dut_name": "edge-router-01", "samples": [{"metric_name": "cpuLoad1m", "raw_value": "0.35"}]}}
   {"event": "cycle_end", "cycle_index": 1, "samples_polled": 6, "batches_sunk": 2, "duration_ms": 18.4}

7. ``inspect`` — Query Stored Telemetry Rows
--------------------------------------------
Queries recent telemetry rows persisted in local storage sinks (SQLite3, CSV, or JSONL) without needing an external SQLite CLI or file reader.

.. code-block:: bash

   # Inspect last 25 samples
   ezsnmp-data-logger inspect --config configs/my_lab.json --limit 25 --json

   # Filter samples by target device and metric name
   ezsnmp-data-logger inspect --config configs/my_lab.json \
       --dut edge-router-01 --metric chassisTempC --limit 10 --json

JSON Output Example:

.. code-block:: json

   {
     "ok": true,
     "data": {
       "count": 10,
       "rows": [
         {
           "ts_unix_ns": 1730560001000000000,
           "dut_name": "edge-router-01",
           "metric_name": "chassisTempC",
           "oid": "1.3.6.1.4.1.9.9.13.1.3.1.3.1",
           "snmp_type": "GAUGE",
           "raw_value": "41.2",
           "sink_type": "sqlite3"
         }
       ]
     }
   }

8. ``gui`` — Launch Cross-Platform Graphical Interface
-------------------------------------------------------
Launches either the native Desktop Tkinter Studio or the Browser Web Studio, auto-detecting the environment:

.. code-block:: bash

   # Auto-detect display (Tkinter if display present, else Browser Studio)
   ezsnmp-data-logger gui --config configs/my_lab.json

   # Force Browser Web Studio on specified port
   ezsnmp-data-logger gui --config configs/my_lab.json --frontend web --port 8765

   # Install operating system desktop launcher (.app on macOS, .desktop on Linux)
   ezsnmp-data-logger gui --install-launcher

9. ``version`` — Inspect Engine Capabilities & Version
------------------------------------------------------
Displays version information, CalSemVer metadata, and compiled engine capabilities.

.. code-block:: bash

   ezsnmp-data-logger version

Text Output:

.. code-block:: text

   ezsnmp-data-logger 2026.11.5.1 (ezsnmp=yes, sqlite3=yes, psql=no)

JSON Output:

.. code-block:: bash

   ezsnmp-data-logger version --json

.. code-block:: json

   {
     "version": "2026.11.5.1",
     "calsemver": true,
     "capabilities": {
       "ezsnmp": true,
       "sqlite3": true,
       "postgresql": false,
       "prometheus": true,
       "swig_languages": ["python", "javascript", "csharp", "go"]
     }
   }
