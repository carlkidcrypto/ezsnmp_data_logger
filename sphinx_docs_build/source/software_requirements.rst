=====================
Software Requirements
=====================

This document defines the formal software requirements for
``ezsnmp_data_logger``. All requirements follow a ``[CATEGORY-NN]`` identifier
convention and use normative **Shall** statements to ensure traceability across
C++17 implementation, SWIG bindings, CLI commands, cross-platform GUIs,
automated tests, and CI/CD pipelines.

.. contents:: Table of Contents
   :local:
   :depth: 2

General & Architectural Requirements
====================================

- **[GEN-01]** The core telemetry engine, JSON configuration parser and
  validator, storage sinks, and CLI dispatcher **Shall** be implemented in
  portable **ISO C++17**.

- **[GEN-02]** ``ezsnmp_data_logger`` **Shall** depend directly on the
  ``ezsnmp`` C++17 ``SessionBase``, ``Result``, and ``GenericErrorBase`` layers
  to perform all live SNMPv1, SNMPv2c, and SNMPv3 ``get``, ``getnext``,
  ``walk``, ``bulkget``, ``bulkwalk``, and ``set`` operations against end
  devices, and **Shall** normalize and store returned ``Result`` payloads in
  the ``data_logger`` storage layer.

- **[GEN-03]** 100% of runtime, Device Under Test (DUT), ``ezsnmp`` session,
  storage sink, CLI, and GUI settings **Shall** be represented and persisted
  in **RFC-8259 JSON**.

- **[GEN-04]** Package ``__init__.py`` files **Shall** contain only symbol
  imports, ``__version__``, and ``__all__`` exports, with zero executable
  side effects at import time.

- **[GEN-05]** All C++17 core libraries, CLI binaries, Python bindings, and
  Graphical User Interfaces **Shall** operate across **Linux, macOS, and
  Windows**.

CalSemVer Versioning Requirements
=================================

- **[VER-01]** ``ezsnmp_data_logger`` **Shall** use **CalSemVer** (Calendar
  Semantic Versioning) for all releases of this project: ``YYYY.M.D`` (4-digit
  year, non-zero-padded month ``1-12``, non-zero-padded day ``1-31``, with an
  optional ``.PATCH`` hotfix segment ``YYYY.M.D.P``) for stable releases,
  PEP-440 pre-release suffixes (``aN``, ``bN``, ``rcN``, e.g.,
  ``2026.11.5.1a0``) for Python, C++17, Meson, and Doxygen artifacts, SemVer-2.0
  pre-release suffixes (``-alpha.N``, ``-beta.N``, ``-rc.N``, e.g.,
  ``2026.11.5.1-alpha.0``) where target ecosystems (npm ``package.json``, .NET
  ``.csproj``) mandate hyphenated pre-releases, and ``v``-prefixed tags (e.g.,
  ``v2026.11.5.1``) for Git releases, Sphinx documentation
  archives, and Go modules.

- **[VER-02]** ``ezsnmp_data_logger::is_valid_calsemver`` **Shall** validate
  CalSemVer identifiers programmatically across C++17 and all 4 SWIG target
  languages, rejecting zero-padded calendar segments (such as ``2026.01.05``)
  or legacy non-calendar versions.

- **[VER-03]** All version declarations across C++17 (``src/version.cpp``,
  ``meson.build``, ``CMakeLists.txt``), Python (``pyproject.toml``,
  ``ezsnmp_data_logger/__init__.py``), documentation (``.doxygen``,
  ``sphinx_docs_build/source/conf.py``), and multi-language package manifests
  (``bindings/javascript/package.json``,
  ``bindings/csharp/EzSnmpDataLogger.csproj``,
  ``bindings/csharp/CMakeLists.txt``) **Shall** remain synchronized on the
  active project CalSemVer identifier and be verified by automated tests.

- **[VER-04]** ``ezsnmp_data_logger`` **Shall** expose its active CalSemVer
  version via ``ezsnmp_data_logger::version()``,
  ``ezsnmp_data_logger.__version__``, and ``ezsnmp-data-logger version
  [--json]``, and **Shall** standardize on ``ezsnmp``'s CalSemVer release line
  (``ezsnmp>=2026.10.28a0``, git tag ``v2026.10.28a0``) until a stable
  CalSemVer ``ezsnmp`` release is published.

JSON Configuration & Validation Requirements
============================================

