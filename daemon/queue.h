#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <cstddef>
#include <atomic>

namespace ezsnmp_logger {

template<typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t max_size)
        : max_size_(max_size), shutdown_(false), dropped_(0) {}

    // Returns false if queue is full (item dropped)
    bool try_push(T item) {
        std::unique_lock<std::mutex> lk(mutex_);
        if (queue_.size() >= max_size_) {
            ++dropped_;
            return false;
        }
        queue_.push(std::move(item));
        lk.unlock();
        cv_.notify_one();
        return true;
    }

    // Blocks until item available or shutdown
    std::optional<T> pop(bool& shutdown_flag) {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_.wait(lk, [this] { return !queue_.empty() || shutdown_; });
        if (queue_.empty()) {
            shutdown_flag = true;
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lk(mutex_);
            shutdown_ = true;
        }
        cv_.notify_all();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return queue_.size();
    }

    size_t dropped() const {
        return dropped_.load();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
    size_t max_size_;
    bool shutdown_;
    std::atomic<size_t> dropped_;
};

} // namespace ezsnmp_logger
