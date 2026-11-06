#!/usr/bin/env python3
"""Package EzSnmp Data Logger Studio into standalone desktop applications.

Generates:
- macOS: Standalone `.app` bundle (`EzSnmp Data Logger Studio.app`),
  drag-and-drop disk image (`EzSnmpDataLoggerStudio-macOS-<arch>.dmg`),
  and `.zip` archive.
- Windows: Standalone single-file executable (`EzSnmpDataLoggerStudio-Windows-x64.exe`),
  Inno Setup installer (`EzSnmpDataLoggerStudio-Windows-x64-Setup.exe`),
  and portable `.zip` archives.
"""

from __future__ import annotations

import argparse
import os
import platform
import plistlib
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


def read_version() -> str:
    """Read the current project CalSemVer string from src/version.cpp or fallback."""
    version_file = REPO_ROOT / "src" / "version.cpp"
    if version_file.is_file():
        text = version_file.read_text(encoding="utf-8")
        for line in text.splitlines():
            if "PROJECT_VERSION_STRING" in line and '"' in line:
                return line.split('"')[1].strip()
    return "2026.11.6"


def resolve_platform(requested: str) -> str:
    """Resolve target platform string to 'windows', 'macos', or 'linux'."""
    val = (requested or "auto").strip().lower()
    if val in ("windows", "win", "win32"):
        return "windows"
    if val in ("macos", "darwin", "osx", "apple"):
        return "macos"
    if val in ("linux", "posix"):
        return "linux"

    if sys.platform == "win32":
        return "windows"
    if sys.platform == "darwin":
        return "macos"
    return "linux"


def build_pyinstaller_cmd(
    target_os: str,
    version: str,
    dist_dir: Path,
    build_dir: Path,
) -> list[str]:
    """Assemble the PyInstaller command invocation with all required flags and assets."""
    entry_script = REPO_ROOT / "ezsnmp_data_logger" / "entry_desktop.py"
    if not entry_script.is_file():
        raise FileNotFoundError(f"Missing desktop entry point: {entry_script}")

    sep = ";" if target_os == "windows" else ":"

    cmd: list[str] = [
        sys.executable,
        "-m",
        "PyInstaller",
        "--clean",
        "--noconfirm",
        "--distpath",
        str(dist_dir),
        "--workpath",
        str(build_dir),
        "--specpath",
        str(build_dir),
    ]

    # Data files
    static_dir = REPO_ROOT / "ezsnmp_data_logger" / "gui" / "static"
    if static_dir.is_dir():
        cmd.extend(["--add-data", f"{static_dir}{sep}ezsnmp_data_logger/gui/static"])

    locale_file = REPO_ROOT / "ezsnmp_data_logger" / "gui" / "locale.json"
    if locale_file.is_file():
        cmd.extend(["--add-data", f"{locale_file}{sep}ezsnmp_data_logger/gui"])

    config_example = REPO_ROOT / "configs" / "example.json"
    if config_example.is_file():
        cmd.extend(["--add-data", f"{config_example}{sep}configs"])

    # Hidden imports
    hidden_imports = [
        "ezsnmp",
        "ezsnmp_data_logger",
        "ezsnmp_data_logger.ezsnmp_data_logger",
        "ezsnmp_data_logger.cli",
        "ezsnmp_data_logger.gui",
        "ezsnmp_data_logger.gui.desktop_tk",
        "ezsnmp_data_logger.gui.server",
        "ezsnmp_data_logger.gui.cli_bridge",
        "ezsnmp_data_logger.gui.launcher",
        "ezsnmp_data_logger.gui.locale_manager",
        "tkinter",
        "tkinter.ttk",
        "tkinter.filedialog",
        "tkinter.messagebox",
        "sqlite3",
        "json",
        "http.server",
        "webbrowser",
        "urllib.request",
        "multiprocessing",
    ]
    for mod in hidden_imports:
        cmd.extend(["--hidden-import", mod])

    # Platform-specific packaging options
    if target_os == "macos":
        icon_icns = static_dir / "app_icon.icns"
        if icon_icns.is_file():
            cmd.extend(["--icon", str(icon_icns)])

        # Include bundled dylibs if present
        dylibs_dir = REPO_ROOT / "ezsnmp_data_logger" / ".dylibs"
        if dylibs_dir.is_dir():
            for dylib in dylibs_dir.glob("*.dylib"):
                cmd.extend(["--add-binary", f"{dylib}{sep}ezsnmp_data_logger/.dylibs"])

        # Include compiled .so if present
        for so_file in (REPO_ROOT / "ezsnmp_data_logger").glob("_ezsnmp_data_logger*.so"):
            cmd.extend(["--add-binary", f"{so_file}{sep}ezsnmp_data_logger"])

        cmd.extend(
            [
                "--onedir",
                "--windowed",
                "--name",
                "EzSnmp Data Logger Studio",
                "--osx-bundle-identifier",
                "com.carlkidcrypto.ezsnmp-data-logger-studio",
            ]
        )

    elif target_os == "windows":
        icon_ico = static_dir / "app_icon.ico"
        if icon_ico.is_file():
            cmd.extend(["--icon", str(icon_ico)])

        # Include bundled DLLs if present
        for dll_file in (REPO_ROOT / "ezsnmp_data_logger").glob("*.dll"):
            cmd.extend(["--add-binary", f"{dll_file}{sep}ezsnmp_data_logger"])

        for pyd_file in (REPO_ROOT / "ezsnmp_data_logger").glob("_ezsnmp_data_logger*.pyd"):
            cmd.extend(["--add-binary", f"{pyd_file}{sep}ezsnmp_data_logger"])

        cmd.extend(
            [
                "--onefile",
                "--name",
                "EzSnmpDataLoggerStudio",
            ]
        )

    else:
        # Linux / other
        icon_png = static_dir / "app_icon.png"
        if icon_png.is_file():
            cmd.extend(["--icon", str(icon_png)])
        cmd.extend(
            [
                "--onefile",
                "--name",
                "EzSnmpDataLoggerStudio",
            ]
        )

    cmd.append(str(entry_script))
    return cmd


