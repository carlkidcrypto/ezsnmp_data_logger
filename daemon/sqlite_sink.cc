#include "sqlite_sink.h"
#include <stdexcept>
#include <filesystem>
#include <string>

namespace ezsnmp_logger {

static void check(int rc, sqlite3* db, const char* ctx) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        std::string msg = ctx;
        msg += ": ";
        msg += sqlite3_errmsg(db);
        throw std::runtime_error(msg);
    }
}

SqliteSink::SqliteSink(const SinkConfig& cfg)
    : commit_every_n_(cfg.sqlite_commit_every_n),
      commit_every_ms_(cfg.sqlite_commit_every_ms),
      last_commit_(std::chrono::steady_clock::now())
{
    auto parent = std::filesystem::path(cfg.sqlite_path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);

    int rc = sqlite3_open(cfg.sqlite_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(std::string("sqlite3_open: ") + sqlite3_errmsg(db_));
    }
    exec("PRAGMA journal_mode=WAL");
    exec("PRAGMA synchronous=NORMAL");
    exec("PRAGMA busy_timeout=5000");
    exec(R"(
        CREATE TABLE IF NOT EXISTS series (
            series_id   INTEGER PRIMARY KEY AUTOINCREMENT,
            dut_name    TEXT NOT NULL,
            dut_ip      TEXT NOT NULL,
            metric_name TEXT NOT NULL,
            oid         TEXT NOT NULL,
            UNIQUE(dut_name, metric_name)
        )
    )");
    exec(R"(
        CREATE TABLE IF NOT EXISTS samples (
            ts_unix_ns INTEGER NOT NULL,
            series_id  INTEGER NOT NULL,
            status     INTEGER NOT NULL,
            i64        INTEGER,
            f64        REAL,
            str_val    TEXT,
            FOREIGN KEY(series_id) REFERENCES series(series_id)
        )
    )");
    exec("CREATE INDEX IF NOT EXISTS idx_samples_series_ts ON samples(series_id, ts_unix_ns)");
    prepare_statements();
}

SqliteSink::~SqliteSink() {
    close();
}

void SqliteSink::exec(const char* sql) {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = "sqlite3_exec: ";
        msg += errmsg ? errmsg : "(null)";
        sqlite3_free(errmsg);
        throw std::runtime_error(msg);
    }
}

void SqliteSink::prepare_statements() {
    int rc;
    rc = sqlite3_prepare_v2(db_,
        "INSERT OR IGNORE INTO series(dut_name, dut_ip, metric_name, oid) VALUES(?,?,?,?)",
        -1, &stmt_ins_series_, nullptr);
    check(rc, db_, "prepare ins_series");

    rc = sqlite3_prepare_v2(db_,
        "SELECT series_id FROM series WHERE dut_name=? AND metric_name=?",
        -1, &stmt_sel_series_, nullptr);
    check(rc, db_, "prepare sel_series");

    rc = sqlite3_prepare_v2(db_,
        "INSERT INTO samples(ts_unix_ns, series_id, status, i64, f64, str_val) VALUES(?,?,?,?,?,?)",
        -1, &stmt_ins_sample_, nullptr);
    check(rc, db_, "prepare ins_sample");
}

void SqliteSink::begin_txn() {
    if (!in_txn_) {
        // Retry on SQLITE_BUSY
        int rc;
        do {
            rc = sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
        } while (rc == SQLITE_BUSY);
        check(rc, db_, "BEGIN IMMEDIATE");
        in_txn_ = true;
    }
}

void SqliteSink::commit_txn() {
    if (in_txn_) {
        exec("COMMIT");
        in_txn_ = false;
        last_commit_ = std::chrono::steady_clock::now();
    }
}

