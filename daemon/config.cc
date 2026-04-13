#include "config.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ezsnmp_logger {

Config parse_config(const std::string& json_text) {
    auto j = nlohmann::json::parse(json_text);

    Config cfg;
    cfg.zmq_endpoint  = j.value("zmq_endpoint", cfg.zmq_endpoint);
    cfg.zmq_hwm       = j.value("zmq_hwm", cfg.zmq_hwm);
    cfg.queue_depth   = j.value("queue_depth", cfg.queue_depth);

    for (auto& s : j.value("sinks", nlohmann::json::array())) {
        SinkConfig sc;
        sc.type = s.at("type").get<std::string>();

        if (sc.type == "csv") {
            sc.csv_dir = s.value("dir", ".");
        } else if (sc.type == "sqlite3") {
            sc.sqlite_path           = s.value("path", "logger.sqlite");
            sc.sqlite_commit_every_n = s.value("commit_every_n", sc.sqlite_commit_every_n);
            sc.sqlite_commit_every_ms= s.value("commit_every_ms", sc.sqlite_commit_every_ms);
        } else if (sc.type == "psql") {
            sc.psql_conninfo          = s.value("conninfo", "");
            sc.psql_batch_size        = s.value("batch_size", sc.psql_batch_size);
            sc.psql_reconnect_delay_ms= s.value("reconnect_delay_ms", sc.psql_reconnect_delay_ms);
        } else {
            throw std::runtime_error("Unknown sink type: " + sc.type);
        }
        cfg.sinks.push_back(std::move(sc));
    }
    return cfg;
}

} // namespace ezsnmp_logger
