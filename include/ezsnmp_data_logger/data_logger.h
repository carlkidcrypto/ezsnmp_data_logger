#pragma once

#include "ezsnmp_data_logger/config.h"
#include "ezsnmp_data_logger/datatypes.h"
#ifndef SWIG
#include <functional>
#endif
#include <memory>
#include <string>
#include <vector>

namespace ezsnmp_data_logger {

/**
 * @brief High-throughput C++17 SNMP Data Logger engine.
 *
 * Orchestrates concurrent SNMP polling across configured Devices Under Test (DUTs),
 * buffers collected Batch objects through a bounded lock-protected queue, and fans
 * telemetry out to CSV, JSONL, SQLite3, PostgreSQL, and Prometheus HTTP sinks.
 *
 * Designed with a clean PIMPL boundary so SWIG 4.x can expose DataLogger directly
 * to Python, JavaScript/Node.js, Java, C#, Go, Ruby, Perl, PHP, Lua, R, Tcl,
 * Octave, D, Guile, and Scilab.
 */
class DataLogger {
  public:
   /**
    * @brief Construct a DataLogger from a parsed LoggerConfig struct.
    * @param config Validated LoggerConfig.
    * @throws ConfigError or SinkError on invalid configuration or sink setup failure.
    */
   explicit DataLogger(LoggerConfig const& config);

   /**
    * @brief Construct a DataLogger directly from a JSON configuration string.
    * @param json_config Complete JSON configuration document.
    * @throws ConfigError or SinkError on invalid JSON or sink setup failure.
    */
   explicit DataLogger(std::string const& json_config);

   /**
    * @brief Create a DataLogger by reading a JSON configuration file from disk.
    * @param file_path Path to the JSON configuration file.
    * @return Configured DataLogger instance (by move/RVO or pointer-safe wrapper).
    */
   static DataLogger from_file(std::string const& file_path);

   ~DataLogger();
#ifndef SWIG
   DataLogger(DataLogger const& other);
   DataLogger& operator=(DataLogger const& other);
   DataLogger(DataLogger&& other) noexcept;
   DataLogger& operator=(DataLogger&& other) noexcept;

   /**
    * @brief Register an optional C++ observer invoked on the writer thread after each Batch is
    * persisted (used by CLI `--json` live event streaming).
    */
   void set_batch_callback(std::function<void(Batch const&)> cb);
#endif

   /**
    * @brief Start background DUT polling threads and sink writer thread (non-blocking).
    */
   void start();

   /**
    * @brief Signal all polling threads to stop, drain the bounded queue, and flush sinks.
    */
   void stop() noexcept;

   /**
    * @brief Run the logger synchronously in the foreground until stop(), max_cycles, or duration.
    */
   void run();

   /**
    * @brief Execute exactly one polling cycle across all enabled DUTs, write to sinks, and flush.
    * @return Vector of Batch objects collected during this single cycle.
    */
   std::vector<Batch> run_once();

   /**
    * @brief Check whether background polling/writing threads are currently active.
    * @return true if running, false otherwise.
    */
   bool is_running() const;

   /**
    * @brief Manually push a custom Batch into the writer queue (enables external language
    * producers).
    * @param batch Batch to enqueue for storage sinks.
    * @return true if enqueued, false if dropped due to queue back-pressure.
    */
   bool push_batch(Batch const& batch);

   /**
    * @brief Parse a Batch from JSON text and enqueue it for storage sinks.
    * @param batch_json JSON string representing a Batch.
    * @return true if enqueued, false if dropped due to queue back-pressure.
    */
   bool push_batch_json(std::string const& batch_json);

   /**
    * @brief Flush all active storage sinks immediately.
    */
   void flush();

   /**
    * @brief Return structured operational telemetry statistics.
    * @return LoggerStats snapshot.
    */
   LoggerStats stats() const;

