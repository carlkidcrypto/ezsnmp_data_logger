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
  devices.

- **[GEN-03]** ``ezsnmp_data_logger`` **Shall** normalize and store returned
  ``ezsnmp::Result`` payloads into typed telemetry records in the
  ``data_logger`` storage layer.

- **[GEN-04]** 100% of runtime, Device Under Test (DUT), ``ezsnmp`` session,
  storage sink, CLI, and GUI settings **Shall** be represented and persisted
  in **RFC-8259 JSON**.

- **[GEN-05]** Package ``__init__.py`` files **Shall** contain only symbol
  imports, ``__version__``, and ``__all__`` exports, with zero executable
  side effects at import time.

- **[GEN-06]** All C++17 core libraries, CLI binaries, Python bindings, and
  Graphical User Interfaces **Shall** operate across **Linux, macOS, and
  Windows**.

CalSemVer Versioning Requirements
=================================

- **[VER-01]** ``ezsnmp_data_logger`` **Shall** use **CalSemVer** (Calendar
  Semantic Versioning) for all releases of this project: ``YYYY.M.D`` (4-digit
  year, non-zero-padded month ``1-12``, non-zero-padded day ``1-31``, with an
  optional ``.PATCH`` hotfix segment ``YYYY.M.D.P``) for stable releases,
  PEP-440 pre-release suffixes (``aN``, ``bN``, ``rcN``, e.g.,
  ``2026.11.7a0``) for Python, C++17, Meson, and Doxygen artifacts, SemVer-2.0
  pre-release suffixes (``-alpha.N``, ``-beta.N``, ``-rc.N``, e.g.,
  ``2026.11.7-alpha.0``) where target ecosystems (npm ``package.json``, .NET
  ``.csproj``) mandate hyphenated pre-releases, and ``v``-prefixed tags (e.g.,
  ``v2026.11.7``) for Git releases, Sphinx documentation
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
  [--json]``.

- **[VER-05]** ``ezsnmp_data_logger`` **Shall** standardize on ``ezsnmp``'s
  CalSemVer release line (``ezsnmp>=2026.11.4a0``, git tag ``v2026.11.4a0``)
  until a stable CalSemVer ``ezsnmp`` release is published.

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
  ``snmpv3`` profiles.

- **[CFG-05]** ``config_json_schema`` **Shall** output a Draft 2020-12 JSON
  Schema covering all configuration keys.

Engine, Queue & Storage Sink Requirements
=========================================

- **[ENG-01]** ``EzSnmpPoller`` **Shall** map all ``SnmpConfig`` fields to
  ``ezsnmp::SessionBase``, coalesce scalar ``get`` metrics per DUT into
  multi-OID ``SessionBase::get`` calls, and scope ``-Cr<N>`` max-repeaters
  strictly to ``bulkget`` and ``bulkwalk`` PDUs.

- **[ENG-02]** ``EzSnmpPoller`` **Shall** capture ``ezsnmp`` exceptions as
  structured error ``Sample`` records without crashing worker threads.

- **[ENG-03]** ``StubSnmpPoller`` **Shall** provide deterministic synthetic
  SNMP ``poll``, ``request``, and ``set_oid`` responses when ``snmp.mode`` is
  set to ``"stub"`` for offline testing, demos, and CI validation.

- **[ENG-04]** ``DataLogger`` **Shall** decouple multi-DUT polling worker
  threads from sink persistence using a thread-safe ``BoundedQueue<Batch>``
  that enforces bounded memory under back-pressure and drains all queued
  batches prior to shutdown.

- **[ENG-05]** ``Sample`` and persistent storage sinks **Shall** preserve full
  ``ezsnmp::Result`` metadata (``oid``, ``oid_index``, ``snmp_type``,
  ``raw_value``) alongside normalized typed values (``type_tag``, ``i64``,
  ``f64``, ``str_val``, ``status``).

- **[ENG-06]** ``Sample::from_json`` **Shall** accept ``ezsnmp``
  ``Result.to_dict()`` JSON objects directly and deserialize them into valid
  ``Sample`` structures.

- **[ENG-07]** ``SqliteSink`` **Shall** operate in SQLite3 WAL mode with
  normalized ``series`` and ``samples`` tables, prepared statements, in-memory
  ``series_id`` caching, and configurable batch/time transaction commits.

- **[ENG-08]** ``CsvSink`` **Shall** write per-DUT RFC-4180 CSV files with
  configurable flush thresholds.

- **[ENG-09]** ``JsonlSink`` **Shall** write per-DUT newline-delimited JSON
  files with configurable flush thresholds.

