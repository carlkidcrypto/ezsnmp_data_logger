#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ezsnmp_data_logger {

/**
 * @brief Single SNMP OID metric or SET request definition within a DUT configuration.
 */
struct MetricConfig {
   std::string name;
   std::string oid;
   std::string operation = "get";
   int max_repeaters = 10;
   std::string set_type = "s";
   std::string set_value;

#ifndef SWIG
   bool operator==(MetricConfig const& o) const {
      return name == o.name && oid == o.oid && operation == o.operation &&
             max_repeaters == o.max_repeaters && set_type == o.set_type && set_value == o.set_value;
   }
#endif
};

/**
 * @brief SNMP protocol, security, MIB, and output formatting settings mapped directly
 * to ezsnmp's C++17 `SessionBase` constructor parameters.
 */
struct SnmpConfig {
   std::string mode = "ezsnmp";
   std::string version = "2c";
   std::string community = "public";
   int port_number = 161;
   int timeout_seconds = 1;
   int retries = 2;

   // SNMPv3 USM parameters (mapped to ezsnmp SessionBase)
   std::string security_level = "noAuthNoPriv";
   std::string security_username;
   std::string auth_protocol = "SHA-256";
   std::string auth_passphrase;
   std::string privacy_protocol = "AES";
   std::string privacy_passphrase;
   std::string context;
   std::string security_engine_id;
   std::string context_engine_id;
   std::string boots_time;

   // ezsnmp SessionBase MIB & formatting options
   std::string load_mibs;
   std::string mib_directories;
   bool print_enums_numerically = false;
   bool print_full_oids = false;
   bool print_oids_numerically = false;
   bool print_timeticks_numerically = true;
   bool print_hex_strings = false;
};

/**
 * @brief Configuration for a single Device Under Test (DUT).
 */
struct DutConfig {
   std::string name;
   std::string dut_ip;
   bool enabled = true;
   int poll_interval_ms = 0;
   SnmpConfig snmp;
   std::vector<MetricConfig> metrics;

#ifndef SWIG
   bool operator==(DutConfig const& o) const {
      return name == o.name && dut_ip == o.dut_ip && enabled == o.enabled &&
             poll_interval_ms == o.poll_interval_ms && metrics == o.metrics;
   }
#endif
};

/**
 * @brief Configuration for a single telemetry output sink.
 */
struct SinkConfig {
   std::string type;
   bool enabled = true;

   // CSV & JSONL settings
   std::string dir = "./data";
   int flush_every_n = 64;

   // SQLite3 settings
   std::string sqlite_path = "./data/logger.sqlite";
   int sqlite_commit_every_n = 100;
   int sqlite_commit_every_ms = 500;

   // PostgreSQL settings
   std::string psql_conninfo;
   int psql_batch_size = 100;
   int psql_reconnect_delay_ms = 2000;
   bool psql_enable_timescaledb = false;

   // Prometheus HTTP exporter settings
   std::string prometheus_host = "127.0.0.1";
   int prometheus_port = 9761;
   std::string prometheus_prefix = "ezsnmp";

#ifndef SWIG
   bool operator==(SinkConfig const& o) const {
      return type == o.type && enabled == o.enabled && dir == o.dir &&
             sqlite_path == o.sqlite_path && psql_conninfo == o.psql_conninfo &&
             prometheus_port == o.prometheus_port;
   }
#endif
};

/**
 * @brief Cross-platform GUI settings stored inside JSON configuration files.
 *
 * Supports native cross-platform desktop (`tkinter.ttk` on Linux, macOS, Windows)
 * and local browser studio (`web` on Linux, macOS, Windows).
 */
struct GuiConfig {
   std::string frontend = "auto";
   std::string host = "127.0.0.1";
   int port = 8765;
   std::string theme = "dark";
   int window_width = 1280;
   int window_height = 820;
   int refresh_interval_ms = 1000;
   bool open_browser = true;
   std::string cli_executable;
   int max_log_lines = 500;
};

/**
 * @brief Engine-level runtime scheduling, queueing, and lifecycle parameters.
 */
struct LoggerRuntimeConfig {
   int poll_interval_ms = 1000;
   int queue_depth = 100000;
   int worker_threads = 4;
   int stats_interval_ms = 2000;
   int max_cycles = 0;
   int run_duration_seconds = 0;
   std::string log_level = "info";
   std::string status_file;

   // Optional legacy ZeroMQ compatibility fields
   std::string zmq_endpoint = "ipc:///tmp/ezsnmp_loggerd.sock";
   int zmq_hwm = 10000;
   std::string process_start_method = "spawn";
};

/**
 * @brief Complete unified JSON configuration for ezsnmp_data_logger.
 */
struct LoggerConfig {
   LoggerRuntimeConfig logger;
   std::vector<DutConfig> duts;
   std::vector<SinkConfig> sinks;
   GuiConfig gui;

   /**
    * @brief Serialize the complete configuration back to JSON text.
    */
   std::string to_json(int indent = 2) const;

   /**
    * @brief Parse a LoggerConfig from a JSON string.
    */
   static LoggerConfig from_json(std::string const& json_text);

   /**
    * @brief Load and parse a LoggerConfig from a JSON file path.
    */
   static LoggerConfig from_file(std::string const& file_path);
};

/**
 * @brief Structured result of validating a JSON configuration.
 */
struct ValidationReport {
   bool valid = false;
   std::vector<std::string> errors;
   std::vector<std::string> warnings;
   int enabled_duts = 0;
   int total_metrics = 0;
   int enabled_sinks = 0;

   /**
    * @brief Serialize this validation report to a JSON string.
    */
   std::string to_json(int indent = -1) const;
};

LoggerConfig parse_config(std::string const& json_text);
LoggerConfig parse_config_file(std::string const& file_path);
ValidationReport validate_config(std::string const& json_text);
ValidationReport validate_config_file(std::string const& file_path);
std::string generate_template_config_json(std::string const& template_name = "minimal",
                                          int indent = 2);
std::string config_json_schema(int indent = 2);

} // namespace ezsnmp_data_logger
