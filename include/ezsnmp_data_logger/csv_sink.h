#pragma once

#ifndef SWIG

#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "ezsnmp_data_logger/config.h"
#include "ezsnmp_data_logger/sink.h"

namespace ezsnmp_data_logger {

/**
 * @brief Buffered RFC-4180 compliant per-DUT CSV storage sink.
 */
class CsvSink : public ISink {
  public:
   explicit CsvSink(SinkConfig const& cfg);
   ~CsvSink() override;

   void write(Batch const& batch) override;
   void flush() override;
   void close() override;
   std::string name() const override { return "csv"; }

  private:
   std::string csv_dir_;
   size_t flush_every_n_ = 64;

   using FileState = BufferedFileState;
   std::unordered_map<std::string, FileState> files_;
   FileState& get_or_open(std::string const& dut_name);
};

} // namespace ezsnmp_data_logger

#endif // SWIG