   /**
    * @brief Return operational telemetry statistics serialized as a JSON string.
    * @param indent Indentation spaces (-1 for compact single-line JSON).
    * @return JSON stats string.
    */
   std::string stats_json(int indent = -1) const;

   /**
    * @brief Return the active configuration serialized as JSON text.
    * @param indent Indentation spaces (default 2).
    * @return JSON configuration string.
    */
   std::string config_json(int indent = 2) const;

   /**
    * @brief Probe configured DUTs once without writing to any storage sink.
    * @param config LoggerConfig containing DUT definitions.
    * @param dut_filter Optional single DUT name to probe (empty = all enabled DUTs).
    * @return Vector of DutProbeResult objects with live samples and latency.
    */
   static std::vector<DutProbeResult> probe(LoggerConfig const& config,
                                            std::string const& dut_filter = "");

   /**
    * @brief Probe configured DUTs from JSON configuration text and return a JSON report.
    * @param json_config JSON configuration text.
    * @param dut_filter Optional single DUT name to probe (empty = all enabled DUTs).
    * @param indent Indentation spaces (-1 for compact).
    * @return JSON report object containing per-DUT probe results.
    */
   static std::string probe_json(std::string const& json_config,
                                 std::string const& dut_filter = "",
                                 int indent = -1);

   /**
    * @brief Execute an on-demand SNMP request (`get`, `getnext`, `walk`, `bulkget`, `bulkwalk`)
    * against a configured DUT via ezsnmp's `SessionBase` and optionally persist results to sinks.
    * @param json_config JSON configuration text.
    * @param dut_name Target DUT name from configuration.
    * @param operation SNMP operation (`get`, `getnext`, `walk`, `bulkget`, `bulkwalk`).
    * @param oids Vector of OID strings to query.
    * @param max_repeaters Max repeaters for bulk operations (default 10).
    * @param record_to_sinks If true, also write the returned Batch to all enabled sinks.
    * @param indent Indentation spaces (-1 for compact).
    * @return JSON report containing returned samples and latency.
    */
   static std::string request_json(std::string const& json_config,
                                   std::string const& dut_name,
                                   std::string const& operation,
                                   std::vector<std::string> const& oids,
                                   int max_repeaters = 10,
                                   bool record_to_sinks = false,
                                   int indent = -1);

   /**
    * @brief Execute an on-demand SNMP SET operation against a configured DUT via ezsnmp's
    * `SessionBase::set` and optionally persist the response to configured sinks.
    * @param json_config JSON configuration text.
    * @param dut_name Target DUT name from configuration.
    * @param oid Target OID string.
    * @param set_type Net-SNMP / ezsnmp type character (`s`, `i`, `u`, `t`, `a`, `o`, `x`).
    * @param set_value Value string to write.
    * @param record_to_sinks If true, also write the SET response Batch to all enabled sinks.
    * @param indent Indentation spaces (-1 for compact).
    * @return JSON report containing SET response sample(s) and latency.
    */
   static std::string set_json(std::string const& json_config,
                               std::string const& dut_name,
                               std::string const& oid,
                               std::string const& set_type,
                               std::string const& set_value,
                               bool record_to_sinks = false,
                               int indent = -1);

   /**
    * @brief Inspect recent logged samples from configured local sinks and return JSON.
    * @param json_config JSON configuration text.
    * @param limit Maximum rows to return (default 50).
    * @param dut_filter Optional DUT name filter.
    * @param metric_filter Optional metric name filter.
    * @param indent Indentation spaces (-1 for compact).
    * @return JSON report object with inspected telemetry rows.
    */
   static std::string inspect_json(std::string const& json_config,
                                   int limit = 50,
                                   std::string const& dut_filter = "",
                                   std::string const& metric_filter = "",
                                   int indent = -1);

  private:
#ifndef SWIG
   class Impl;
   std::shared_ptr<Impl> impl_;
#endif
};

} // namespace ezsnmp_data_logger
