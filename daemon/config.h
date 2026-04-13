#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace ezsnmp_logger {

struct SinkConfig {
    std::string type;           // "csv", "sqlite3", "psql"
    // CSV
    std::string csv_dir;
    // SQLite
    std::string sqlite_path;
    int sqlite_commit_every_n = 100;
    int sqlite_commit_every_ms = 500;
    // PostgreSQL
    std::string psql_conninfo;
    int psql_batch_size = 100;
    int psql_reconnect_delay_ms = 2000;
};

struct Config {
    std::string zmq_endpoint = "ipc:///tmp/ezsnmp_loggerd.sock";
    int zmq_hwm = 10000;
    int queue_depth = 100000;
    std::vector<SinkConfig> sinks;
};

Config parse_config(const std::string& json_text);

} // namespace ezsnmp_logger
