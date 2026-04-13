# ezsnmp_data_logger — System Specification

## 1. Overview

`ezsnmp_data_logger` is a high-throughput SNMP data collection and storage system consisting of:

- A **C++17 daemon** (`ezsnmp_loggerd`) that receives MessagePack-encoded batches over ZeroMQ and writes them to one or more storage sinks (CSV, SQLite3, PostgreSQL).
- **Python producers** (one process per Device Under Test, DUT) that poll SNMP OIDs via the `ezsnmp` library and push batches to the daemon over a ZeroMQ PUSH/PULL pipeline.
- A **Meson + Ninja** build system for the C++ daemon.

---

## 2. Architecture

```
┌────────────────────────────┐
│  Python Orchestrator       │
│  (orchestrator.py)         │
│   spawns N DUT workers     │
└───────────┬────────────────┘
            │ fork/spawn
    ┌───────┴───────┐
    │  DUT Worker 1 │ ... DUT Worker N
    │ (dut_worker.py)│
    │  ezsnmp poll  │
    │  msgpack pack │
    └──────┬────────┘
           │ ZMQ PUSH
           ▼
┌──────────────────────────────────┐
│  ezsnmp_loggerd (C++ Daemon)     │
│  ZMQ PULL socket                 │
│  ──────────────────────────────  │
│  Recv Thread → BoundedQueue      │
│  Writer Thread ← BoundedQueue    │
│     │                            │
│     ├─► CsvSink                  │
│     ├─► SqliteSink               │
│     └─► PsqlSink                 │
└──────────────────────────────────┘
```

---

## 3. Wire Format

Batches are encoded with **MessagePack** and transmitted over ZeroMQ.

### Top-level map fields

| Key          | Type    | Description                           |
|--------------|---------|---------------------------------------|
| `ts_unix_ns` | int64   | Batch timestamp, Unix nanoseconds     |
| `dut_name`   | string  | Device name (e.g. `"core_sw_1"`)      |
| `dut_ip`     | string  | Device IP address                     |
| `samples`    | array   | Array of compact sample arrays        |

### Per-sample array (7 elements)

| Index | Field         | Type   | Description                            |
|-------|---------------|--------|----------------------------------------|
| 0     | `metric_name` | string | Human-readable metric name             |
| 1     | `oid`         | string | SNMP OID string                        |
| 2     | `type_tag`    | int    | 0=null, 1=i64, 2=f64, 3=str           |
| 3     | `i64`         | int64  | Integer value (valid when type_tag=1)  |
| 4     | `f64`         | double | Float value (valid when type_tag=2)    |
| 5     | `str_val`     | string | String value (valid when type_tag=3)   |
| 6     | `status`      | int    | SNMP status code (0 = OK)              |

---

## 4. Configuration

Both the daemon and orchestrator share the same JSON config file.

### Full schema

```json
{
  "zmq_endpoint":          "ipc:///tmp/ezsnmp_loggerd.sock",
  "zmq_hwm":               10000,
  "queue_depth":           100000,
  "poll_interval_ms":      1000,
  "process_start_method":  "spawn",
  "duts": [...],
  "sinks": [...]
}
```

### DUT entry

```json
{
  "name":   "core_sw_1",
  "dut_ip": "10.0.0.10",
  "snmp": {
    "version":     2,
    "community":   "public",
    "port_number": 161,
    "timeout":     1,
    "retries":     2
  },
  "metrics": [
    { "name": "sysUpTime", "oid": "sysUpTime.0" }
  ]
}
```

### Sink entries

#### CSV
```json
{ "type": "csv", "dir": "./data" }
```

#### SQLite3
```json
{
  "type":             "sqlite3",
  "path":             "./data/logger.sqlite",
  "commit_every_n":   100,
  "commit_every_ms":  500
}
```

#### PostgreSQL
```json
{
  "type":                 "psql",
  "conninfo":             "host=127.0.0.1 port=5432 dbname=snmp user=snmp password=snmp",
  "batch_size":           100,
  "reconnect_delay_ms":   2000
}
```

---

## 5. Storage Schema

### `series` table

| Column       | Type                    | Notes                         |
|--------------|-------------------------|-------------------------------|
| `series_id`  | INTEGER PRIMARY KEY     | SERIAL in PostgreSQL           |
| `dut_name`   | TEXT NOT NULL           |                               |
| `dut_ip`     | TEXT NOT NULL           |                               |
| `metric_name`| TEXT NOT NULL           |                               |
| `oid`        | TEXT NOT NULL           |                               |
|              | UNIQUE(dut_name, metric_name) |                          |

### `samples` table