int64_t SqliteSink::upsert_series(const std::string& dut_name,
                                   const std::string& dut_ip,
                                   const std::string& metric_name,
                                   const std::string& oid) {
    std::string key = dut_name + '\0' + metric_name;
    auto it = series_cache_.find(key);
    if (it != series_cache_.end()) return it->second;

    // Insert (ignore if exists)
    sqlite3_bind_text(stmt_ins_series_, 1, dut_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_ins_series_, 2, dut_ip.c_str(),   -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_ins_series_, 3, metric_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_ins_series_, 4, oid.c_str(),       -1, SQLITE_STATIC);
    sqlite3_step(stmt_ins_series_);
    sqlite3_reset(stmt_ins_series_);
    sqlite3_clear_bindings(stmt_ins_series_);

    // Select id
    sqlite3_bind_text(stmt_sel_series_, 1, dut_name.c_str(),    -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_sel_series_, 2, metric_name.c_str(), -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt_sel_series_);
    int64_t sid = -1;
    if (rc == SQLITE_ROW) {
        sid = sqlite3_column_int64(stmt_sel_series_, 0);
    }
    sqlite3_reset(stmt_sel_series_);
    sqlite3_clear_bindings(stmt_sel_series_);

    if (sid < 0) throw std::runtime_error("upsert_series: could not get series_id");
    series_cache_[key] = sid;
    return sid;
}

void SqliteSink::maybe_commit() {
    ++batch_count_;
    auto now = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_commit_).count();
    if (batch_count_ >= commit_every_n_ || elapsed_ms >= commit_every_ms_) {
        commit_txn();
        batch_count_ = 0;
    }
}

void SqliteSink::write(const Batch& batch) {
    begin_txn();

    for (const auto& s : batch.samples) {
        int64_t sid = upsert_series(batch.dut_name, batch.dut_ip, s.metric_name, s.oid);

        sqlite3_bind_int64(stmt_ins_sample_, 1, batch.ts_unix_ns);
        sqlite3_bind_int64(stmt_ins_sample_, 2, sid);
        sqlite3_bind_int  (stmt_ins_sample_, 3, s.status);

        if (s.type_tag == TypeTag::Int64) {
            sqlite3_bind_int64(stmt_ins_sample_, 4, s.i64);
            sqlite3_bind_null (stmt_ins_sample_, 5);
            sqlite3_bind_null (stmt_ins_sample_, 6);
        } else if (s.type_tag == TypeTag::Float64) {
            sqlite3_bind_null  (stmt_ins_sample_, 4);
            sqlite3_bind_double(stmt_ins_sample_, 5, s.f64);
            sqlite3_bind_null  (stmt_ins_sample_, 6);
        } else if (s.type_tag == TypeTag::Str) {
            sqlite3_bind_null(stmt_ins_sample_, 4);
            sqlite3_bind_null(stmt_ins_sample_, 5);
            sqlite3_bind_text(stmt_ins_sample_, 6, s.str_val.c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt_ins_sample_, 4);
            sqlite3_bind_null(stmt_ins_sample_, 5);
            sqlite3_bind_null(stmt_ins_sample_, 6);
        }

        int rc = sqlite3_step(stmt_ins_sample_);
        if (rc != SQLITE_DONE) {
            sqlite3_reset(stmt_ins_sample_);
            sqlite3_clear_bindings(stmt_ins_sample_);
            throw std::runtime_error(std::string("ins_sample: ") + sqlite3_errmsg(db_));
        }
        sqlite3_reset(stmt_ins_sample_);
        sqlite3_clear_bindings(stmt_ins_sample_);
    }

    maybe_commit();
}

void SqliteSink::flush() {
    commit_txn();
    batch_count_ = 0;
}

void SqliteSink::close() {
    if (!db_) return;
    flush();
    if (stmt_ins_sample_) { sqlite3_finalize(stmt_ins_sample_); stmt_ins_sample_ = nullptr; }
    if (stmt_sel_series_) { sqlite3_finalize(stmt_sel_series_); stmt_sel_series_ = nullptr; }
    if (stmt_ins_series_) { sqlite3_finalize(stmt_ins_series_); stmt_ins_series_ = nullptr; }
    sqlite3_close(db_);
    db_ = nullptr;
}

} // namespace ezsnmp_logger
