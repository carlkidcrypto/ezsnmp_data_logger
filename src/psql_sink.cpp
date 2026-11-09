#include "ezsnmp_data_logger/psql_sink.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

#include "ezsnmp_data_logger/exceptions.h"

#if defined(EZSNMP_DL_HAVE_LIBPQ) && EZSNMP_DL_HAVE_LIBPQ
#include <libpq-fe.h>
#endif

namespace ezsnmp_data_logger {

#if defined(EZSNMP_DL_HAVE_LIBPQ) && EZSNMP_DL_HAVE_LIBPQ

PsqlSink::PsqlSink(SinkConfig const& cfg)
    : conninfo_(cfg.psql_conninfo),
      batch_size_(cfg.psql_batch_size > 0 ? cfg.psql_batch_size : 100),
      reconnect_delay_ms_(cfg.psql_reconnect_delay_ms > 0 ? cfg.psql_reconnect_delay_ms : 2000),
      enable_timescaledb_(cfg.psql_enable_timescaledb) {
   if (conninfo_.empty()) {
      throw SinkError("PsqlSink: psql_conninfo must not be empty");
   }
   if (conninfo_ == "stub_for_testing") {
      return;
   }
   if (!connect_db()) {
      throw SinkError("PsqlSink: failed initial PostgreSQL connection");
   }
}

PsqlSink::~PsqlSink() { close(); }

bool PsqlSink::connect_db() {
   if (conn_) {
      PQfinish(conn_);
      conn_ = nullptr;
   }
   conn_ = PQconnectdb(conninfo_.c_str());
   if (PQstatus(conn_) != CONNECTION_OK) {
      PQfinish(conn_);
      conn_ = nullptr;
      return false;
   }

   exec_ddl(
       "CREATE TABLE IF NOT EXISTS series ("
       "  series_id   BIGSERIAL PRIMARY KEY,"
       "  dut_name    TEXT NOT NULL,"
       "  dut_ip      TEXT NOT NULL,"
       "  metric_name TEXT NOT NULL,"
       "  oid         TEXT NOT NULL,"
       "  UNIQUE(dut_name, metric_name, oid)"
       ");");

   exec_ddl(
       "CREATE TABLE IF NOT EXISTS samples ("
       "  ts_unix_ns  BIGINT NOT NULL,"
       "  series_id   BIGINT NOT NULL REFERENCES series(series_id),"
       "  oid_index   TEXT NOT NULL DEFAULT '',"
       "  snmp_type   TEXT NOT NULL DEFAULT '',"
       "  raw_value   TEXT NOT NULL DEFAULT '',"
       "  type_tag    SMALLINT NOT NULL,"
       "  i64         BIGINT,"
       "  f64         DOUBLE PRECISION,"
       "  str_val     TEXT,"
       "  status      INTEGER NOT NULL"
       ");");

   exec_ddl(
       "CREATE INDEX IF NOT EXISTS idx_samples_series_ts ON samples(series_id, ts_unix_ns DESC);");

   if (enable_timescaledb_) {
      exec_ddl(
          "SELECT create_hypertable('samples', 'ts_unix_ns', if_not_exists => TRUE, "
          "chunk_time_interval => 86400000000000::BIGINT);");
   }

   prepare_statements();
   return true;
}

void PsqlSink::ensure_connected() {
   if (conn_ && PQstatus(conn_) == CONNECTION_OK) {
      return;
   }
   std::this_thread::sleep_for(std::chrono::milliseconds(reconnect_delay_ms_));
   if (!connect_db()) {
      throw SinkError("PsqlSink: PostgreSQL reconnect failed");
   }
}

void PsqlSink::exec_ddl(char const* sql) {
   PGresult* res = PQexec(conn_, sql);
   ExecStatusType st = PQresultStatus(res);
   if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
      std::string err = PQerrorMessage(conn_);
      PQclear(res);
      throw SinkError("PsqlSink DDL error: " + err);
   }
   PQclear(res);
}

void PsqlSink::prepare_statements() {
   PGresult* r1 = PQprepare(conn_, "upsert_series",
                            "INSERT INTO series(dut_name, dut_ip, metric_name, oid) "
                            "VALUES($1, $2, $3, $4) "
                            "ON CONFLICT(dut_name, metric_name, oid) "
                            "DO UPDATE SET dut_ip = EXCLUDED.dut_ip "
                            "RETURNING series_id;",
                            4, nullptr);
   if (PQresultStatus(r1) != PGRES_COMMAND_OK) {
      std::string err = PQerrorMessage(conn_);
      PQclear(r1);
      throw SinkError("PsqlSink prepare upsert_series failed: " + err);
   }
   PQclear(r1);

   PGresult* r2 = PQprepare(conn_, "insert_sample",
                            "INSERT INTO samples(ts_unix_ns, series_id, oid_index, snmp_type, "
                            "raw_value, type_tag, i64, f64, str_val, status) "
                            "VALUES($1, $2, $3, $4, $5, $6, $7, $8, $9, $10);",
                            10, nullptr);
   if (PQresultStatus(r2) != PGRES_COMMAND_OK) {
      std::string err = PQerrorMessage(conn_);
      PQclear(r2);
      throw SinkError("PsqlSink prepare insert_sample failed: " + err);
   }
   PQclear(r2);
}