| Column      | Type              | Notes                          |
|-------------|-------------------|--------------------------------|
| `ts_unix_ns`| INTEGER/BIGINT    | Unix nanoseconds               |
| `series_id` | INTEGER NOT NULL  | FK → series.series_id          |
| `status`    | INTEGER NOT NULL  | SNMP status code               |
| `i64`       | INTEGER (null)    | Set when type_tag = 1          |
| `f64`       | REAL (null)       | Set when type_tag = 2          |
| `str_val`   | TEXT (null)       | Set when type_tag = 3          |

Index: `(series_id, ts_unix_ns)`

### CSV format

File per DUT at `{csv_dir}/{dut_name}.csv`.

Header row:
```
ts_unix_ns,metric_name,oid,type_tag,i64,f64,str_val,status
```

---

## 6. C++ Daemon Components

### `config.h` / `config.cc`
Parses JSON configuration using `nlohmann/json`. Provides `parse_config(json_text)` returning a `Config` struct.

### `batch.h` / `batch.cc`
Defines `Sample`, `Batch`, and `TypeTag`. Implements `decode_batch(data, size)` using `msgpack-cxx`.

### `queue.h`
Template `BoundedQueue<T>`: thread-safe, bounded, non-blocking push, blocking pop. Tracks dropped item count atomically.

### `sink.h`
Abstract `ISink` interface: `write(Batch)`, `flush()`, `close()`, `name()`.

### `csv_sink.h` / `csv_sink.cc`
Buffered CSV writer. One file per DUT. Writes header on first use. Flushes buffer when it exceeds threshold or when `flush()` is called.

### `sqlite_sink.h` / `sqlite_sink.cc`
SQLite3 writer using WAL mode. Caches `series_id` per `(dut_name, metric_name)`. Batches inserts in `BEGIN IMMEDIATE` transactions. Commits every N batches or every X milliseconds.

### `psql_sink.h` / `psql_sink.cc`
PostgreSQL writer using `libpq`. Uses prepared statements. Batches inserts. Reconnects automatically on connection failure with exponential back-off (capped at 30 seconds).

### `daemon.h` / `daemon.cc`
Orchestrates all components:
- Binds ZMQ PULL socket.
- Receiver thread decodes batches and enqueues them.
- Writer thread dequeues and fans out to all sinks.
- Exposes `stats_json()` returning a JSON object with `processed_batches`, `dropped_batches`, `queue_depth`, `last_error`.

### `main.cc`
CLI entry point. Accepts `--config <file>` or `--config-string <json>`. Installs signal handlers for graceful shutdown. Prints stats to stdout every 10 seconds.

---

## 7. Python Producers

### `dut_worker.py`
Single DUT polling loop:
1. Opens a ZMQ PUSH socket and connects to the daemon endpoint.
2. On each tick: calls `ezsnmp.Session.get(oid_list)`, classifies each value, packs as MessagePack, sends with `NOBLOCK` (drops on back-pressure).
3. Sleeps for the remainder of the poll interval.
4. Handles `SIGTERM` for graceful shutdown.
5. Falls back to a random stub if `ezsnmp` is not installed.

### `orchestrator.py`
Multi-DUT orchestrator:
1. Loads config, validates metric name uniqueness per DUT.
2. Spawns one `dut_worker.run_worker` process per DUT using `multiprocessing` with configurable start method (default: `spawn`).
3. Monitors worker liveness every 5 seconds.
4. Handles `SIGINT`/`SIGTERM` by sending `terminate()` to all workers, joining them, and exiting.

---

## 8. Build System

Built with **Meson** and **Ninja** targeting C++17.

```bash
meson setup build
meson compile -C build
# Binary: build/ezsnmp_loggerd
```

### Dependencies

| Library         | Version | Provided via         |
|-----------------|---------|----------------------|
| libzmq          | 4.3.5   | pkg-config           |
| msgpack-cxx     | headers | /usr/include/msgpack.hpp |
| sqlite3         | 3.45.1  | pkg-config           |
| libpq           | 18.3    | pkg-config           |
| nlohmann/json   | headers | /usr/include/nlohmann/json.hpp |

---

## 9. Running

### Start the daemon

```bash
./build/ezsnmp_loggerd --config configs/example.json
```

### Start the Python orchestrator

```bash
python3 -m python_producers.orchestrator --config configs/example.json
```

### Stats output (every 10s on daemon stdout)

```json
{"processed_batches":1234,"dropped_batches":0,"queue_depth":0,"last_error":""}
```

---

## 10. Scalability and Reliability

- **Back-pressure**: The ZMQ HWM and `BoundedQueue` cap memory use. Dropped batches are counted and reported in stats.
- **SQLite WAL mode**: Allows concurrent reads while the writer thread commits, reducing lock contention.
- **PostgreSQL reconnection**: Exponential back-off with cap prevents thundering-herd reconnect storms.
- **Series ID caching**: `series_id` is looked up once per `(dut_name, metric_name)` pair and cached in-process, minimising per-sample DB round-trips.
- **Graceful shutdown**: SIGINT/SIGTERM flush all sinks before exit to avoid data loss.
