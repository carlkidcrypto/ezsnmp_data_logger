=====================
Software Requirements
=====================

This document defines the formal software requirements for ``ezsnmp_data_logger``.
All requirements follow a ``[CATEGORY-NN]`` identifier convention and use normative
**Shall** statements to ensure traceability across C++17 implementation, SWIG bindings,
CLI commands, cross-platform GUIs, automated tests, and CI/CD pipelines.

General & Architectural Requirements
====================================

- **[GEN-01]** The core telemetry engine, JSON configuration parser/validator, storage sinks, and CLI dispatcher **Shall** be implemented in portable **ISO C++17**.
- **[GEN-02]** ``ezsnmp_data_logger`` **Shall** depend directly on the ``ezsnmp`` C++17 ``SessionBase``, ``Result``, and ``GenericErrorBase`` layers to perform all live SNMPv1, SNMPv2c, and SNMPv3 ``get``, ``getnext``, ``walk``, ``bulkget``, ``bulkwalk``, and ``set`` operations against end devices, and **Shall** normalize and store returned ``Result`` payloads in the ``data_logger`` storage layer.
- **[GEN-03]** 100% of runtime, Device Under Test (DUT), ``ezsnmp`` session, storage sink, CLI, and GUI settings **Shall** be represented and persisted in **RFC-8259 JSON**.
- **[GEN-04]** Package ``__init__.py`` files **Shall** contain only symbol imports, ``__version__``, and ``__all__`` exports, with zero executable side effects at import time.
- **[GEN-05]** All C++17 core libraries, CLI binaries, Python bindings, and Graphical User Interfaces **Shall** operate across **Linux, macOS, and Windows**.

JSON Configuration & Validation Requirements
============================================

- **[CFG-01]** The C++17 library **Shall** include a self-contained, order-preserving RFC-8259 JSON parser and serializer with line-and-column syntax diagnostics and zero external header-only JSON library dependencies.
- **[CFG-02]** ``validate_config`` and ``validate_config_file`` **Shall** validate ``logger``, ``duts``, ``sinks``, and ``gui`` sections and return a structured ``ValidationReport`` containing ``valid``, ``errors``, ``warnings``, ``enabled_duts``, ``total_metrics``, and ``enabled_sinks``.
- **[CFG-03]** Configuration validation **Shall** reject protocol-invalid combinations, including ``bulkget`` or ``bulkwalk`` with SNMPv1 and SNMPv3 ``authNoPriv`` / ``authPriv`` configurations missing USM credentials or passphrases shorter than 8 characters.
- **[CFG-04]** ``generate_template_config_json`` **Shall** generate valid starter JSON documents for ``minimal``, ``demo``, ``enterprise``, and ``snmpv3`` profiles, and ``config_json_schema`` **Shall** output a Draft 2020-12 JSON Schema covering all configuration keys.

Engine, Queue & Storage Sink Requirements
=========================================

- **[ENG-01]** ``EzSnmpPoller`` **Shall** map all ``SnmpConfig`` fields to ``ezsnmp::SessionBase``, coalesce scalar ``get`` metrics per DUT into multi-OID ``SessionBase::get`` calls, scope ``-Cr<N>`` max-repeaters strictly to ``bulkget`` and ``bulkwalk`` PDUs, and capture ``ezsnmp`` exceptions as structured error ``Sample`` records without crashing worker threads.
- **[ENG-02]** ``StubSnmpPoller`` **Shall** provide deterministic synthetic SNMP ``poll``, ``request``, and ``set_oid`` responses when ``snmp.mode`` is set to ``"stub"`` for offline testing, demos, and CI validation.
- **[ENG-03]** ``DataLogger`` **Shall** decouple multi-DUT polling worker threads from sink persistence using a thread-safe ``BoundedQueue<Batch>`` that enforces bounded memory under back-pressure and drains all queued batches prior to shutdown.
- **[ENG-04]** ``Sample`` and persistent storage sinks **Shall** preserve full ``ezsnmp::Result`` metadata (``oid``, ``oid_index``, ``snmp_type``, ``raw_value``) alongside normalized typed values (``type_tag``, ``i64``, ``f64``, ``str_val``, ``status``) and **Shall** accept ``ezsnmp`` ``Result.to_dict()`` JSON objects directly via ``Sample::from_json``.
- **[ENG-05]** ``SqliteSink`` **Shall** operate in SQLite3 WAL mode with normalized ``series`` and ``samples`` tables, prepared statements, in-memory ``series_id`` caching, and configurable batch/time transaction commits.
- **[ENG-06]** ``CsvSink`` and ``JsonlSink`` **Shall** write per-DUT RFC-4180 CSV and newline-delimited JSON files with configurable flush thresholds.
- **[ENG-07]** ``PsqlSink`` **Shall** support PostgreSQL and optional TimescaleDB hypertables via ``libpq`` when compiled with ``EZSNMP_DL_HAVE_LIBPQ=1``, and **Shall** raise a descriptive ``SinkError`` when ``libpq`` was not compiled into the binary.
- **[ENG-08]** ``PrometheusSink`` **Shall** expose latest numeric SNMP readings and scrape counters over an embedded cross-platform HTTP ``/metrics`` endpoint supporting ephemeral port ``0`` binding on POSIX and Winsock2.
- **[ENG-09]** ``inspect_configured_sinks`` **Shall** query recent stored telemetry rows across SQLite3, JSONL, and CSV sinks with optional DUT and metric filters.