- **[CFG-01]** The C++17 library **Shall** include a self-contained,
  order-preserving RFC-8259 JSON parser and serializer with line-and-column
  syntax diagnostics and zero external header-only JSON library dependencies.

- **[CFG-02]** ``validate_config`` and ``validate_config_file`` **Shall**
  validate ``logger``, ``duts``, ``sinks``, and ``gui`` sections and return a
  structured ``ValidationReport`` containing ``valid``, ``errors``,
  ``warnings``, ``enabled_duts``, ``total_metrics``, and ``enabled_sinks``.

- **[CFG-03]** Configuration validation **Shall** reject protocol-invalid
  combinations, including ``bulkget`` or ``bulkwalk`` with SNMPv1, and SNMPv3
  ``authNoPriv`` / ``authPriv`` configurations missing USM credentials or
  passphrases shorter than 8 characters.

- **[CFG-04]** ``generate_template_config_json`` **Shall** generate valid
  starter JSON documents for ``minimal``, ``demo``, ``enterprise``, and
  ``snmpv3`` profiles, and ``config_json_schema`` **Shall** output a Draft
  2020-12 JSON Schema covering all configuration keys.

Engine, Queue & Storage Sink Requirements
=========================================

- **[ENG-01]** ``EzSnmpPoller`` **Shall** map all ``SnmpConfig`` fields to
  ``ezsnmp::SessionBase``, coalesce scalar ``get`` metrics per DUT into
  multi-OID ``SessionBase::get`` calls, scope ``-Cr<N>`` max-repeaters strictly
  to ``bulkget`` and ``bulkwalk`` PDUs, and capture ``ezsnmp`` exceptions as
  structured error ``Sample`` records without crashing worker threads.

- **[ENG-02]** ``StubSnmpPoller`` **Shall** provide deterministic synthetic
  SNMP ``poll``, ``request``, and ``set_oid`` responses when ``snmp.mode`` is
  set to ``"stub"`` for offline testing, demos, and CI validation.

- **[ENG-03]** ``DataLogger`` **Shall** decouple multi-DUT polling worker
  threads from sink persistence using a thread-safe ``BoundedQueue<Batch>``
  that enforces bounded memory under back-pressure and drains all queued
  batches prior to shutdown.

- **[ENG-04]** ``Sample`` and persistent storage sinks **Shall** preserve full
  ``ezsnmp::Result`` metadata (``oid``, ``oid_index``, ``snmp_type``,
  ``raw_value``) alongside normalized typed values (``type_tag``, ``i64``,
  ``f64``, ``str_val``, ``status``) and **Shall** accept ``ezsnmp``
  ``Result.to_dict()`` JSON objects directly via ``Sample::from_json``.

- **[ENG-05]** ``SqliteSink`` **Shall** operate in SQLite3 WAL mode with
  normalized ``series`` and ``samples`` tables, prepared statements, in-memory
  ``series_id`` caching, and configurable batch/time transaction commits.

- **[ENG-06]** ``CsvSink`` and ``JsonlSink`` **Shall** write per-DUT RFC-4180
  CSV and newline-delimited JSON files with configurable flush thresholds.

- **[ENG-07]** ``PsqlSink`` **Shall** support PostgreSQL and optional
  TimescaleDB hypertables via ``libpq`` when compiled with
  ``EZSNMP_DL_HAVE_LIBPQ=1``, and **Shall** raise a descriptive ``SinkError``
  when ``libpq`` was not compiled into the binary.

- **[ENG-08]** ``PrometheusSink`` **Shall** expose latest numeric SNMP
  readings and scrape counters over an embedded cross-platform HTTP ``/metrics``
  endpoint supporting ephemeral port ``0`` binding on POSIX and Winsock2.

- **[ENG-09]** ``inspect_configured_sinks`` **Shall** query recent stored
  telemetry rows across SQLite3, JSONL, and CSV sinks with optional DUT and
  metric filters.

Multi-Language SWIG 4.x Distribution Requirements
=================================================

- **[SWIG-01]** Modular SWIG 4.x interface files in ``interface/``
  (``ezsnmp_data_logger.i``, ``exceptions.i``, ``datatypes.i``, ``config.i``,
  ``data_logger.i``) **Shall** expose ``DataLogger`` (via copyable PIMPL),
  ``CliRunner``, configuration structs, telemetry datatypes, and C++ exceptions
  cleanly to target languages.

