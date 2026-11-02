=============================
Cross-Platform GUI User Guide
=============================

``ezsnmp_data_logger`` provides a cross-platform Graphical User Interface across **Linux, macOS, and Windows** that interacts **strictly with the CLI behind the scenes** via JSON.

Behind-the-Scenes CLI Architecture
==================================

Every button click in the GUI invokes ``CliSubprocessBridge``, which spawns ``ezsnmp-data-logger <subcommand> --json`` as a child CLI process, parses its JSON output (or live ``run --json`` event stream), and records the exact command, exit code, and latency in the **Behind-the-Scenes CLI Command Audit Trail**.

Two Cross-Platform Frontends (Linux, macOS, Windows)
====================================================

Configured via ``"gui": { "frontend": "auto" | "tk" | "web" }`` in your JSON file (or ``--frontend`` on the command line):

1. **Native Desktop Studio (**``tk``**):** Built on Python's cross-platform standard-library ``tkinter.ttk`` (Linux X11/Wayland, macOS Aqua, Windows Win32) with dark and light themes, real-time ``Canvas`` multi-series charts, DUT/SNMPv3 editor, EzSnmp GET/WALK/SET workbench, Data Explorer, and Raw JSON editor.
2. **Browser Web Studio (**``web``**):** Built on Python's standard-library ``ThreadingHTTPServer`` + responsive HTML5/SVG, ideal for headless servers, SSH port-forwarding, Docker containers, WSL2, or Python builds without Tcl/Tk.

Launching the GUI
=================

.. code-block:: bash

   # Auto-selects native Desktop Tk window when a display is present, else Web Studio
   ezsnmp-data-logger gui --config configs/example.json

   # Force native cross-platform tkinter.ttk Desktop Studio
   ezsnmp-data-logger-gui --config configs/example.json --frontend tk

   # Force cross-platform Browser Web Studio on http://127.0.0.1:8765
   ezsnmp-data-logger-gui --config configs/example.json --frontend web --port 8765

   # Launch in Spanish using the built-in 'es' catalog
   ezsnmp-data-logger-gui --config configs/example.json --locale es

   # Launch using a custom translation locale.json file
   ezsnmp-data-logger-gui --config configs/example.json --locale-file /path/to/my_locale.json

Multi-Language Localization (``locale.json``)
=============================================

All user-facing UI strings (titles, buttons, status bars, tabs, KPI cards, table headers, form labels, and dialog messages) across both the native Desktop Studio and the Browser Web Studio are externalized into ``locale.json``:

- **Built-in Catalogs:** Comes pre-packaged with English (``"en"``) and Spanish (``"es"``).
- **Persistent Configuration:** Configure your preferred language directly in the JSON configuration under ``"gui": { "locale": "es" }`` (or ``"locale_file": "path/to/custom.json"``).
- **Desktop Studio Switching:** Switch between languages live via the combobox in Tab 4 (Sinks & JSON Settings).
- **Browser Studio Switching:** Switch languages instantly from the language dropdown in the top navigation toolbar, backed by ``GET /api/locale`` and ``POST /api/locale``.
- **Graceful Fallback:** Any key not defined in a custom language file automatically falls back to the canonical English string.