Multi-Language SWIG 4.x Distribution Requirements
=================================================

- **[SWIG-01]** Modular SWIG 4.x interface files in ``interface/`` (``ezsnmp_data_logger.i``, ``exceptions.i``, ``datatypes.i``, ``config.i``, ``data_logger.i``) **Shall** expose ``DataLogger`` (via copyable PIMPL), ``CliRunner``, configuration structs, telemetry datatypes, and C++ exceptions cleanly to target languages.
- **[SWIG-02]** ``interface/ezsnmp_data_logger.i`` and ``tools/generate_swig_bindings.py`` **Shall** generate valid wrapper code across all 16 SWIG 4.x targets: ``python``, ``javascript`` (Node.js N-API), ``java``, ``csharp``, ``go``, ``ruby``, ``perl5``, ``php``, ``lua``, ``r``, ``tcl8``, ``octave``, ``d``, ``guile``, ``scilab``, and ``xml``.
- **[SWIG-03]** In Python bindings, blocking C++ operations (``run``, ``run_once``, ``stop``, ``probe``, ``request_json``, ``set_json``, ``CliRunner::run``, ``CliRunner::execute``) **Shall** release Python's GIL via ``%thread``.

Command-Line Interface (CLI) Requirements
=========================================

- **[CLI-01]** The ``ezsnmp-data-logger`` CLI **Shall** provide ``run``, ``validate``, ``probe``, ``request``, ``set``, ``inspect``, ``init-config``, ``schema``, ``version``, and ``help`` subcommands, while remaining backward-compatible with legacy ``ezsnmp_loggerd --config <file>`` invocations.
- **[CLI-02]** Every operational CLI subcommand **Shall** support ``--json`` to emit machine-readable JSON documents (and newline-delimited ``started``, ``batch``, ``stats``, and ``stopped`` JSON events during ``run --json``).
- **[CLI-03]** ``CliRunner::execute`` **Shall** allow any SWIG target language to invoke CLI commands in-process and capture ``exit_code``, ``stdout_text``, and ``stderr_text`` in a ``CliResult`` structure.

Cross-Platform GUI Requirements
===============================

- **[GUI-01]** All Graphical User Interfaces **Shall** act strictly as frontends driving the ``ezsnmp-data-logger`` CLI behind the scenes via subprocess execution and JSON payloads (``CliSubprocessBridge``), without duplicating SNMP or storage engine logic.
- **[GUI-02]** Every GUI library and runtime component **Shall** work across **Linux, macOS, and Windows** using Python standard-library ``tkinter.ttk`` (Native Desktop Studio) and ``http.server.ThreadingHTTPServer`` + HTML5/SVG (Browser Web Studio), selectable via ``gui.frontend`` (``"auto"``, ``"tk"``, or ``"web"``) in the JSON configuration.
- **[GUI-03]** Both GUI frontends **Shall** display a live Behind-the-Scenes CLI Command Audit Trail recording the exact CLI command, exit code, and execution time (ms) of every user action.
- **[GUI-04]** Both GUI frontends **Shall** support starting/stopping live CLI polling with real-time numeric telemetry charts, executing ad-hoc ``ezsnmp`` ``get``/``getnext``/``walk``/``bulkget``/``bulkwalk``/``set`` operations, inspecting stored sink rows, and editing/validating/saving JSON configurations.

Testing, Documentation & CI/CD Requirements
===========================================

- **[C++T-01]** C++17 GoogleTest suites in ``cpp_tests/`` **Shall** test JSON parsing, configuration validation, bounded queues, all local/HTTP sinks, ``DataLogger``, ``CliRunner``, ``StubSnmpPoller``, and live ``EzSnmpPoller`` UDP ``get``/``set`` round-trips, using ``EZSNMP_SKIP_TEST_AND_RETURN`` for older GoogleTest compatibility.
- **[PYT-01]** Python ``pytest`` suites in ``python_tests/`` **Shall** verify Python SWIG bindings, ``[GEN-04]`` ``__init__.py`` purity, ``CliSubprocessBridge`` streaming, Browser Studio HTTP endpoints, ``tkinter.ttk`` Desktop Studio instantiation, repository JSON configs, and 16-language SWIG code generation.
- **[DOC-01]** Documentation in ``sphinx_docs_build/`` **Shall** use Sphinx + ``furo`` + ``sphinx.ext.autodoc`` + Doxygen XML + ``breathe`` alongside ``sphinx_docs_build/landing/`` with ``VERSION_LIST_START`` / ``VERSION_LIST_END`` markers matching sibling ``carlkidcrypto`` repositories, building cleanly with ``SPHINXOPTS="-W"``.
- **[FMT-01]** All hand-written C++17 files **Shall** conform to ``.clang-format`` and all hand-written Python files **Shall** conform to ``black --line-length 100``.
- **[CICD-01]** GitHub Actions workflows in ``.github/workflows/`` **Shall** keep matrix job counts strictly under 256, scope ``concurrency.group`` by ``${{ github.workflow }}``, and pass ``yamllint`` and ``actionlint``.
