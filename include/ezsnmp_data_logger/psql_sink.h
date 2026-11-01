#pragma once

#ifndef SWIG

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "ezsnmp_data_logger/config.h"
#include "ezsnmp_data_logger/sink.h"

struct pg_conn;

namespace ezsnmp_data_logger {

/**
 * @brief PostgreSQL / TimescaleDB storage sink with prepared statements and auto-reconnect.
 */
class PsqlSink : public ISink {
  public:
   explicit PsqlSink(SinkConfig const& cfg);
   ~PsqlSink() override;

   void write(Batch const& batch) override;
   void flush() override;
   void close() override;
   std::string name() const override { return "psql"; }

  private:
#if defined(EZSNMP_DL_HAVE_LIBPQ) && EZSNMP_DL_HAVE_LIBPQ
   std::string conninfo_;
   int batch_size_ = 100;
   int reconnect_delay_ms_ = 2000;
   bool enable_timescaledb_ = false;
   pg_conn* conn_ = nullptr;

   std::unordered_map<std::string, int64_t> series_cache_;

   struct PendingSample {
      int64_t ts_unix_ns;
      int64_t series_id;
      std::string oid_index;
      std::string snmp_type;
      std::string raw_value;
      int status;
      TypeTag type_tag;
      int64_t i64;
      double f64;
      std::string str_val;
   };
   std::vector<PendingSample> pending_;
#endif

   bool connect_db();
   void ensure_connected();
   void exec_ddl(char const* sql);
   void prepare_statements();
   void flush_pending();

   int64_t upsert_series(std::string const& dut_name,
                         std::string const& dut_ip,
                         std::string const& metric_name,
                         std::string const& oid);
};

} // namespace ezsnmp_data_logger

#endif // SWIG
