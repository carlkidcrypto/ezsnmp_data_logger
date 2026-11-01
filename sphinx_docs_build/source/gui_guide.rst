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
