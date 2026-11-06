==================
Installation Guide
==================

``ezsnmp_data_logger`` is distributed as **100% self-contained** binary wheels across **Linux, macOS, and Windows**. Users never need to install or configure external Net-SNMP, OpenSSL, or SQLite3 system libraries—everything required to poll SNMP devices and persist telemetry is bundled directly inside the package.

All releases follow the **CalSemVer** versioning scheme (e.g. ``2026.11.6``) and require Python **3.11** or newer.

.. contents:: Table of Contents
   :local:
   :depth: 2

macOS Installation
==================

Prerequisites
-------------
- macOS 11.0 (Big Sur) or newer.
- Apple Silicon (``arm64``) or Intel (``x86_64``).
- Optional: Python 3.11, 3.12, 3.13, or 3.14 (only required when using pip; standalone DMG does not require Python).

1. Standalone Disk Image (DMG) — No Python Required
---------------------------------------------------
Download the standalone disk image package directly from `GitHub Releases <https://github.com/carlkidcrypto/ezsnmp_data_logger/releases>`_:

- **Download:** ``EzSnmpDataLogger-macOS-arm64.dmg``
- Double-click the downloaded ``.dmg`` file to mount the disk image.
- Drag **EzSnmpDataLogger.app** into the **Applications** folder shortcut.
- Launch **EzSnmp Data Logger** directly from Launchpad, Spotlight, or Finder!

2. Install via pip
------------------
Install the self-contained package from PyPI (use ``--pre`` while in alpha testing):

.. code-block:: bash

   python3 -m pip install --upgrade pip
   python3 -m pip install --pre "ezsnmp-data-logger>=2026.11.6"

Verify the installation:

.. code-block:: bash

   ezsnmp-data-logger version
   # Output: ezsnmp-data-logger 2026.11.6 (ezsnmp=yes, sqlite3=yes, psql=no)