- **[ENG-10]** ``PsqlSink`` **Shall** support PostgreSQL and optional
  TimescaleDB hypertables via ``libpq`` when compiled with
  ``EZSNMP_DL_HAVE_LIBPQ=1``, and **Shall** raise a descriptive ``SinkError``
  when ``libpq`` was not compiled into the binary.

- **[ENG-11]** ``PrometheusSink`` **Shall** expose latest numeric SNMP
  readings and scrape counters over an embedded cross-platform HTTP ``/metrics``
  endpoint supporting ephemeral port ``0`` binding on POSIX and Winsock2.

- **[ENG-12]** ``inspect_configured_sinks`` **Shall** query recent stored
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
  under the project's CalSemVer release identifiers (``[VER-01]``-``[VER-05]``).

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
  **Shall** provision ``ezsnmp`` ``v2026.11.4a0`` and build minimal
  SHA-256-verified Net-SNMP ``5.10.pre2`` libraries with OpenSSL and
  Blumenthal-AES support.

- **[PKG-04]** ``pyproject.toml`` **Shall** declare ``swig>=4.3.0`` in
  ``[build-system] requires`` and ``ezsnmp>=2026.11.4a0`` in ``dependencies``.

- **[PKG-05]** ``cibuildwheel`` **Shall** configure ``auditwheel repair``
  (Linux ``manylinux``/``musllinux``), ``delocate-wheel`` (macOS), and
  ``delvewheel repair`` (Windows ``AMD64``/``x86``/``ARM64``) to produce
  standalone Python wheels.

- **[PKG-06]** ``tools/bundle_native_package.py`` **Shall** inspect compiled
  native extensions/libraries across ``python``, ``javascript`` (``.node``),
  ``csharp`` (``runtimes/<rid>/native/``), and ``go`` (``bindings/go/lib/``),
  bundle non-OS shared libraries into a package-local runtime directory, rewrite
  dynamic loader search paths (``$ORIGIN`` via ``patchelf`` on Linux,
  ``@loader_path`` via ``install_name_tool`` on macOS, and side-by-side DLLs on
  Windows), and verify zero external non-OS runtime library dependencies
  remain.

- **[PKG-07]** ``tools/package_desktop_app.py`` **Shall** package standalone
  Windows single-file executables (``EzSnmpDataLogger-Windows-x64.exe``)
  and Inno Setup installation packages
  (``EzSnmpDataLogger-Windows-x64-Setup.exe``) bundling embedded Net-SNMP
  libraries and desktop shortcuts.

- **[PKG-08]** ``tools/package_desktop_app.py`` **Shall** package standalone
  macOS application bundles (``EzSnmpDataLogger.app``) and compressed
  disk images (``EzSnmpDataLogger-macOS-<arch>.dmg``) with Retina ICNS
  icons and a drag-and-drop link to ``/Applications``.

- **[PKG-09]** ``tools/package_desktop_app.py`` **Shall** package standalone
  Linux executables, Debian packages (``.deb``), Red Hat packages (``.rpm``),
  and portable compressed archives (``.tar.gz``) bundling desktop entries
  and application icons.

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
  CLI polling with real-time numeric telemetry charts and interactive field
  selection (including automatic Y-axis rescaling, "Select All", "Clear All",
  and clickable canvas legend hitboxes).

- **[GUI-05]** Both GUI frontends **Shall** provide an interactive Diagnostics
  tab executing ad-hoc ``ezsnmp`` ``get``, ``getnext``, ``walk``, ``bulkget``,
  ``bulkwalk``, and ``set`` operations with tabular results display.

- **[GUI-06]** Both GUI frontends **Shall** provide an interactive Data
  Inspector tab querying and displaying recent stored telemetry rows across
  configured SQLite3, CSV, and JSONL sinks.

- **[GUI-07]** Both GUI frontends **Shall** provide an interactive Configuration
  Studio tab for loading, editing, validating, and saving RFC-8259 JSON
  configuration files.