def package_macos_dmg(
    dist_dir: Path,
    version: str,
    arch: str,
) -> Path:
    """Wrap EzSnmp Data Logger Studio.app into a compressed DMG with an Applications symlink."""
    app_path = dist_dir / "EzSnmp Data Logger Studio.app"
    if not app_path.is_dir():
        raise FileNotFoundError(f"Expected macOS app bundle not found at: {app_path}")

    # 1. Update Info.plist with exact version metadata
    plist_path = app_path / "Contents" / "Info.plist"
    if plist_path.is_file():
        try:
            with open(plist_path, "rb") as f:
                data = plistlib.load(f)
            data["CFBundleShortVersionString"] = version
            data["CFBundleVersion"] = version
            data["NSHighResolutionCapable"] = True
            data["LSMinimumSystemVersion"] = "11.0"
            with open(plist_path, "wb") as f:
                plistlib.dump(data, f)
        except Exception as exc:
            print(f"Warning: Could not update Info.plist: {exc}")

    # 2. Re-sign app bundle ad-hoc
    codesign = shutil.which("codesign")
    if codesign:
        subprocess.run(
            [codesign, "--force", "--deep", "--sign", "-", str(app_path)],
            check=False,
            capture_output=True,
        )

    # 3. Create zip archive of the .app
    zip_out = dist_dir / f"EzSnmpDataLoggerStudio-macOS-{arch}.zip"
    print(f"Creating macOS ZIP archive: {zip_out} ...")
    subprocess.run(
        ["zip", "-r", "-y", str(zip_out), app_path.name],
        cwd=dist_dir,
        check=True,
    )

    # 4. Prepare staging directory for DMG
    staging_dir = dist_dir / "dmg_staging"
    if staging_dir.exists():
        shutil.rmtree(staging_dir)
    staging_dir.mkdir(parents=True, exist_ok=True)

    target_app = staging_dir / app_path.name
    print(f"Copying {app_path} to {target_app} ...")
    shutil.copytree(app_path, target_app, symlinks=True)

    # Create /Applications symlink
    apps_link = staging_dir / "Applications"
    if not apps_link.exists():
        os.symlink("/Applications", str(apps_link))

    dmg_out = dist_dir / f"EzSnmpDataLoggerStudio-macOS-{arch}.dmg"
    if dmg_out.is_file():
        dmg_out.unlink()

    print(f"Creating macOS DMG disk image: {dmg_out} ...")
    subprocess.run(
        [
            "hdiutil",
            "create",
            "-volname",
            "EzSnmp Data Logger Studio",
            "-srcfolder",
            str(staging_dir),
            "-ov",
            "-format",
            "UDZO",
            str(dmg_out),
        ],
        check=True,
    )

    # Clean up staging
    shutil.rmtree(staging_dir, ignore_errors=True)
    return dmg_out


def find_iscc_compiler() -> str | None:
    """Locate the Inno Setup ISCC.exe compiler on Windows."""
    which_iscc = shutil.which("iscc")
    if which_iscc:
        return which_iscc

    standard_paths = [
        Path(r"C:\Program Files (x86)\Inno Setup 6\ISCC.exe"),
        Path(r"C:\Program Files\Inno Setup 6\ISCC.exe"),
        Path(r"C:\Program Files (x86)\Inno Setup 5\ISCC.exe"),
    ]
    for p in standard_paths:
        if p.is_file():
            return str(p)
    return None


