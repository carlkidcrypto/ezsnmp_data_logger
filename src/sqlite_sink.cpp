#include "ezsnmp_data_logger/sqlite_sink.h"

#include <filesystem>

#include "ezsnmp_data_logger/exceptions.h"

#if !defined(EZSNMP_DL_HAVE_SQLITE3)
#define EZSNMP_DL_HAVE_SQLITE3 1
#endif

#if EZSNMP_DL_HAVE_SQLITE3
#include <sqlite3.h>
#endif

namespace ezsnmp_data_logger {

#if EZSNMP_DL_HAVE_SQLITE3

SqliteSink::SqliteSink(SinkConfig const& cfg)
    : commit_every_n_(cfg.sqlite_commit_every_n > 0 ? cfg.sqlite_commit_every_n : 100),
      commit_every_ms_(cfg.sqlite_commit_every_ms > 0 ? cfg.sqlite_commit_every_ms : 500),
      last_commit_(std::chrono::steady_clock::now()) {
   if (cfg.sqlite_path != ":memory:") {
      std::filesystem::path p(cfg.sqlite_path);
      if (p.has_parent_path() && !p.parent_path().empty()) {
         std::error_code ec;
         std::filesystem::create_directories(p.parent_path(), ec);
         if (ec) {
            throw SinkError("SqliteSink: failed to create parent directory for '" +
                            cfg.sqlite_path + "': " + ec.message());
         }
      }
   }

   int rc =
       sqlite3_open_v2(cfg.sqlite_path.c_str(), &db_,
                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
   if (rc != SQLITE_OK) {
      std::string err = db_ ? sqlite3_errmsg(db_) : "unknown error";
      if (db_) {
         sqlite3_close(db_);
         db_ = nullptr;
      }
      throw SinkError("SqliteSink: cannot open '" + cfg.sqlite_path + "': " + err);
   }

   exec("PRAGMA journal_mode = WAL;");
   exec("PRAGMA synchronous = NORMAL;");
   exec("PRAGMA temp_store = MEMORY;");
   exec("PRAGMA busy_timeout = 5000;");

   exec(
       "CREATE TABLE IF NOT EXISTS series ("
       "  series_id   INTEGER PRIMARY KEY AUTOINCREMENT,"
       "  dut_name    TEXT NOT NULL,"
       "  dut_ip      TEXT NOT NULL,"
       "  metric_name TEXT NOT NULL,"
       "  oid         TEXT NOT NULL,"
       "  UNIQUE(dut_name, metric_name, oid)"
       ");");

   exec(
       "CREATE TABLE IF NOT EXISTS samples ("
       "  ts_unix_ns  INTEGER NOT NULL,"
       "  series_id   INTEGER NOT NULL,"
       "  oid_index   TEXT NOT NULL DEFAULT '',"
       "  snmp_type   TEXT NOT NULL DEFAULT '',"
       "  raw_value   TEXT NOT NULL DEFAULT '',"
       "  type_tag    INTEGER NOT NULL,"
       "  i64         INTEGER,"
       "  f64         REAL,"
       "  str_val     TEXT,"
       "  status      INTEGER NOT NULL,"
       "  FOREIGN KEY(series_id) REFERENCES series(series_id)"
       ");");

   exec("CREATE INDEX IF NOT EXISTS idx_samples_series_ts ON samples(series_id, ts_unix_ns DESC);");
   exec("CREATE INDEX IF NOT EXISTS idx_samples_ts ON samples(ts_unix_ns DESC);");

   prepare_statements();
}

SqliteSink::~SqliteSink() { close(); }

void SqliteSink::exec(char const* sql) {
   char* errmsg = nullptr;
   int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg);
   if (rc != SQLITE_OK) {
      std::string msg = errmsg ? errmsg : "unknown sqlite3_exec error";
      if (errmsg) {
         sqlite3_free(errmsg);
      }
      throw SinkError("SqliteSink SQL error: " + msg);
   }
}

void SqliteSink::prepare_statements() {
   char const* sql_sel =
       "SELECT series_id FROM series WHERE dut_name=?1 AND metric_name=?2 AND oid=?3;";
   if (sqlite3_prepare_v2(db_, sql_sel, -1, &stmt_sel_series_, nullptr) != SQLITE_OK) {
      throw SinkError(std::string("SqliteSink prepare select series failed: ") +
                      sqlite3_errmsg(db_));
   }

   char const* sql_ins_series =
       "INSERT OR IGNORE INTO series(dut_name, dut_ip, metric_name, oid) VALUES(?1, ?2, ?3, ?4);";
   if (sqlite3_prepare_v2(db_, sql_ins_series, -1, &stmt_ins_series_, nullptr) != SQLITE_OK) {
      throw SinkError(std::string("SqliteSink prepare insert series failed: ") +
                      sqlite3_errmsg(db_));
   }

   char const* sql_ins_sample =
       "INSERT INTO samples(ts_unix_ns, series_id, oid_index, snmp_type, raw_value, type_tag, "
       "i64, f64, str_val, status) VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10);";
   if (sqlite3_prepare_v2(db_, sql_ins_sample, -1, &stmt_ins_sample_, nullptr) != SQLITE_OK) {
      throw SinkError(std::string("SqliteSink prepare insert sample failed: ") +
                      sqlite3_errmsg(db_));
   }
}

void SqliteSink::begin_txn() {
   if (!in_txn_ && db_) {
      exec("BEGIN IMMEDIATE TRANSACTION;");
      in_txn_ = true;
   }
}

void SqliteSink::commit_txn() {
   if (in_txn_ && db_) {
      exec("COMMIT;");
      in_txn_ = false;
      batch_count_ = 0;
      last_commit_ = std::chrono::steady_clock::now();
   }
}

void SqliteSink::maybe_commit() {
   if (!in_txn_) {
      return;
   }
   auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - last_commit_)
                      .count();
   if (batch_count_ >= commit_every_n_ || elapsed >= commit_every_ms_) {
      commit_txn();
   }
}

