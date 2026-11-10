#pragma once

#ifndef SWIG

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "ezsnmp_data_logger/config.h"
#include "ezsnmp_data_logger/datatypes.h"

namespace ezsnmp_data_logger {

/**
 * @brief Line-buffered file handle state for persistent file sinks (CSV, JSONL).
 */
struct BufferedFileState {
   std::ofstream file;
   std::vector<std::string> buffer;

   BufferedFileState() = default;
   BufferedFileState(BufferedFileState&& other) noexcept
       : file(std::move(other.file)), buffer(std::move(other.buffer)) {}
   BufferedFileState& operator=(BufferedFileState&& other) noexcept {
      if (this != &other) {
         file = std::move(other.file);
         buffer = std::move(other.buffer);
      }
      return *this;
   }
   BufferedFileState(BufferedFileState const&) = delete;
   BufferedFileState& operator=(BufferedFileState const&) = delete;
};

/**
 * @brief Abstract storage/telemetry output sink interface.
 */
class ISink {
  public:
   virtual ~ISink() = default;

   /**
    * @brief Write a polled Batch to this sink.
    * @param batch Timestamped batch of SNMP samples.
    */
   virtual void write(Batch const& batch) = 0;

   /**
    * @brief Flush any buffered samples to underlying storage.
    */
   virtual void flush() = 0;

   /**
    * @brief Flush and release all resources held by this sink.
    */
   virtual void close() = 0;

   /**
    * @brief Return the short type identifier of this sink (e.g. "csv", "sqlite3").
    * @return Sink type name.
    */
   virtual std::string name() const = 0;
};

/**
 * @brief Instantiate a concrete ISink from a SinkConfig.
 * @param cfg Sink configuration entry.
 * @return Unique pointer to constructed ISink.
 * @throws SinkError if sink initialization fails or support is not compiled in.
 */
std::unique_ptr<ISink> create_sink(SinkConfig const& cfg);

/**
 * @brief Query recent stored telemetry rows across configured local sinks (SQLite3, JSONL, CSV).
 * @param cfg Full LoggerConfig specifying sink paths.
 * @param limit Maximum number of most-recent rows to return.
 * @param dut_filter Optional DUT name filter (empty = all DUTs).
 * @param metric_filter Optional metric name filter (empty = all metrics).
 * @return Vector of InspectRow sorted newest-first.
 */
std::vector<InspectRow> inspect_configured_sinks(LoggerConfig const& cfg,
                                                 int limit = 50,
                                                 std::string const& dut_filter = "",
                                                 std::string const& metric_filter = "");

} // namespace ezsnmp_data_logger

#endif // SWIG
