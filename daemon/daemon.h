#pragma once
#include "config.h"
#include "batch.h"
#include "queue.h"
#include "sink.h"
#include <memory>
#include <thread>
#include <atomic>
#include <string>
#include <vector>

namespace ezsnmp_logger {

class Daemon {
public:
    explicit Daemon(Config cfg);
    ~Daemon();

    void run();
    void stop();

    std::string stats_json() const;

private:
    Config cfg_;
    std::vector<std::unique_ptr<ISink>> sinks_;

    void* zmq_ctx_ = nullptr;
    void* zmq_sock_ = nullptr;

    BoundedQueue<Batch> queue_;
    std::atomic<bool> running_{false};

    std::thread recv_thread_;
    std::thread writer_thread_;

    std::atomic<int64_t> processed_batches_{0};
    std::atomic<int64_t> dropped_batches_{0};
    std::string last_error_;
    mutable std::mutex error_mutex_;

    void recv_loop();
    void writer_loop();
};

} // namespace ezsnmp_logger
