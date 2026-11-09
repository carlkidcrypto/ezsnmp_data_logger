#pragma once

#ifndef SWIG

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>

namespace ezsnmp_data_logger {

/**
 * @brief Thread-safe bounded MPMC queue with non-blocking push and graceful drain-on-shutdown.
 * @tparam T Element type stored in the queue.
 */
template <typename T>
class BoundedQueue {
  public:
   /**
    * @brief Construct a BoundedQueue with a maximum capacity.
    * @param max_size Maximum number of queued elements before try_push drops incoming items.
    */
   explicit BoundedQueue(size_t max_size) : max_size_(max_size == 0 ? 1 : max_size), dropped_(0) {}

   /**
    * @brief Attempt to enqueue an item without blocking.
    * @param item Item to enqueue.
    * @return true if enqueued, false if the queue is full or shutting down.
    */
   bool try_push(T item) {
      {
         std::lock_guard<std::mutex> lk(mutex_);
         if (shutdown_.load()) {
            return false;
         }
         if (queue_.size() >= max_size_) {
            ++dropped_;
            return false;
         }
         queue_.push(std::move(item));
      }
      cv_.notify_one();
      return true;
   }

   /**
    * @brief Block until an item is available or shutdown completes after draining all items.
    * @param shutdown_flag Set to true when shutdown was requested and the queue is fully drained.
    * @return Next item, or std::nullopt when drained after shutdown.
    */
   std::optional<T> pop(bool& shutdown_flag) {
      std::unique_lock<std::mutex> lk(mutex_);
      cv_.wait(lk, [this] { return !queue_.empty() || shutdown_.load(); });
      if (!queue_.empty()) {
         T item = std::move(queue_.front());
         queue_.pop();
         return item;
      }
      shutdown_flag = true;
      return std::nullopt;
   }

   /**
    * @brief Block until an item is available or shutdown completes after draining all items.
    * @return Next item, or std::nullopt when drained after shutdown.
    */
   std::optional<T> pop_blocking() {
      bool ignored = false;
      return pop(ignored);
   }

   /**
    * @brief Signal shutdown so blocked pop() calls wake up once remaining items drain.
    */
   void shutdown() {
      shutdown_.store(true);
      cv_.notify_all();
   }

   /**
    * @brief Reset shutdown flag for restarting the queue when empty.
    */
   void reset() {
      std::lock_guard<std::mutex> lk(mutex_);
      shutdown_.store(false);
   }

   /**
    * @brief Alias for reset() when reopening the queue for a subsequent start() cycle.
    */
   void reopen() { reset(); }

   /**
    * @brief Return the current number of buffered items.
    * @return Queue depth.
    */
   size_t size() const {
      std::lock_guard<std::mutex> lk(mutex_);
      return queue_.size();
   }

   /**
    * @brief Return the cumulative count of items dropped due to back-pressure.
    * @return Dropped item count.
    */
   size_t dropped() const { return dropped_.load(); }

  private:
   mutable std::mutex mutex_;
   std::condition_variable cv_;
   std::queue<T> queue_;
   size_t max_size_;
   std::atomic<bool> shutdown_{false};
   std::atomic<size_t> dropped_;
};

} // namespace ezsnmp_data_logger

#endif // SWIG
