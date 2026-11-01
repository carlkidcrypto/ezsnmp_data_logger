=========
CLI Guide
=========

The ``ezsnmp-data-logger`` CLI provides full control over polling, validation, live ``ezsnmp`` requests/sets, and sink inspection. Passing ``--json`` to any subcommand outputs structured JSON (the exact contract used by the cross-platform GUIs behind the scenes).

Subcommands
===========

.. code-block:: bash

   # 1. Generate a starter JSON configuration
   ezsnmp-data-logger init-config --template demo --output configs/my_lab.json

   # 2. Validate JSON configuration syntax and SNMP protocol rules
   ezsnmp-data-logger validate --config configs/my_lab.json --json

   # 3. Probe all enabled DUTs once without writing to storage sinks
   ezsnmp-data-logger probe --config configs/my_lab.json

   # 4. Run an on-demand ezsnmp GET / WALK / BULKWALK against a DUT
   ezsnmp-data-logger request --config configs/my_lab.json \
       --dut core-switch-01 --op get --oid 1.3.6.1.2.1.1.3.0 --record --json

   # 5. Run an on-demand ezsnmp SET operation against a DUT
   ezsnmp-data-logger set --config configs/my_lab.json \
       --dut core-switch-01 --oid 1.3.6.1.2.1.1.5.0 --type s --value lab-core-01 --json

   # 6. Run a single polling cycle across all enabled DUTs and exit
   ezsnmp-data-logger run --config configs/my_lab.json --once

   # 7. Run continuous multi-DUT polling (streams JSON events with --json)
   ezsnmp-data-logger run --config configs/my_lab.json --json

   # 8. Query recent stored telemetry rows from SQLite3 / JSONL / CSV sinks
   ezsnmp-data-logger inspect --config configs/my_lab.json --limit 25 --json

   # 9. Print version, compiled capabilities, and supported SWIG languages
   ezsnmp-data-logger version --json
