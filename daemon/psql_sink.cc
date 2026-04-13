#include "psql_sink.h"
#include <stdexcept>
#include <string>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdio>

namespace ezsnmp_logger {

PsqlSink::PsqlSink(const SinkConfig& cfg)
    : conninfo_(cfg.psql_conninfo),
      batch_size_(cfg.psql_batch_size),
      reconnect_delay_ms_(cfg.psql_reconnect_delay_ms)
{
    connect();
}

PsqlSink::~PsqlSink() {
    close();
}

bool PsqlSink::connect() {
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
    exec_ddl(R"(
        CREATE TABLE IF NOT EXISTS series (
            series_id   SERIAL PRIMARY KEY,
            dut_name    TEXT NOT NULL,
            dut_ip      TEXT NOT NULL,
            metric_name TEXT NOT NULL,
            oid         TEXT NOT NULL,
            UNIQUE(dut_name, metric_name)
        )
    )");
    exec_ddl(R"(
        CREATE TABLE IF NOT EXISTS samples (
            ts_unix_ns BIGINT NOT NULL,
            series_id  INTEGER NOT NULL,
            status     INTEGER NOT NULL,
            i64        BIGINT,
            f64        DOUBLE PRECISION,
            str_val    TEXT,
            FOREIGN KEY(series_id) REFERENCES series(series_id)
        )
    )");
    exec_ddl("CREATE INDEX IF NOT EXISTS idx_samples_series_ts ON samples(series_id, ts_unix_ns)");
    prepare_statements();
    return true;
}

void PsqlSink::ensure_connected() {
    if (conn_ && PQstatus(conn_) == CONNECTION_OK) return;
    int delay = reconnect_delay_ms_;
    while (!connect()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        delay = std::min(delay * 2, 30000);
    }
}

void PsqlSink::exec_ddl(const char* sql) {
    PGresult* res = PQexec(conn_, sql);
    ExecStatusType status = PQresultStatus(res);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        std::string msg = "psql exec_ddl: ";
        msg += PQerrorMessage(conn_);
        PQclear(res);
        throw std::runtime_error(msg);
    }
    PQclear(res);
}

void PsqlSink::prepare_statements() {
    PGresult* res;
    // Upsert series
    res = PQprepare(conn_, "ins_series",
        "INSERT INTO series(dut_name, dut_ip, metric_name, oid) "
        "VALUES($1,$2,$3,$4) ON CONFLICT(dut_name,metric_name) DO NOTHING",
        4, nullptr);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::string msg = "prepare ins_series: ";
        msg += PQerrorMessage(conn_);
        PQclear(res);
        throw std::runtime_error(msg);
    }
    PQclear(res);

    res = PQprepare(conn_, "sel_series",
        "SELECT series_id FROM series WHERE dut_name=$1 AND metric_name=$2",
        2, nullptr);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::string msg = "prepare sel_series: ";
        msg += PQerrorMessage(conn_);
        PQclear(res);
        throw std::runtime_error(msg);
    }
    PQclear(res);
}

int64_t PsqlSink::upsert_series(const std::string& dut_name,
                                  const std::string& dut_ip,
                                  const std::string& metric_name,
                                  const std::string& oid) {
    std::string key = dut_name + '\0' + metric_name;
    auto it = series_cache_.find(key);
    if (it != series_cache_.end()) return it->second;

    ensure_connected();
    const char* params_ins[4] = {
        dut_name.c_str(), dut_ip.c_str(), metric_name.c_str(), oid.c_str()
    };
    PGresult* res = PQexecPrepared(conn_, "ins_series", 4, params_ins, nullptr, nullptr, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::string msg = "ins_series exec: ";
        msg += PQerrorMessage(conn_);
        PQclear(res);
        throw std::runtime_error(msg);
    }
    PQclear(res);

    const char* params_sel[2] = { dut_name.c_str(), metric_name.c_str() };
    res = PQexecPrepared(conn_, "sel_series", 2, params_sel, nullptr, nullptr, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) < 1) {
        std::string msg = "sel_series: ";
        msg += PQerrorMessage(conn_);
        PQclear(res);
        throw std::runtime_error(msg);
    }
    int64_t sid = std::stoll(PQgetvalue(res, 0, 0));
    PQclear(res);

    series_cache_[key] = sid;
    return sid;
}

void PsqlSink::flush_pending() {
    if (pending_.empty()) return;
    ensure_connected();

    PGresult* res = PQexec(conn_, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return;
    }
    PQclear(res);

    for (auto& p : pending_) {
        char ts_str[32], sid_str[32], status_str[16];
        char i64_str[32], f64_str[64];
        snprintf(ts_str,     sizeof(ts_str),     "%lld", (long long)p.ts_unix_ns);
        snprintf(sid_str,    sizeof(sid_str),     "%lld", (long long)p.series_id);
        snprintf(status_str, sizeof(status_str),  "%d",   p.status);

        const char* params[6];
        params[0] = ts_str;
        params[1] = sid_str;
        params[2] = status_str;

        if (p.type_tag == TypeTag::Int64) {
            snprintf(i64_str, sizeof(i64_str), "%lld", (long long)p.i64);
            params[3] = i64_str;
            params[4] = nullptr;
            params[5] = nullptr;
        } else if (p.type_tag == TypeTag::Float64) {
            snprintf(f64_str, sizeof(f64_str), "%.17g", p.f64);
            params[3] = nullptr;
            params[4] = f64_str;
            params[5] = nullptr;
        } else if (p.type_tag == TypeTag::Str) {
            params[3] = nullptr;
            params[4] = nullptr;
            params[5] = p.str_val.c_str();
        } else {
            params[3] = nullptr;
            params[4] = nullptr;
            params[5] = nullptr;
        }

        // Use inline INSERT since we need nullable params
        PGresult* r = PQexecParams(conn_,
            "INSERT INTO samples(ts_unix_ns,series_id,status,i64,f64,str_val) VALUES($1,$2,$3,$4,$5,$6)",
            6, nullptr, params, nullptr, nullptr, 0);
        if (PQresultStatus(r) != PGRES_COMMAND_OK) {
            PQclear(r);
            PQexec(conn_, "ROLLBACK");
            throw std::runtime_error(std::string("ins_sample: ") + PQerrorMessage(conn_));
        }
        PQclear(r);
    }

    res = PQexec(conn_, "COMMIT");
    PQclear(res);
    pending_.clear();
}

void PsqlSink::write(const Batch& batch) {
    ensure_connected();
    for (const auto& s : batch.samples) {
        int64_t sid = upsert_series(batch.dut_name, batch.dut_ip, s.metric_name, s.oid);
        pending_.push_back({batch.ts_unix_ns, sid, s.status, s.type_tag, s.i64, s.f64, s.str_val});
    }
    if (static_cast<int>(pending_.size()) >= batch_size_) {
        flush_pending();
    }
}

void PsqlSink::flush() {
    flush_pending();
}

void PsqlSink::close() {
    if (!conn_) return;
    try { flush_pending(); } catch (...) {}
    PQfinish(conn_);
    conn_ = nullptr;
}

} // namespace ezsnmp_logger
