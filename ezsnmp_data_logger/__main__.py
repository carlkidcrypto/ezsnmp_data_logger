"""Allow running `python -m ezsnmp_data_logger <subcommand> ...`."""

from .cli import main

if __name__ == "__main__":
    raise SystemExit(main())
