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
 * @brief Buffered per-DUT newline-delimited JSON (.jsonl) storage sink.
 */
class JsonlSink : public ISink {
  public:
   explicit JsonlSink(SinkConfig const& cfg);
   ~JsonlSink() override;

   void write(Batch const& batch) override;
   void flush() noexcept override;
   void close() noexcept override;
   std::string name() const override { return "jsonl"; }

  private:
   std::string dir_;
   size_t flush_every_n_ = 64;

   using FileState = BufferedFileState;
   std::unordered_map<std::string, FileState> files_;
   FileState& get_or_open(std::string const& dut_name);
};

} // namespace ezsnmp_data_logger

#endif // SWIG
