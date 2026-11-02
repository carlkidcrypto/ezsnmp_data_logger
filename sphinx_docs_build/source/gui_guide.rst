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

All user-facing UI strings (titles, buttons, status bars, tabs, KPI cards, table headers, form labels, and dialog messages) across both the native Desktop Studio and the Browser Web Studio are externalized into ``locale.json``.

Each key in ``locale.json`` is mapped to an object containing language translations as needed:

.. code-block:: json

   {
     "buttons.start_logger": {
       "en": "Start CLI Logger",
       "es": "Iniciar Logger CLI"
     },
     "buttons.stop_logger": {
       "en": "Stop CLI Logger"
     }
   }

- **Baseline Language:** Standardizes on English (``"en"``) as the primary baseline language for all keys. Additional language translations (e.g. ``"es"``, ``"fr"``) can be added per key as needed.
- **Persistent Configuration:** Configure your preferred language directly in the JSON configuration under ``"gui": { "locale": "en" }`` (or ``"locale_file": "path/to/custom.json"``).
- **Desktop Studio Switching:** Switch between languages live via the combobox in Tab 4 (Sinks & JSON Settings).
- **Browser Studio Switching:** Switch languages instantly from the language dropdown in the top navigation toolbar, backed by ``GET /api/locale`` and ``POST /api/locale``.
- **Graceful Fallback:** Any key or translation not defined in a custom language file automatically falls back to the canonical English string.

Interactive Selectable Telemetry Series
=======================================

In Tab 1 (Live Dashboard), both Desktop Tkinter and Browser Web Studio support interactive field selection:

- **View One or Many Series:** Toggle individual series on or off by clicking their checkbuttons (Desktop Tk) or series chips/pills (Web Studio).
- **Auto-Rescaling Y-Axis:** When high-value metrics (e.g. ``sysUpTime``) are deselected, the Y-axis immediately rescales to fit smaller metrics (e.g. ``cpuLoad1m``, ``chassisTempC``) with high fidelity.
- **Quick Actions:** Use **Select All** and **Clear All** buttons to instantly manage visibility across all monitored fields.
- **Canvas Legend Interaction:** In Desktop Tkinter, clicking any legend entry directly on the chart canvas toggles its visibility.

Custom Branding Icons & Desktop Application Launchers
=====================================================

``ezsnmp_data_logger`` ships with a custom branding icon and favicon suite, ensuring neither Desktop Studio nor Web Studio display generic Python or browser tab icons:

- **Browser Web Studio Favicons:** Vector SVG (``favicon.svg``), Windows ICO (``favicon.ico``), multi-resolution PNGs (``favicon-32x32.png``, ``favicon-16x16.png``), and Apple Touch Icon (``apple-touch-icon.png``).
- **Desktop Tkinter Window & Dock Icon:** Custom window titlebar icon (``app_icon.png`` / ``app_icon.ico``) and automatic macOS Dock icon configuration via standard Cocoa/AppKit integration (zero extra dependencies).
- **Desktop Application Launcher Installer:** Install a native launcher with custom icons for your operating system:

.. code-block:: bash

   # Install native launcher (~/Applications on macOS, ~/.local/share/applications on Linux)
   ezsnmp-data-logger-gui --install-launcher

   # Or specify a custom destination directory
   ezsnmp-data-logger-gui --install-launcher --launcher-dir ~/Desktop


