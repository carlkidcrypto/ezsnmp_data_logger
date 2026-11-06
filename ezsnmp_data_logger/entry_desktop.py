"""Unified desktop application entry point for frozen bundles (Windows .exe and macOS .app).

Dispatches intelligently:
- Without arguments or with GUI-specific arguments: launches the graphical user interface.
- With 'cli' as the first argument: strips 'cli' and dispatches to the CLI runner.
- With a known CLI subcommand (probe, request, set, run, inspect, validate, schema, version, init-config): dispatches to the CLI runner.
- Supports multiprocessing.freeze_support() for Windows execution.
"""

from __future__ import annotations

import multiprocessing
import sys
from typing import Sequence

CLI_SUBCOMMANDS = {
    "run",
    "validate",
    "probe",
    "request",
    "set",
    "inspect",
    "init-config",
    "schema",
    "version",
    "help",
    "--help",
    "-h",
    "--version",
    "-v",
}


def main(argv: Sequence[str] | None = None) -> int:
    """Unified entry point for standalone desktop applications and frozen binaries."""
    multiprocessing.freeze_support()

    args = list(sys.argv[1:] if argv is None else argv)

    # Filter out macOS Finder process serial number argument (e.g. -psn_0_123456)
    args = [arg for arg in args if not arg.startswith("-psn_")]

    if not args:
        from ezsnmp_data_logger.gui import main as gui_main

        return gui_main([])

    first = args[0].strip().lower()

    if first == "cli":
        from ezsnmp_data_logger.cli import main as cli_main

        return cli_main(args[1:])

    if first in CLI_SUBCOMMANDS:
        from ezsnmp_data_logger.cli import main as cli_main

        return cli_main(args)

    if first == "gui":
        from ezsnmp_data_logger.gui import main as gui_main

        return gui_main(args[1:])

    # Default to GUI with arguments (e.g. --frontend tk, -c config.json)
    from ezsnmp_data_logger.gui import main as gui_main

    return gui_main(args)


if __name__ == "__main__":
    sys.exit(main())
