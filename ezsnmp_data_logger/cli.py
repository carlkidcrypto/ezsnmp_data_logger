"""Command-line entry point for ezsnmp_data_logger (delegates to C++17 CliRunner or GUI)."""

from __future__ import annotations

import sys

from .ezsnmp_data_logger import CliRunner, StringVector


def main(argv: list[str] | None = None) -> int:
    """Execute the ezsnmp_data_logger CLI or launch the cross-platform GUI."""
    args = list(sys.argv[1:] if argv is None else argv)
    if args and args[0] == "gui":
        from .gui import main as gui_main

        return gui_main(args[1:])
    vec = StringVector(args)
    return int(CliRunner.run(vec))


if __name__ == "__main__":
    raise SystemExit(main())