int64_t SqliteSink::upsert_series(std::string const& dut_name,
                                  std::string const& dut_ip,
                                  std::string const& metric_name,
                                  std::string const& oid) {
   std::string key = dut_name + "\x1f" + metric_name + "\x1f" + oid;
   auto it = series_cache_.find(key);
   if (it != series_cache_.end()) {
      return it->second;
   }

   sqlite3_reset(stmt_ins_series_);
   sqlite3_clear_bindings(stmt_ins_series_);
   sqlite3_bind_text(stmt_ins_series_, 1, dut_name.c_str(), -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt_ins_series_, 2, dut_ip.c_str(), -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt_ins_series_, 3, metric_name.c_str(), -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt_ins_series_, 4, oid.c_str(), -1, SQLITE_TRANSIENT);
   if (sqlite3_step(stmt_ins_series_) != SQLITE_DONE) {
      throw SinkError(std::string("SqliteSink insert series failed: ") + sqlite3_errmsg(db_));
   }

   sqlite3_reset(stmt_sel_series_);
   sqlite3_clear_bindings(stmt_sel_series_);
   sqlite3_bind_text(stmt_sel_series_, 1, dut_name.c_str(), -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt_sel_series_, 2, metric_name.c_str(), -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt_sel_series_, 3, oid.c_str(), -1, SQLITE_TRANSIENT);
   if (sqlite3_step(stmt_sel_series_) != SQLITE_ROW) {
      throw SinkError("SqliteSink failed to retrieve series_id after upsert");
   }
   int64_t sid = sqlite3_column_int64(stmt_sel_series_, 0);
   series_cache_[key] = sid;
   return sid;
}

void SqliteSink::write(Batch const& batch) {
   if (!db_) {
      throw SinkError("SqliteSink::write called after close()");
   }
   begin_txn();

   for (auto const& s : batch.samples) {
      int64_t sid = upsert_series(batch.dut_name, batch.dut_ip, s.metric_name, s.oid);
      sqlite3_reset(stmt_ins_sample_);
      sqlite3_clear_bindings(stmt_ins_sample_);

      sqlite3_bind_int64(stmt_ins_sample_, 1, batch.ts_unix_ns);
      sqlite3_bind_int64(stmt_ins_sample_, 2, sid);
      sqlite3_bind_text(stmt_ins_sample_, 3, s.oid_index.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt_ins_sample_, 4, s.snmp_type.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt_ins_sample_, 5, s.raw_value.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(stmt_ins_sample_, 6, static_cast<int>(s.type_tag));

      if (s.type_tag == TypeTag::Int64) {
         sqlite3_bind_int64(stmt_ins_sample_, 7, s.i64);
      } else {
         sqlite3_bind_null(stmt_ins_sample_, 7);
      }

      if (s.type_tag == TypeTag::Float64) {
         sqlite3_bind_double(stmt_ins_sample_, 8, s.f64);
      } else {
         sqlite3_bind_null(stmt_ins_sample_, 8);
      }

      if (s.type_tag == TypeTag::Str) {
         sqlite3_bind_text(stmt_ins_sample_, 9, s.str_val.c_str(), -1, SQLITE_TRANSIENT);
      } else {
         sqlite3_bind_null(stmt_ins_sample_, 9);
      }

      sqlite3_bind_int(stmt_ins_sample_, 10, s.status);

      if (sqlite3_step(stmt_ins_sample_) != SQLITE_DONE) {
         throw SinkError(std::string("SqliteSink insert sample failed: ") + sqlite3_errmsg(db_));
      }
   }

   ++batch_count_;
   maybe_commit();
}

void SqliteSink::flush() { commit_txn(); }

void SqliteSink::close() {
   if (!db_) {
      return;
   }
   try {
      commit_txn();
   } catch (...) {
   }
   if (stmt_ins_sample_) {
      sqlite3_finalize(stmt_ins_sample_);
      stmt_ins_sample_ = nullptr;
   }
   if (stmt_sel_series_) {
      sqlite3_finalize(stmt_sel_series_);
      stmt_sel_series_ = nullptr;
   }
   if (stmt_ins_series_) {
      sqlite3_finalize(stmt_ins_series_);
      stmt_ins_series_ = nullptr;
   }
   sqlite3_close(db_);
   db_ = nullptr;
}

#else

SqliteSink::SqliteSink(SinkConfig const& /*cfg*/) {
   throw SinkError("SQLite3 support was not compiled into ezsnmp_data_logger");
}
SqliteSink::~SqliteSink() = default;
void SqliteSink::write(Batch const& /*batch*/) {}
void SqliteSink::flush() {}
void SqliteSink::close() {}
void SqliteSink::exec(char const* /*sql*/) {}
void SqliteSink::prepare_statements() {}
void SqliteSink::begin_txn() {}
void SqliteSink::commit_txn() {}
void SqliteSink::maybe_commit() {}
int64_t SqliteSink::upsert_series(std::string const&,
                                  std::string const&,
                                  std::string const&,
                                  std::string const&) {
   return 0;
}

#endif // EZSNMP_DL_HAVE_SQLITE3

} // namespace ezsnmp_data_logger