- **[SWIG-02]** ``interface/ezsnmp_data_logger.i``,
  ``tools/generate_swig_bindings.py``, and ``bindings/`` **Shall** generate and
  package clean SWIG 4.x bindings exclusively for the 4 supported target
  languages: ``python``, ``javascript`` (Node.js N-API), ``csharp`` (.NET
  P/Invoke), and ``go`` (cgo).

- **[SWIG-03]** In Python bindings, blocking C++ operations (``run``,
  ``run_once``, ``stop``, ``probe``, ``request_json``, ``set_json``,
  ``CliRunner::run``, ``CliRunner::execute``) **Shall** release Python's GIL
  via ``%thread``.

Self-Contained Multi-Language Packaging Requirements
====================================================

- **[PKG-01]** Every language package built from ``ezsnmp_data_logger`` across
  ``python``, ``javascript`` (Node.js N-API), ``csharp``, and ``go`` **Shall**
  be **self-contained** so end users never need to install external native
  libraries (Net-SNMP, OpenSSL, SQLite3, or SWIG) at runtime, while publishing
  under the project's CalSemVer release identifiers (``[VER-01]``-``[VER-04]``).

- **[PKG-02]** ``tools/prepare_sqlite3_amalgamation.py`` **Shall** provision a
  SHA-256-verified public-domain SQLite3 C amalgamation
  (``third_party/sqlite3/sqlite3.c`` and ``sqlite3.h``) that ``setup.py``,
  ``meson.build``, ``CMakeLists.txt``, and
  ``bindings/{javascript,csharp,go}`` compile directly into the native binary,
  eliminating runtime ``libsqlite3`` shared-library dependencies across Linux,
  macOS, and Windows.

- **[PKG-03]** Standalone dependency scripts
  (``tools/prepare_linux_netsnmp.sh``, ``tools/prepare_macos_netsnmp.sh``,
  ``tools/prepare_windows_netsnmp.ps1``, and ``tools/prepare_ezsnmp_source.py``)
  **Shall** provision ``ezsnmp`` ``v2026.10.28a0`` and build minimal
  SHA-256-verified Net-SNMP ``5.10.pre2`` libraries with OpenSSL and
  Blumenthal-AES support.

- **[PKG-04]** ``pyproject.toml`` **Shall** declare ``swig>=4.3.0`` in
  ``[build-system] requires``, ``ezsnmp>=2026.10.28a0`` in ``dependencies``,
  and configure ``cibuildwheel`` with ``auditwheel repair`` (Linux
  ``manylinux``/``musllinux``), ``delocate-wheel`` (macOS), and ``delvewheel
  repair`` (Windows ``AMD64``/``x86``/``ARM64``) to produce standalone Python
  wheels.

- **[PKG-05]** ``tools/bundle_native_package.py`` **Shall** inspect compiled
  native extensions/libraries across ``python``, ``javascript`` (``.node``),
  ``csharp`` (``runtimes/<rid>/native/``), and ``go`` (``bindings/go/lib/``),
  bundle non-OS shared libraries into a package-local runtime directory, rewrite
  dynamic loader search paths (``$ORIGIN`` via ``patchelf`` on Linux,
  ``@loader_path`` via ``install_name_tool`` on macOS, and side-by-side DLLs on
  Windows), and verify zero external non-OS runtime library dependencies
  remain.

Command-Line Interface (CLI) Requirements
=========================================

- **[CLI-01]** The ``ezsnmp-data-logger`` CLI **Shall** provide ``run``,
  ``validate``, ``probe``, ``request``, ``set``, ``inspect``, ``init-config``,
  ``gui``, ``schema``, ``version``, and ``help`` subcommands, while remaining
  backward-compatible with legacy ``ezsnmp_loggerd --config <file>``
  invocations.

- **[CLI-02]** Every operational CLI subcommand **Shall** support ``--json`` to
  emit machine-readable JSON documents (and newline-delimited ``started``,
  ``batch``, ``stats``, and ``stopped`` JSON events during ``run --json``).

- **[CLI-03]** ``CliRunner::execute`` **Shall** allow any SWIG target language
  to invoke CLI commands in-process and capture ``exit_code``, ``stdout_text``,
  and ``stderr_text`` in a ``CliResult`` structure.

Cross-Platform GUI Requirements
===============================

- **[GUI-01]** All Graphical User Interfaces **Shall** act strictly as
  frontends driving the ``ezsnmp-data-logger`` CLI behind the scenes via
  subprocess execution and JSON payloads (``CliSubprocessBridge``), without
  duplicating SNMP or storage engine logic.