def package_windows_artifacts(
    dist_dir: Path,
    version: str,
    arch: str,
    skip_installer: bool = False,
) -> list[Path]:
    """Generate standalone Windows executable copies, ZIP archives, and Inno Setup installer."""
    exe_src = dist_dir / "EzSnmpDataLoggerStudio.exe"
    if not exe_src.is_file():
        raise FileNotFoundError(f"Expected Windows executable not found at: {exe_src}")

    arch_exe = dist_dir / f"EzSnmpDataLoggerStudio-Windows-{arch}.exe"
    shutil.copy2(exe_src, arch_exe)
    results = [arch_exe]

    # Create portable ZIPs (both standard naming and WinGet compatibility naming)
    zip_paths = [
        dist_dir / f"ezsnmp_data_logger-windows-{arch.lower()}.zip",
        dist_dir / f"EzSnmpDataLoggerStudio-Windows-{arch}.zip",
    ]
    for zpath in zip_paths:
        print(f"Creating Windows ZIP archive: {zpath} ...")
        with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as zf:
            zf.write(exe_src, arcname="EzSnmpDataLoggerStudio.exe")
            zf.write(exe_src, arcname="ezsnmp-data-logger.exe")
            ico = REPO_ROOT / "ezsnmp_data_logger" / "gui" / "static" / "app_icon.ico"
            if ico.is_file():
                zf.write(ico, arcname="app_icon.ico")
            license_file = REPO_ROOT / "LICENSE"
            if license_file.is_file():
                zf.write(license_file, arcname="LICENSE")
            readme_file = REPO_ROOT / "README.rst"
            if readme_file.is_file():
                zf.write(readme_file, arcname="README.rst")
        results.append(zpath)

    # Inno Setup installer
    if not skip_installer:
        iscc = find_iscc_compiler()
        iss_file = REPO_ROOT / "tools" / "windows_installer.iss"
        if iscc and iss_file.is_file():
            print(f"Compiling Windows Inno Setup installer with {iscc} ...")
            cmd = [
                iscc,
                f"/DAppVersion={version}",
                f"/DSourceExe={exe_src.resolve()}",
                str(iss_file.resolve()),
            ]
            subprocess.run(cmd, check=True)
            setup_exe = dist_dir / "EzSnmpDataLoggerStudio-Windows-x64-Setup.exe"
            if setup_exe.is_file():
                results.append(setup_exe)
        else:
            print(
                "Notice: Inno Setup compiler (ISCC.exe) not found; "
                "skipping installer .exe generation (standalone .exe and .zip ready)."
            )

    return results


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Package EzSnmp Data Logger Studio for desktop distribution."
    )
    parser.add_argument(
        "--platform",
        choices=["auto", "windows", "macos", "linux"],
        default="auto",
        help="Target platform to package (defaults to current OS).",
    )
    parser.add_argument(
        "--version",
        default="",
        help="Override version string (defaults to src/version.cpp).",
    )
    parser.add_argument(
        "--dist-dir",
        type=Path,
        default=REPO_ROOT / "dist",
        help="Target output directory for distributions.",
    )
    parser.add_argument(
        "--skip-dmg",
        action="store_true",
        help="Skip macOS DMG creation.",
    )
    parser.add_argument(
        "--skip-installer",
        action="store_true",
        help="Skip Windows Inno Setup installer compilation.",
    )
    args = parser.parse_args(argv)

    target_os = resolve_platform(args.platform)
    ver = args.version or read_version()
    dist_dir = args.dist_dir.resolve()
    build_dir = REPO_ROOT / "build_pyinstaller"

    dist_dir.mkdir(parents=True, exist_ok=True)
    build_dir.mkdir(parents=True, exist_ok=True)

    print(f"Packaging EzSnmp Data Logger Studio v{ver} for [{target_os}]...")
    cmd = build_pyinstaller_cmd(target_os, ver, dist_dir, build_dir)
    print("Running PyInstaller command:")
    print(" ".join(cmd))
    subprocess.run(cmd, check=True)

    arch = platform.machine().lower()
    if arch in ("x86_64", "amd64"):
        arch = "x64"

    if target_os == "macos":
        if not args.skip_dmg:
            dmg = package_macos_dmg(dist_dir, ver, arch)
            print(f"Generated macOS DMG: {dmg}")
    elif target_os == "windows":
        artifacts = package_windows_artifacts(
            dist_dir, ver, arch, skip_installer=args.skip_installer
        )
        print("Generated Windows artifacts:")
        for a in artifacts:
            print(f"  - {a}")

    print("\nPackaging completed successfully!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