- **[GUI-08]** All user-facing UI strings (window and header titles, button
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

- **[GUI-09]** Both GUI frontends **Shall** include custom branding icon assets
  replacing generic Python or browser tab icons, providing modern vector SVG
  favicons (``favicon.svg``), standard ICO (``favicon.ico``), multi-resolution
  PNGs (``favicon-32x32.png``, ``favicon-16x16.png``, ``apple-touch-icon.png``),
  and top bar brand badges for Browser Web Studio, and custom window titlebar
  icons (``app_icon.png`` / ``app_icon.ico``) with dynamic macOS Dock
  application icon integration via Cocoa/AppKit ``ctypes`` for Desktop Tkinter
  Studio.

- **[GUI-10]** Operating system desktop application launchers **Shall** be
  installable on macOS (``.app`` bundle with Retina ``app_icon.icns``), Linux
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
  Python SWIG bindings, ``[GEN-05]`` ``__init__.py`` purity, CalSemVer version
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

- **[FMT-01]** All hand-written C++17 files **Shall** conform strictly to
  ``.clang-format``.

- **[FMT-02]** All hand-written Python files **Shall** conform strictly to
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
  retry resilience, and skipping existing distribution artifacts idempotently.

- **[CICD-03]** Automated release publishing workflows **Shall** support
  publishing self-contained Node.js packages to npm
  (``build_and_publish_to_npm.yml``) with OIDC Trusted Publishing, Sigstore
  provenance attestations, package lock caching, multi-core parallel native
  addon compilation, idempotent skip detection, and prerelease tag resolution.

- **[CICD-04]** Automated release publishing workflows **Shall** support
  building and packaging multi-runtime C# (.NET 8) packages to NuGet and
  GitHub Packages (``build_and_publish_to_nuget.yml``) using strict C++17
  conformance (``CMAKE_CXX_EXTENSIONS=OFF``), OpenSSL include path
  discovery across Linux and macOS, multi-core MSVC (``/MP``) parallel builds,
  and OIDC Trusted Publishing authentication (``NuGet/login@v1``) with duplicate
  skipping.

- **[CICD-05]** Automated release publishing workflows **Shall** support
  generating validated WinGet community manifests in canonical directory
  hierarchies and automated pull request submission via Microsoft
  ``wingetcreate`` (``build_and_publish_to_winget.yml``,
  ``tools/generate_winget_manifest.py``; marked as FUTURE / WIP pending
  credentials setup).

- **[CICD-06]** Automated dependency maintenance **Shall** be configured via
  ``.github/dependabot.yml`` with weekly updates across GitHub Actions,
  Python (root and Sphinx docs), npm (``bindings/javascript``), NuGet
  (``bindings/csharp``), and Go (``bindings/go``), grouping package updates
  and respecting agentic workflow exclusions.

- **[CICD-07]** SonarQube Cloud automatic analysis **Shall** be configured
  via root ``.sonarcloud.properties`` targeting an A rating across security,
  reliability, and maintainability, excluding documentation (``docs/**``,
  ``sphinx_docs_build/**``), workflows (``.github/**``), vendored dependencies
  (``third_party/**``), and generated SWIG artifacts from code smells and
  copy-paste duplication metrics.

- **[CICD-08]** Continuous integration workflows **Shall** incorporate smart
  file-change detection via ``tj-actions/changed-files`` to skip redundant
  multi-platform matrix runner executions when pull requests only touch
  unrelated subsystems.

- **[CICD-09]** Continuous integration workflows **Shall** consolidate
  multi-OS conditionals using ``carlkidcrypto/os-specific-runner`` across
  Ubuntu, macOS, and Windows.

- **[CICD-10]** Python wheel packaging **Shall** utilize parallel multi-core
  compilation (via ``EZSNMP_BUILD_JOBS`` and ``ThreadPoolExecutor``), fast
  dependency isolation via ``build[uv]``, and cross-version deterministic C++
  object caching (via ``EZSNMP_OBJECT_CACHE_DIR``) across matrix runners.

- **[CICD-11]** Test deployment workflows
  (``build_and_publish_to_test_pypi.yml``) **Shall** automatically trigger
  on pushes to ``main`` guarded by smart change detection
  (``tj-actions/changed-files``) over source and packaging files, ensuring
  distribution packaging regressions fail prior to production release.

- **[CICD-12]** Desktop distribution workflows
  (``build_and_publish_desktop_apps.yml``) **Shall** package standalone Linux
  (``.deb``, ``.rpm``, ``.tar.gz``), Windows (``.exe``, ``-Setup.exe``,
  ``.zip``), and macOS (``.dmg``, ``.zip``) assets, decouple artifact
  packaging across multi-OS matrix runners from release publishing, and
  publish consolidated release assets to GitHub Releases.

- **[CICD-13]** Automated test workflows (``tests_python.yml`` and
  ``tests_cpp_native.yml``) **Shall** generate coverage reports and upload
  them to Codecov (``codecov/codecov-action``) using repository secret
  ``CODECOV_TOKEN``, tagged with dedicated flags (``python`` and ``cpp``)
  and tracked via separate components (``python_code`` and ``cpp_code``).

- **[CICD-14]** Python test suite and CLI/GUI modules **Shall** maintain
  100% statement and branch coverage across all hand-authored code, omitting
  machine-generated SWIG bindings in ``pyproject.toml`` coverage settings.