int64_t PsqlSink::upsert_series(std::string const& dut_name,
                                std::string const& dut_ip,
                                std::string const& metric_name,
                                std::string const& oid) {
   std::string key = dut_name + "\x1f" + metric_name + "\x1f" + oid;
   auto it = series_cache_.find(key);
   if (it != series_cache_.end()) {
      return it->second;
   }

   ensure_connected();
   char const* values[4] = {dut_name.c_str(), dut_ip.c_str(), metric_name.c_str(), oid.c_str()};
   PGresult* res = PQexecPrepared(conn_, "upsert_series", 4, values, nullptr, nullptr, 0);
   if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) < 1) {
      std::string err = PQerrorMessage(conn_);
      PQclear(res);
      throw SinkError("PsqlSink upsert_series failed: " + err);
   }
   int64_t sid = std::stoll(PQgetvalue(res, 0, 0));
   PQclear(res);
   series_cache_[key] = sid;
   return sid;
}

void PsqlSink::flush_pending() {
   if (pending_.empty()) {
      return;
   }
   ensure_connected();
   exec_ddl("BEGIN;");
   try {
      for (auto const& p : pending_) {
         std::string ts_s = std::to_string(p.ts_unix_ns);
         std::string sid_s = std::to_string(p.series_id);
         std::string tag_s = std::to_string(static_cast<int>(p.type_tag));
         std::string i64_s = std::to_string(p.i64);
         std::ostringstream f64_ss;
         f64_ss << std::setprecision(15) << p.f64;
         std::string f64_s = f64_ss.str();
         std::string st_s = std::to_string(p.status);

         char const* vals[10] = {ts_s.c_str(),
                                 sid_s.c_str(),
                                 p.oid_index.c_str(),
                                 p.snmp_type.c_str(),
                                 p.raw_value.c_str(),
                                 tag_s.c_str(),
                                 p.type_tag == TypeTag::Int64 ? i64_s.c_str() : nullptr,
                                 p.type_tag == TypeTag::Float64 ? f64_s.c_str() : nullptr,
                                 p.type_tag == TypeTag::Str ? p.str_val.c_str() : nullptr,
                                 st_s.c_str()};

         PGresult* res = PQexecPrepared(conn_, "insert_sample", 10, vals, nullptr, nullptr, 0);
         if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            std::string err = PQerrorMessage(conn_);
            PQclear(res);
            throw SinkError("PsqlSink insert_sample failed: " + err);
         }
         PQclear(res);
      }
      exec_ddl("COMMIT;");
      pending_.clear();
   } catch (...) {
      try {
         exec_ddl("ROLLBACK;");
      } catch (...) {
      }
      throw;
   }
}

void PsqlSink::write(Batch const& batch) {
   for (auto const& s : batch.samples) {
      int64_t sid = upsert_series(batch.dut_name, batch.dut_ip, s.metric_name, s.oid);
      PendingSample ps;
      ps.ts_unix_ns = batch.ts_unix_ns;
      ps.series_id = sid;
      ps.oid_index = s.oid_index;
      ps.snmp_type = s.snmp_type;
      ps.raw_value = s.raw_value;
      ps.status = s.status;
      ps.type_tag = s.type_tag;
      ps.i64 = s.i64;
      ps.f64 = s.f64;
      ps.str_val = s.str_val;
      pending_.push_back(std::move(ps));
   }
   if (static_cast<int>(pending_.size()) >= batch_size_) {
      flush_pending();
   }
}

void PsqlSink::flush() noexcept {
   try {
      flush_pending();
   } catch (...) {
   }
}

void PsqlSink::close() noexcept {
   flush();
   if (conn_) {
      PQfinish(conn_);
      conn_ = nullptr;
   }
}

#else

PsqlSink::PsqlSink(SinkConfig const& cfg) {
   if (cfg.psql_conninfo == "stub_for_testing") {
      return;
   }
   throw SinkError(
       "PostgreSQL (libpq) support was not compiled into ezsnmp_data_logger. "
       "Install libpq and rebuild with -Dezsnmp_dl_psql=enabled or use sqlite3/csv/jsonl sinks.");
}

PsqlSink::~PsqlSink() = default;
void PsqlSink::write(Batch const& /*batch*/) {
   flush_pending();
   upsert_series("", "", "", "");
}
void PsqlSink::flush() noexcept { flush_pending(); }
void PsqlSink::close() noexcept {}
bool PsqlSink::connect_db() { return false; }
void PsqlSink::ensure_connected() {}
void PsqlSink::exec_ddl(char const* /*sql*/) {}
void PsqlSink::prepare_statements() {}
void PsqlSink::flush_pending() {}
int64_t PsqlSink::upsert_series(std::string const&,
                                std::string const&,
                                std::string const&,
                                std::string const&) {
   return 0;
}

#endif // EZSNMP_DL_HAVE_LIBPQ

} // namespace ezsnmp_data_logger
