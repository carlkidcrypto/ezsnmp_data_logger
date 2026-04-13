#include "daemon.h"
#include "csv_sink.h"
#include "sqlite_sink.h"
#include "psql_sink.h"
#include <zmq.h>
#include <stdexcept>
#include <string>
#include <cstring>
#include <nlohmann/json.hpp>

namespace ezsnmp_logger {

Daemon::Daemon(Config cfg)
    : cfg_(std::move(cfg)),
      queue_(static_cast<size_t>(cfg_.queue_depth))
{
    // Create sinks
    for (const auto& sc : cfg_.sinks) {
        if (sc.type == "csv") {
            sinks_.push_back(std::make_unique<CsvSink>(sc));
        } else if (sc.type == "sqlite3") {
            sinks_.push_back(std::make_unique<SqliteSink>(sc));
        } else if (sc.type == "psql") {
            sinks_.push_back(std::make_unique<PsqlSink>(sc));
        }
    }

    zmq_ctx_ = zmq_ctx_new();
    if (!zmq_ctx_) throw std::runtime_error("zmq_ctx_new failed");

    zmq_sock_ = zmq_socket(zmq_ctx_, ZMQ_PULL);
    if (!zmq_sock_) {
        zmq_ctx_term(zmq_ctx_);
        throw std::runtime_error("zmq_socket failed");
    }

    int hwm = cfg_.zmq_hwm;
    zmq_setsockopt(zmq_sock_, ZMQ_RCVHWM, &hwm, sizeof(hwm));

    if (zmq_bind(zmq_sock_, cfg_.zmq_endpoint.c_str()) != 0) {
        zmq_close(zmq_sock_);
        zmq_ctx_term(zmq_ctx_);
        throw std::runtime_error(std::string("zmq_bind failed: ") + zmq_strerror(errno));
    }
}

Daemon::~Daemon() {
    stop();
    if (zmq_sock_) { zmq_close(zmq_sock_); zmq_sock_ = nullptr; }
    if (zmq_ctx_)  { zmq_ctx_term(zmq_ctx_); zmq_ctx_ = nullptr; }
}

void Daemon::run() {
    running_ = true;
    recv_thread_   = std::thread([this]{ recv_loop(); });
    writer_thread_ = std::thread([this]{ writer_loop(); });
    recv_thread_.join();
    writer_thread_.join();
}

void Daemon::stop() {
    running_ = false;
    queue_.shutdown();
    // Unblock zmq_recv by closing the socket
    if (zmq_sock_) {
        zmq_close(zmq_sock_);
        zmq_sock_ = nullptr;
    }
}

void Daemon::recv_loop() {
    while (running_) {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        int rc = zmq_msg_recv(&msg, zmq_sock_, 0);
        if (rc < 0) {
            zmq_msg_close(&msg);
            if (!running_) break;
            std::lock_guard<std::mutex> lk(error_mutex_);
            last_error_ = std::string("zmq_msg_recv: ") + zmq_strerror(errno);
            continue;
        }
        try {
            Batch b = decode_batch(zmq_msg_data(&msg), static_cast<size_t>(zmq_msg_size(&msg)));
            if (!queue_.try_push(std::move(b))) {
                ++dropped_batches_;
            }
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lk(error_mutex_);
            last_error_ = std::string("decode_batch: ") + e.what();
        }
        zmq_msg_close(&msg);
    }
}

void Daemon::writer_loop() {
    while (true) {
        bool sd = false;
        auto item = queue_.pop(sd);
        if (sd && !item) break;
        if (!item) continue;

        for (auto& sink : sinks_) {
            try {
                sink->write(*item);
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lk(error_mutex_);
                last_error_ = sink->name() + " write: " + e.what();
            }
        }
        ++processed_batches_;
    }
    // Flush all sinks on shutdown
    for (auto& sink : sinks_) {
        try { sink->flush(); } catch (...) {}
        try { sink->close(); } catch (...) {}
    }
}

std::string Daemon::stats_json() const {
    std::string err;
    {
        std::lock_guard<std::mutex> lk(error_mutex_);
        err = last_error_;
    }
    nlohmann::json j;
    j["processed_batches"] = processed_batches_.load();
    j["dropped_batches"]   = dropped_batches_.load() + static_cast<int64_t>(queue_.dropped());
    j["queue_depth"]       = static_cast<int64_t>(queue_.size());
    j["last_error"]        = err;
    return j.dump();
}

} // namespace ezsnmp_logger