3. Install the Native macOS Application Launcher
------------------------------------------------
To launch the GUI from Finder, Spotlight, or Launchpad with a high-resolution Retina icon (replacing Python's generic rocket icon):

.. code-block:: bash

   # Installs "EzSnmp Data Logger Studio.app" into ~/Applications
   ezsnmp-data-logger-gui --install-launcher

   # Or specify a custom location (e.g. ~/Desktop)
   ezsnmp-data-logger-gui --install-launcher --launcher-dir ~/Desktop

You can now double-click **EzSnmp Data Logger Studio.app** or launch it from Spotlight!

4. Launch from the Terminal
---------------------------

.. code-block:: bash

   # Launch native Aqua Tkinter Desktop Studio
   ezsnmp-data-logger-gui --config configs/example.json

   # Or launch Browser Web Studio in Safari/Chrome
   ezsnmp-data-logger-gui --config configs/example.json --frontend web --port 8765

Windows Installation
====================

Prerequisites
-------------
- Windows 10, Windows 11, or Windows Server 2019/2022 (64-bit AMD64 or ARM64).
- Optional: Python 3.11, 3.12, 3.13, or 3.14 (only required when using pip; standalone EXE/Setup does not require Python).

1. Standalone Installer & Portable Executable — No Python Required
------------------------------------------------------------------
Download precompiled standalone binaries directly from `GitHub Releases <https://github.com/carlkidcrypto/ezsnmp_data_logger/releases>`_:

- **Graphical Setup Installer:** ``EzSnmpDataLogger-Windows-x64-Setup.exe`` (installs desktop shortcut, Start Menu launcher, and optional PATH registration).
- **Portable Single-File Executable:** ``EzSnmpDataLogger-Windows-x64.exe`` (run immediately from any folder or USB drive).
- **Portable ZIP Archive:** ``EzSnmpDataLogger-Windows-x64.zip`` or ``ezsnmp_data_logger-windows-x64.zip``.

2. Install via WinGet (Windows Package Manager)
-----------------------------------------------
Install the official Windows Package Manager package:

.. code-block:: powershell

   winget install carlkidcrypto.EzSnmpDataLogger

3. Install via pip
------------------
Open PowerShell or Command Prompt and run:

.. code-block:: powershell

   py -m pip install --upgrade pip
   py -m pip install --pre "ezsnmp-data-logger>=2026.11.6"

Verify the installation:

.. code-block:: powershell

   ezsnmp-data-logger version

4. Install Windows Desktop Shortcut
-----------------------------------
Create an executable launcher shortcut on your Desktop with the custom Windows icon:

.. code-block:: powershell

   ezsnmp-data-logger-gui --install-launcher

This creates ``EzSnmp Data Logger Studio.bat`` on your Desktop using the multi-resolution ``app_icon.ico``.

5. Launch on Windows
--------------------

.. code-block:: powershell

   # Launch native Win32 Tkinter Desktop Studio
   ezsnmp-data-logger-gui -c configs\example.json

   # Or launch Browser Web Studio in Microsoft Edge
   ezsnmp-data-logger-gui -c configs\example.json --frontend web --port 8765

Linux Installation
==================

Supported Distributions
-----------------------
- **glibc-based distributions:** Ubuntu (20.04+), Debian (11+), Fedora (38+), RHEL/CentOS (8+), Arch Linux.
- **musl-based distributions:** Alpine Linux (3.18+).
- Python 3.11, 3.12, 3.13, or 3.14.

1. Standalone Packages (.deb & .rpm) — No Python Required
---------------------------------------------------------
Pre-compiled standalone distributions bundling embedded dependencies,
command aliases, desktop menu entries, and application icons are
available from GitHub Releases:

- **Debian / Ubuntu (.deb):**

  .. code-block:: bash

     # Install via dpkg or apt
     sudo dpkg -i EzSnmpDataLogger-Linux-x86_64.deb
     # or
     sudo apt install ./EzSnmpDataLogger-Linux-x86_64.deb

- **Fedora / RHEL / CentOS / openSUSE (.rpm):**

  .. code-block:: bash

     # Install via rpm or dnf
     sudo rpm -i EzSnmpDataLogger-Linux-x86_64.rpm
     # or
     sudo dnf install ./EzSnmpDataLogger-Linux-x86_64.rpm

- **Portable Archive (.tar.gz):**

  .. code-block:: bash

     tar -xzf EzSnmpDataLogger-Linux-x86_64.tar.gz
     ./EzSnmpDataLogger cli version

2. Install via pip
------------------
On glibc systems:

.. code-block:: bash

   python3 -m pip install --upgrade pip
   python3 -m pip install --pre "ezsnmp-data-logger>=2026.11.6"

On Alpine Linux:

.. code-block:: bash

   apk add python3 py3-pip
   python3 -m pip install --upgrade pip
   python3 -m pip install --pre "ezsnmp-data-logger>=2026.11.6"

Verify the CLI:

.. code-block:: bash

   ezsnmp-data-logger version

2. Install Desktop Menu Entry (.desktop)
----------------------------------------
On desktop Linux (GNOME, KDE, XFCE):

.. code-block:: bash

   # Installs ezsnmp-data-logger.desktop into ~/.local/share/applications/
   ezsnmp-data-logger-gui --install-launcher

The application will now appear in your application launcher menu under **Development** and **System** with its custom PNG icon.

3. Headless Server & Cloud Deployment
-------------------------------------
On headless servers without an X11 or Wayland display server, ``ezsnmp-data-logger-gui`` automatically detects the absence of a display and starts the **Browser Web Studio**:

.. code-block:: bash

   # Bind to all interfaces for remote access without popping a local browser
   ezsnmp-data-logger-gui --config configs/example.json --frontend web --host 0.0.0.0 --port 8765 --no-browser

Access the studio from any browser at ``http://<server-ip>:8765``.

4. Running as a Systemd Service
-------------------------------
To run continuous SNMP polling as a background service on Linux:

Create ``/etc/systemd/system/ezsnmp-data-logger.service``:

.. code-block:: ini

   [Unit]
   Description=EzSnmp Telemetry Data Logger Service
   After=network.target

   [Service]
   Type=simple
   User=snmplogger
   ExecStart=/usr/local/bin/ezsnmp-data-logger run --config /etc/ezsnmp/logger.json
   Restart=on-failure
   RestartSec=5s

   [Install]
   WantedBy=multi-user.target

Enable and start the service:

.. code-block:: bash

   sudo systemctl daemon-reload
   sudo systemctl enable --now ezsnmp-data-logger.service
   sudo systemctl status ezsnmp-data-logger.service

Development & Source Build
==========================

If you wish to build ``ezsnmp_data_logger`` from source or contribute to its development:

Prerequisites
-------------
- A C++17 compatible compiler (GCC 9+, Clang 10+, or MSVC 2019+).
- Python 3.11+ with ``setuptools``, ``wheel``, and ``swig>=4.3.0``.
- ``meson`` and ``ninja`` (for native C++ test suites).

Clone and Build
---------------

.. code-block:: bash

   git clone https://github.com/carlkidcrypto/ezsnmp_data_logger.git
   cd ezsnmp_data_logger

   # Install editable package locally without build isolation
   python3 -m pip install -e . --no-build-isolation

   # Run Python and GUI tests
   pytest -v python_tests/

   # Run native C++ test suites (Meson + GoogleTest)
   meson setup build
   meson test -C build -v