- **[GUI-02]** Every GUI library and runtime component **Shall** work across
  **Linux, macOS, and Windows** using Python standard-library ``tkinter.ttk``
  (Native Desktop Studio) and ``http.server.ThreadingHTTPServer`` + HTML5/SVG
  (Browser Web Studio), selectable via ``gui.frontend`` (``"auto"``, ``"tk"``,
  or ``"web"``) in the JSON configuration.

- **[GUI-03]** Both GUI frontends **Shall** display a live Behind-the-Scenes CLI
  Command Audit Trail recording the exact CLI command, exit code, and execution
  time (ms) of every user action.

- **[GUI-04]** Both GUI frontends **Shall** support starting and stopping live
  CLI polling with real-time numeric telemetry charts, executing ad-hoc
  ``ezsnmp`` ``get``, ``getnext``, ``walk``, ``bulkget``, ``bulkwalk``, and
  ``set`` operations, inspecting stored sink rows, editing, validating, and
  saving JSON configurations, and **Shall** allow users to interactively
  select one or many plotted fields to view at a time with automatic Y-axis
  rescaling, "Select All", "Clear All", and clickable canvas legend hitboxes.

- **[GUI-05]** All user-facing UI strings (window and header titles, button
  labels, tabs, cards, sections, table headers, form labels, status bars, and
  dialogs) across both Desktop (Tkinter) and Web (Browser) frontends **Shall**
  be externalized into a ``locale.json`` definition file formatted as
  ``{"LOCALE_KEY_NAME": {"en": "...", ...}}`` with per-language definitions
  as needed (defaulting to English ``"en"`` in
  ``ezsnmp_data_logger/gui/locale.json``), managed by ``LocaleManager``,
  exposed via the ``GET /api/locale`` and ``POST /api/locale`` endpoints,
  configurable via ``gui.locale`` and ``gui.locale_file`` in the unified JSON
  configuration or CLI ``--locale`` / ``--locale-file`` flags, and provide
  automatic fallback to English when keys or translations are omitted.

- **[GUI-06]** Both GUI frontends and operating system application launchers
  **Shall** include custom branding icon suites replacing generic Python or
  browser tab icons: Browser Web Studio supplies modern vector SVG favicons
  (``favicon.svg``), standard ICO (``favicon.ico``), multi-resolution PNGs
  (``favicon-32x32.png``, ``favicon-16x16.png``, ``apple-touch-icon.png``),
  and top bar brand badges; Desktop Tkinter Studio applies custom window
  titlebar icons (``app_icon.png`` / ``app_icon.ico``) and dynamically
  configures the macOS Dock application icon using standard Cocoa/AppKit
  integration via ``ctypes``; and desktop application launchers can be
  installed on macOS (``.app`` bundle with Retina ``app_icon.icns``), Linux
  (``.desktop`` entry with ``app_icon.png``), and Windows via
  ``ezsnmp-data-logger-gui --install-launcher`` and
  ``tools/install_desktop_launcher.py``.

Testing, Documentation & CI/CD Requirements
===========================================

- **[C++T-01]** C++17 GoogleTest suites in ``cpp_tests/`` **Shall** test JSON
  parsing, configuration validation, bounded queues, all local/HTTP sinks,
  ``DataLogger``, ``CliRunner``, ``StubSnmpPoller``, and live ``EzSnmpPoller``
  UDP ``get``/``set`` round-trips, using ``EZSNMP_SKIP_TEST_AND_RETURN`` for
  older GoogleTest compatibility.

- **[PYT-01]** Python ``pytest`` suites in ``python_tests/`` **Shall** verify
  Python SWIG bindings, ``[GEN-04]`` ``__init__.py`` purity, CalSemVer version
  alignment, self-contained native library bundling, ``CliSubprocessBridge``
  streaming, Browser Studio HTTP endpoints, ``tkinter.ttk`` Desktop Studio
  instantiation, repository JSON configs, and 4-language SWIG code generation
  (``python``, ``javascript``, ``csharp``, ``go``).

- **[DOC-01]** Documentation in ``sphinx_docs_build/`` **Shall** use Sphinx +
  ``furo`` + ``sphinx.ext.autodoc`` + ``sphinx.ext.githubpages`` + Doxygen XML
  + ``breathe`` alongside ``sphinx_docs_build/landing/`` with
  ``VERSION_LIST_START`` / ``VERSION_LIST_END`` markers matching sibling
  ``carlkidcrypto`` repositories, maintaining a ``.nojekyll`` sentinel file in
  ``docs/`` to disable Jekyll processing on GitHub Pages, building cleanly
  with ``SPHINXOPTS="-W"``.

