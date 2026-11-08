---
name: desktop-distribution
description: Use when packaging, testing, or updating standalone desktop distributions (Windows EXE/Setup, macOS DMG, Linux DEB/RPM) or Microsoft WinGet manifests for EzSnmpDataLogger.
---

# Desktop Distribution Guide (EzSnmpDataLogger)

Use this skill when developing, packaging, testing, or releasing standalone desktop applications and WinGet manifests for `EzSnmpDataLogger`.

## 1. Standard Package Naming

Per ecosystem standards, all packages, binaries, installers, and bundles are standardized on **`EzSnmpDataLogger`**:
- **Windows Executable**: `EzSnmpDataLogger-Windows-x64.exe`
- **Windows Installer**: `EzSnmpDataLogger-Windows-x64-Setup.exe`
- **macOS Disk Image**: `EzSnmpDataLogger-macOS-<arch>.dmg`
- **macOS App Bundle**: `EzSnmpDataLogger.app`
- **Linux DEB**: `EzSnmpDataLogger-Linux-<arch>.deb` / `ezsnmp-data-logger_<version>_<arch>.deb`
- **Linux RPM**: `EzSnmpDataLogger-Linux-<arch>.rpm` / `ezsnmp-data-logger-<version>-1.<arch>.rpm`
- **WinGet Identifier**: `carlkidcrypto.EzSnmpDataLogger`

---

## 2. Desktop Packaging Tools

The primary packaging script is located at `tools/package_desktop_app.py`:

```bash
# Package on current platform automatically
python tools/package_desktop_app.py

# Package targeting specific platform
python tools/package_desktop_app.py --platform windows
python tools/package_desktop_app.py --platform macos
python tools/package_desktop_app.py --platform linux
```

Generated outputs are placed into `dist/`.

---

## 3. Microsoft WinGet Manifests

Generate Microsoft WinGet YAML manifests via `tools/generate_winget_manifest.py`:

```bash
python tools/generate_winget_manifest.py \
  --version <version> \
  --installer-url "https://github.com/carlkidcrypto/ezsnmp_data_logger/releases/download/v<version>/EzSnmpDataLogger-Windows-x64-Setup.exe" \
  --installer-type inno \
  --out-dir dist-winget
```

Validates directory structure matching `manifests/c/carlkidcrypto/EzSnmpDataLogger/<version>/`.

---

## 4. Release Publishing Decoupling

In `.github/workflows/build_and_publish_desktop_apps.yml`:
- Packaging is decoupled from release publishing:
  - Multi-OS matrix (`windows-latest`, `macos-latest`, `ubuntu-latest`) packages binaries and uploads artifacts (`actions/upload-artifact@v7`).
  - Single downstream job `publish-release-assets` on `ubuntu-latest` downloads all artifacts (`actions/download-artifact@v8`) and uploads them sequentially to the GitHub Release via `gh release upload` with automatic retry.
