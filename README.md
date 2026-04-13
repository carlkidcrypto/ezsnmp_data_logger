# ezsnmp_data_logger

A high-throughput SNMP data collection and storage system.

## Components

- **`ezsnmp_loggerd`** — C++17 daemon that receives MessagePack batches over ZeroMQ and writes to CSV, SQLite3, and PostgreSQL sinks.
- **Python producers** — One process per Device Under Test (DUT), polling SNMP via `ezsnmp` and pushing data to the daemon.

## Quick Start

### Build the daemon

```bash
# Install system dependencies (Debian/Ubuntu)
sudo apt-get install -y libzmq3-dev libmsgpack-dev libsqlite3-dev libpq-dev nlohmann-json3-dev

# Build
meson setup build
meson compile -C build

# Run
./build/ezsnmp_loggerd --config configs/example.json
```

### Run the Python producers

```bash
pip install -r requirements.txt
python3 -m python_producers.orchestrator --config configs/example.json
```

## Documentation

See [docs/system_spec.md](docs/system_spec.md) for full system specification, wire format, storage schema, and configuration reference.
