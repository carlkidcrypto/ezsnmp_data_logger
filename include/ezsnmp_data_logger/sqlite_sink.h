#pragma once

#ifndef SWIG

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "ezsnmp_data_logger/config.h"
#include "ezsnmp_data_logger/sink.h"

struct sqlite3;
struct sqlite3_stmt;

namespace ezsnmp_data_logger {

/**
 * @brief WAL-mode SQLite3 time-series storage sink with prepared statements and series caching.
 */
class SqliteSink : public ISink {
  public:
   explicit SqliteSink(SinkConfig const& cfg);
   ~SqliteSink() override;

   void write(Batch const& batch) override;
   void flush() override;
   void close() override;
   std::string name() const override { return "sqlite3"; }

  private:
   sqlite3* db_ = nullptr;
   int commit_every_n_ = 100;
   int commit_every_ms_ = 500;
   int batch_count_ = 0;
   bool in_txn_ = false;
   std::chrono::steady_clock::time_point last_commit_;

   std::unordered_map<std::string, int64_t> series_cache_;

   sqlite3_stmt* stmt_ins_sample_ = nullptr;
   sqlite3_stmt* stmt_sel_series_ = nullptr;
   sqlite3_stmt* stmt_ins_series_ = nullptr;

   void exec(char const* sql);
   void prepare_statements();
   void begin_txn();
   void commit_txn();
   void maybe_commit();

   int64_t upsert_series(std::string const& dut_name,
                         std::string const& dut_ip,
                         std::string const& metric_name,
                         std::string const& oid);
};

} // namespace ezsnmp_data_logger

#endif // SWIG
