#pragma once
#include "sink.h"
#include "config.h"
#include <string>
#include <unordered_map>
#include <chrono>
#include <sqlite3.h>

namespace ezsnmp_logger {

class SqliteSink : public ISink {
public:
    explicit SqliteSink(const SinkConfig& cfg);
    ~SqliteSink() override;

    void write(const Batch& batch) override;
    void flush() override;
    void close() override;
    std::string name() const override { return "sqlite3"; }

private:
    sqlite3* db_ = nullptr;
    int commit_every_n_;
    int commit_every_ms_;
    int batch_count_ = 0;
    std::chrono::steady_clock::time_point last_commit_;

    // series_id cache: (dut_name + "\0" + metric_name) -> series_id
    std::unordered_map<std::string, int64_t> series_cache_;

    void exec(const char* sql);
    void begin_txn();
    void commit_txn();
    bool in_txn_ = false;

    int64_t upsert_series(const std::string& dut_name,
                          const std::string& dut_ip,
                          const std::string& metric_name,
                          const std::string& oid);

    sqlite3_stmt* stmt_ins_sample_ = nullptr;
    sqlite3_stmt* stmt_sel_series_ = nullptr;
    sqlite3_stmt* stmt_ins_series_ = nullptr;

    void prepare_statements();
    void maybe_commit();
};

} // namespace ezsnmp_logger