- **[DOC-02]** Documentation **Shall** provide a dedicated multi-OS
  installation guide (``installation.rst``) for macOS, Windows, Linux, and
  Alpine Linux, an exhaustive CLI command reference (``cli_guide.rst``) covering
  all 9 subcommands, flags, ASN.1 type codes, and JSON contracts, and a
  thorough GUI user guide (``gui_guide.rst``) detailing Tabs 1-6, the CLI
  audit trail, real-time charting with series selection, and OS launchers.

- **[FMT-01]** All hand-written C++17 files **Shall** conform to
  ``.clang-format`` and all hand-written Python files **Shall** conform to
  ``black --line-length 100``.

- **[CICD-01]** GitHub Actions workflows in ``.github/workflows/`` **Shall**
  keep matrix job counts strictly under 256, scope ``concurrency.group`` by
  ``${{ github.workflow }}``, and pass ``yamllint`` and ``actionlint``.

- **[CICD-02]** Automated release publishing workflows **Shall** support
  distributing multi-platform binary wheels and source distributions to PyPI
  and TestPyPI (``build_and_publish_to_pypi.yml``,
  ``build_and_publish_to_test_pypi.yml``) using ``cibuildwheel`` and
  ``carlkidcrypto/os-specific-runner`` across Linux, macOS, and Windows,
  pre-provisioning embedded C++17 dependencies (``third_party/ezsnmp``) with
  retry resilience, skipping existing distribution artifacts idempotently,
  and publishing self-contained Node.js packages to npm
  (``build_and_publish_to_npm.yml``) with OIDC Trusted Publishing, Sigstore
  provenance attestations, package lock caching, multi-core parallel native
  addon compilation, idempotent skip detection, and prerelease tag resolution.

- **[CICD-03]** Automated release publishing workflows **Shall** support
  building and packaging multi-runtime C# (.NET 8) packages to NuGet and
  GitHub Packages (``build_and_publish_to_nuget.yml``) using strict C++17
  conformance (``CMAKE_CXX_EXTENSIONS=OFF``), OpenSSL include path
  discovery across Linux and macOS, multi-core MSVC (``/MP``) parallel builds,
  OIDC Trusted Publishing authentication (``NuGet/login@v1``) with duplicate
  skipping, and generating validated WinGet community manifests with automated
  repository submission (``build_and_publish_to_winget.yml``,
  ``tools/generate_winget_manifest.py``).

- **[CICD-04]** Automated dependency maintenance **Shall** be configured via
  ``.github/dependabot.yml`` with weekly updates across GitHub Actions,
  Python (root and Sphinx docs), npm (``bindings/javascript``), NuGet
  (``bindings/csharp``), and Go (``bindings/go``), grouping package updates
  and respecting agentic workflow exclusions.

- **[CICD-05]** SonarQube Cloud automatic analysis **Shall** be configured
  via root ``.sonarcloud.properties`` targeting an A rating across security,
  reliability, and maintainability, excluding documentation (``docs/**``,
  ``sphinx_docs_build/**``), workflows (``.github/**``), vendored dependencies
  (``third_party/**``), and generated SWIG artifacts from code smells and
  copy-paste duplication metrics.

- **[CICD-06]** Continuous integration workflows **Shall** incorporate smart
  file-change detection via ``tj-actions/changed-files`` to skip redundant
  multi-platform matrix runner executions when pull requests only touch
  unrelated subsystems, and **Shall** consolidate multi-OS conditionals
  using ``carlkidcrypto/os-specific-runner`` across Ubuntu, macOS, and
  Windows.

- **[CICD-07]** Python wheel packaging **Shall** utilize parallel multi-core
  compilation (via ``EZSNMP_BUILD_JOBS`` and ``ThreadPoolExecutor``), fast
  dependency isolation via ``build[uv]``, and cross-version deterministic C++
  object caching (via ``EZSNMP_OBJECT_CACHE_DIR``) across matrix runners.

- **[CICD-08]** Test deployment workflows
  (``build_and_publish_to_test_pypi.yml``) **Shall** automatically trigger
  on pushes to ``main`` guarded by smart change detection
  (``tj-actions/changed-files``) over source and packaging files, ensuring
  distribution packaging regressions fail prior to production release.



