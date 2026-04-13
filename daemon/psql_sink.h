#pragma once
#include "sink.h"
#include "config.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <libpq-fe.h>

namespace ezsnmp_logger {

class PsqlSink : public ISink {
public:
    explicit PsqlSink(const SinkConfig& cfg);
    ~PsqlSink() override;

    void write(const Batch& batch) override;
    void flush() override;
    void close() override;
    std::string name() const override { return "psql"; }

private:
    std::string conninfo_;
    int batch_size_;
    int reconnect_delay_ms_;
    PGconn* conn_ = nullptr;

    // series_id cache: (dut_name + "\0" + metric_name) -> series_id
    std::unordered_map<std::string, int64_t> series_cache_;

    // Pending samples buffer
    struct PendingSample {
        int64_t ts_unix_ns;
        int64_t series_id;
        int status;
        TypeTag type_tag;
        int64_t i64;
        double f64;
        std::string str_val;
    };
    std::vector<PendingSample> pending_;

    bool connect();
    void ensure_connected();
    void exec_ddl(const char* sql);
    void prepare_statements();
    void flush_pending();

    int64_t upsert_series(const std::string& dut_name,
                          const std::string& dut_ip,
                          const std::string& metric_name,
                          const std::string& oid);
};

} // namespace ezsnmp_logger
