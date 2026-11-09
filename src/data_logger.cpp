#include "ezsnmp_data_logger/data_logger.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <queue>
#include <thread>

#include "ezsnmp_data_logger/exceptions.h"
#include "ezsnmp_data_logger/json_utils.h"
#include "ezsnmp_data_logger/queue.h"
#include "ezsnmp_data_logger/sink.h"
#include "ezsnmp_data_logger/snmp_poller.h"

namespace ezsnmp_data_logger {

namespace {

void write_status_file_atomic(std::string const& path, std::string const& json_payload) {
   if (path.empty()) {
      return;
   }
   try {
      std::filesystem::path target(path);
      if (target.has_parent_path() && !target.parent_path().empty()) {
         std::error_code ec;
         std::filesystem::create_directories(target.parent_path(), ec);
      }
      std::filesystem::path tmp = target;
      tmp += ".tmp";
      {
         std::ofstream out(tmp, std::ios::out | std::ios::trunc);
         if (!out.is_open()) {
            return;
         }
         out << json_payload << "\n";
      }
      std::error_code ec;
      std::filesystem::rename(tmp, target, ec);
      if (ec) {
         std::filesystem::remove(target, ec);
         std::filesystem::rename(tmp, target, ec);
      }
   } catch (...) {
   }
}

void persist_batch_once(LoggerConfig const& cfg, Batch const& batch) {
   for (auto const& scfg : cfg.sinks) {
      if (!scfg.enabled) {
         continue;
      }
      auto sink = create_sink(scfg);
      sink->write(batch);
      sink->flush();
      sink->close();
   }
}

DutConfig const& find_dut_or_throw(LoggerConfig const& cfg, std::string const& dut_name) {
   for (auto const& d : cfg.duts) {
      if (d.name == dut_name) {
         return d;
      }
   }
   throw ConfigError("DUT '" + dut_name + "' not found in configuration");
}

} // namespace

class DataLogger::Impl {
  public:
   explicit Impl(LoggerConfig const& cfg)
       : config_(cfg),
         queue_(cfg.logger.queue_depth > 0 ? static_cast<size_t>(cfg.logger.queue_depth) : 100000),
         start_time_(std::chrono::steady_clock::now()) {
      ValidationReport rep = validate_config(config_.to_json(-1));
      if (!rep.valid) {
         std::string joined;
         for (size_t i = 0; i < rep.errors.size(); ++i) {
            if (i > 0) {
               joined += "; ";
            }
            joined += rep.errors[i];
         }
         throw ConfigError("Invalid LoggerConfig: " + joined);
      }

      for (auto const& scfg : config_.sinks) {
         if (scfg.enabled) {
            sinks_.push_back(create_sink(scfg));
         }
      }

      for (auto const& dcfg : config_.duts) {
         if (dcfg.enabled) {
            DutRuntime dr;
            dr.config = dcfg;
            dr.interval_ms =
                dcfg.poll_interval_ms > 0 ? dcfg.poll_interval_ms : config_.logger.poll_interval_ms;
            if (dr.interval_ms < 1) {
               dr.interval_ms = 1000;
            }
            dr.poller = create_snmp_poller(dcfg);
            duts_.push_back(std::move(dr));
         }
      }
   }

   ~Impl() {
      stop();
      std::lock_guard<std::mutex> lock(sinks_mutex_);
      for (auto& s : sinks_) {
         s->close();
      }
      sinks_.clear();
   }

   void set_batch_callback(std::function<void(Batch const&)> cb) {
      std::lock_guard<std::mutex> lock(cb_mutex_);
      batch_cb_ = std::move(cb);
   }

   void start() {
      std::lock_guard<std::mutex> lock(lifecycle_mutex_);
      if (running_.load()) {
         return;
      }

      queue_.reopen();
      stop_requested_.store(false);
      all_cycles_completed_.store(false);
      start_time_ = std::chrono::steady_clock::now();

      {
         std::lock_guard<std::mutex> slock(sched_mutex_);
         while (!schedule_.empty()) {
            schedule_.pop();
         }
         auto now = std::chrono::steady_clock::now();
         for (size_t i = 0; i < duts_.size(); ++i) {
            duts_[i].cycles_done = 0;
            schedule_.push(SchedItem{now, i});
         }
      }

      running_.store(true);
      writer_thread_ = std::thread(&Impl::writer_loop, this);

      if (!duts_.empty()) {
         size_t max_workers = static_cast<size_t>(std::max(1, config_.logger.worker_threads));
         size_t worker_count = std::min(duts_.size(), max_workers);
         workers_.reserve(worker_count);
         for (size_t w = 0; w < worker_count; ++w) {
            workers_.emplace_back(&Impl::worker_loop, this);
         }
      }
   }

   void stop() noexcept {
      try {
         std::lock_guard<std::mutex> lock(lifecycle_mutex_);
         if (!running_.load()) {
            return;
         }

         stop_requested_.store(true);
         sched_cv_.notify_all();
         wait_cv_.notify_all();

         for (auto& wt : workers_) {
            if (wt.joinable()) {
               wt.join();
            }
         }
         workers_.clear();

         queue_.shutdown();
         if (writer_thread_.joinable()) {
            writer_thread_.join();
         }

         flush_sinks();
         running_.store(false);
         update_status_file();
         wait_cv_.notify_all();
      } catch (...) {
      }
   }

   void run() {
      start();
      auto deadline = std::chrono::steady_clock::time_point::max();
      if (config_.logger.run_duration_seconds > 0) {
         deadline = std::chrono::steady_clock::now() +
                    std::chrono::seconds(config_.logger.run_duration_seconds);
      }

      {
         std::unique_lock<std::mutex> wlock(wait_mutex_);
         while (running_.load() && !stop_requested_.load() && !all_cycles_completed_.load()) {
            if (config_.logger.run_duration_seconds > 0) {
               if (wait_cv_.wait_until(wlock, deadline) == std::cv_status::timeout) {
                  break;
               }
            } else {
               wait_cv_.wait_for(wlock, std::chrono::milliseconds(200));
            }
         }
      }
      stop();
   }

   std::vector<Batch> run_once() {
      std::vector<Batch> batches;
      batches.reserve(duts_.size());

      for (auto& dr : duts_) {
         try {
            Batch b = dr.poller->poll();
            bool any_err = false;
            for (auto const& s : b.samples) {
               if (s.status != 0) {
                  any_err = true;
               }
            }
            if (any_err) {
               poll_errors_.fetch_add(1);
            }
            write_batch_to_sinks(b);
            batches.push_back(std::move(b));
         } catch (std::exception const& e) {
            poll_errors_.fetch_add(1);
            set_last_error(e.what());
         }
      }

      cycles_completed_.fetch_add(1);
      flush_sinks();
      update_status_file();
      return batches;
   }

   bool is_running() const { return running_.load(); }

   bool push_batch(Batch const& batch) {
      if (running_.load()) {
         bool ok = queue_.try_push(Batch(batch));
         if (!ok) {
            dropped_batches_.fetch_add(1);
         }
         return ok;
      }
      write_batch_to_sinks(batch);
      flush_sinks();
      return true;
   }

   bool push_batch_json(std::string const& batch_json) {
      Batch b = Batch::from_json(batch_json);
      if (b.ts_unix_ns <= 0) {
         b.ts_unix_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
      }
      return push_batch(b);
   }

   void flush_sinks() {
      std::lock_guard<std::mutex> lock(sinks_mutex_);
      for (auto& s : sinks_) {
         try {
            s->flush();
         } catch (std::exception const& e) {
            sink_errors_.fetch_add(1);
            set_last_error(e.what());
         }
      }
   }

   LoggerStats stats() const {
      LoggerStats st;
      st.running = running_.load();
      st.cycles_completed = cycles_completed_.load();
      st.processed_batches = processed_batches_.load();
      st.processed_samples = processed_samples_.load();
      st.dropped_batches = dropped_batches_.load();
      st.poll_errors = poll_errors_.load();
      st.sink_errors = sink_errors_.load();
      st.queue_depth = static_cast<int64_t>(queue_.size());
      st.uptime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - start_time_)
                         .count();
      {
         std::lock_guard<std::mutex> elock(err_mutex_);
         st.last_error = last_error_;
      }
      return st;
   }

   LoggerConfig const& config() const { return config_; }

  private:
   struct DutRuntime {
      DutConfig config;
      int interval_ms = 1000;
      int64_t cycles_done = 0;
      std::unique_ptr<ISnmpPoller> poller;
   };

   struct SchedItem {
      std::chrono::steady_clock::time_point due;
      size_t dut_index;
      bool operator>(SchedItem const& other) const { return due > other.due; }
   };

   void set_last_error(std::string const& msg) {
      std::lock_guard<std::mutex> lock(err_mutex_);
      last_error_ = msg;
   }

   void update_status_file() {
      if (!config_.logger.status_file.empty()) {
         write_status_file_atomic(config_.logger.status_file, stats().to_json(2));
      }
   }

   void write_batch_to_sinks(Batch const& batch) {
      {
         std::lock_guard<std::mutex> lock(sinks_mutex_);
         for (auto& s : sinks_) {
            try {
               s->write(batch);
            } catch (std::exception const& e) {
               sink_errors_.fetch_add(1);
               set_last_error(e.what());
            }
         }
      }
      processed_batches_.fetch_add(1);
      processed_samples_.fetch_add(static_cast<int64_t>(batch.samples.size()));

      std::function<void(Batch const&)> cb_copy;
      {
         std::lock_guard<std::mutex> cblock(cb_mutex_);
         cb_copy = batch_cb_;
      }
      if (cb_copy) {
         try {
            cb_copy(batch);
         } catch (...) {
         }
      }
   }

   void writer_loop() {
      auto last_status_write = std::chrono::steady_clock::now();
      int status_interval = std::max(250, config_.logger.stats_interval_ms);
      while (auto opt = queue_.pop_blocking()) {
         write_batch_to_sinks(*opt);
         auto now = std::chrono::steady_clock::now();
         if (!config_.logger.status_file.empty() &&
             std::chrono::duration_cast<std::chrono::milliseconds>(now - last_status_write)
                     .count() >= status_interval) {
            update_status_file();
            last_status_write = now;
         }
      }
   }

   void worker_loop() {
      int const max_cycles = config_.logger.max_cycles;
      while (!stop_requested_.load()) {
         size_t dut_idx = 0;
         std::chrono::steady_clock::time_point scheduled_due;
         {
            std::unique_lock<std::mutex> slock(sched_mutex_);
            while (!stop_requested_.load()) {
               if (schedule_.empty()) {
                  return;
               }
               auto top = schedule_.top();
               auto now = std::chrono::steady_clock::now();
               if (now >= top.due) {
                  schedule_.pop();
                  dut_idx = top.dut_index;
                  scheduled_due = top.due;
                  break;
               }
               sched_cv_.wait_until(slock, top.due);
            }
            if (stop_requested_.load()) {
               return;
            }
         }

         try {
            Batch b = duts_[dut_idx].poller->poll();
            bool any_err = false;
            for (auto const& s : b.samples) {
               if (s.status != 0) {
                  any_err = true;
               }
            }
            if (any_err) {
               poll_errors_.fetch_add(1);
            }
            if (!queue_.try_push(std::move(b))) {
               dropped_batches_.fetch_add(1);
            }
         } catch (std::exception const& e) {
            poll_errors_.fetch_add(1);
            set_last_error(e.what());
         }

         {
            std::lock_guard<std::mutex> slock(sched_mutex_);
            int64_t done = ++duts_[dut_idx].cycles_done;

            int64_t min_cycles = duts_[0].cycles_done;
            for (auto const& d : duts_) {
               min_cycles = std::min(min_cycles, d.cycles_done);
            }
            cycles_completed_.store(min_cycles);

            if (max_cycles <= 0 || done < max_cycles) {
               auto now = std::chrono::steady_clock::now();
               auto next_due =
                   scheduled_due + std::chrono::milliseconds(duts_[dut_idx].interval_ms);
               if (next_due < now) {
                  next_due = now;
               }
               schedule_.push(SchedItem{next_due, dut_idx});
               sched_cv_.notify_one();
            } else if (schedule_.empty() && min_cycles >= max_cycles) {
               all_cycles_completed_.store(true);
               sched_cv_.notify_all();
               wait_cv_.notify_all();
               return;
            }
         }
      }
   }

   LoggerConfig config_;
   BoundedQueue<Batch> queue_;
   std::vector<std::unique_ptr<ISink>> sinks_;
   std::mutex sinks_mutex_;

   std::vector<DutRuntime> duts_;
   std::priority_queue<SchedItem, std::vector<SchedItem>, std::greater<SchedItem>> schedule_;
   std::mutex sched_mutex_;
   std::condition_variable sched_cv_;

   std::mutex lifecycle_mutex_;
   std::atomic<bool> running_{false};
   std::atomic<bool> stop_requested_{false};
   std::atomic<bool> all_cycles_completed_{false};
   std::mutex wait_mutex_;
   std::condition_variable wait_cv_;

   std::thread writer_thread_;
   std::vector<std::thread> workers_;

   std::chrono::steady_clock::time_point start_time_;
   std::atomic<int64_t> cycles_completed_{0};
   std::atomic<int64_t> processed_batches_{0};
   std::atomic<int64_t> processed_samples_{0};
   std::atomic<int64_t> dropped_batches_{0};
   std::atomic<int64_t> poll_errors_{0};
   std::atomic<int64_t> sink_errors_{0};

   mutable std::mutex err_mutex_;
   std::string last_error_;

   std::mutex cb_mutex_;
   std::function<void(Batch const&)> batch_cb_;
};

DataLogger::DataLogger(LoggerConfig const& config) : impl_(std::make_shared<Impl>(config)) {}

DataLogger::DataLogger(std::string const& json_config)
    : impl_(std::make_shared<Impl>(LoggerConfig::from_json(json_config))) {}

DataLogger DataLogger::from_file(std::string const& file_path) {
   return DataLogger(LoggerConfig::from_file(file_path));
}

DataLogger::~DataLogger() = default;
DataLogger::DataLogger(DataLogger const& other) = default;
DataLogger& DataLogger::operator=(DataLogger const& other) = default;
DataLogger::DataLogger(DataLogger&& other) noexcept = default;
DataLogger& DataLogger::operator=(DataLogger&& other) noexcept = default;

void DataLogger::set_batch_callback(std::function<void(Batch const&)> cb) {
   impl_->set_batch_callback(std::move(cb));
}

void DataLogger::start() { impl_->start(); }

void DataLogger::stop() noexcept {
   if (impl_) {
      impl_->stop();
   }
}

void DataLogger::run() { impl_->run(); }

std::vector<Batch> DataLogger::run_once() { return impl_->run_once(); }

bool DataLogger::is_running() const { return impl_->is_running(); }

bool DataLogger::push_batch(Batch const& batch) { return impl_->push_batch(batch); }

bool DataLogger::push_batch_json(std::string const& batch_json) {
   return impl_->push_batch_json(batch_json);
}

void DataLogger::flush() { impl_->flush_sinks(); }

LoggerStats DataLogger::stats() const { return impl_->stats(); }

std::string DataLogger::stats_json(int indent) const { return impl_->stats().to_json(indent); }

std::string DataLogger::config_json(int indent) const { return impl_->config().to_json(indent); }

std::vector<DutProbeResult> DataLogger::probe(LoggerConfig const& config,
                                              std::string const& dut_filter) {
   std::vector<DutProbeResult> results;
   for (auto const& dut : config.duts) {
      if (!dut_filter.empty()) {
         if (dut.name != dut_filter) {
            continue;
         }
      } else if (!dut.enabled) {
         continue;
      }

      DutProbeResult res;
      res.dut_name = dut.name;
      res.dut_ip = dut.dut_ip;
      auto t0 = std::chrono::steady_clock::now();
      try {
         auto poller = create_snmp_poller(dut);
         Batch b = poller->poll();
         auto t1 = std::chrono::steady_clock::now();
         res.latency_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
         res.samples = std::move(b.samples);

         bool any_ok = res.samples.empty();
         std::string first_err;
         for (auto const& s : res.samples) {
            if (s.status == 0) {
               any_ok = true;
            } else if (first_err.empty()) {
               first_err =
                   s.str_val.empty() ? "SNMP error status " + std::to_string(s.status) : s.str_val;
            }
         }
         res.reachable = any_ok;
         res.error = first_err;
      } catch (std::exception const& e) {
         auto t1 = std::chrono::steady_clock::now();
         res.latency_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
         res.reachable = false;
         res.error = e.what();
      }
      results.push_back(std::move(res));
   }
   return results;
}

std::string DataLogger::probe_json(std::string const& json_config,
                                   std::string const& dut_filter,
                                   int indent) {
   LoggerConfig cfg = LoggerConfig::from_json(json_config);
   auto results = probe(cfg, dut_filter);

   json::Value out = json::Value::object();
   out["ok"] = true;
   out["count"] = static_cast<int64_t>(results.size());
   json::Value arr = json::Value::array();
   for (auto const& r : results) {
      arr.push_back(json::Value::parse(r.to_json(-1)));
   }
   out["results"] = std::move(arr);
   return out.dump(indent);
}

std::string DataLogger::request_json(std::string const& json_config,
                                     std::string const& dut_name,
                                     std::string const& operation,
                                     std::vector<std::string> const& oids,
                                     int max_repeaters,
                                     bool record_to_sinks,
                                     int indent) {
   LoggerConfig cfg = LoggerConfig::from_json(json_config);
   DutConfig const& dut = find_dut_or_throw(cfg, dut_name);
   auto t0 = std::chrono::steady_clock::now();
   auto poller = create_snmp_poller(dut);
   Batch batch = poller->request(operation, oids, max_repeaters);
   auto t1 = std::chrono::steady_clock::now();
   double latency_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

   if (record_to_sinks) {
      persist_batch_once(cfg, batch);
   }

   json::Value out = json::Value::object();
   out["ok"] = true;
   out["dut_name"] = dut.name;
   out["dut_ip"] = dut.dut_ip;
   out["operation"] = operation;
   json::Value oid_arr = json::Value::array();
   for (auto const& o : oids) {
      oid_arr.push_back(o);
   }
   out["oids"] = std::move(oid_arr);
   out["latency_ms"] = latency_ms;
   out["recorded_to_sinks"] = record_to_sinks;
   out["batch"] = json::Value::parse(batch.to_json(-1));
   out["samples"] = out["batch"]["samples"];
   return out.dump(indent);
}

std::string DataLogger::set_json(std::string const& json_config,
                                 std::string const& dut_name,
                                 std::string const& oid,
                                 std::string const& set_type,
                                 std::string const& set_value,
                                 bool record_to_sinks,
                                 int indent) {
   LoggerConfig cfg = LoggerConfig::from_json(json_config);
   DutConfig const& dut = find_dut_or_throw(cfg, dut_name);
   auto t0 = std::chrono::steady_clock::now();
   auto poller = create_snmp_poller(dut);
   Batch batch = poller->set_oid(oid, set_type, set_value);
   auto t1 = std::chrono::steady_clock::now();
   double latency_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

   if (record_to_sinks) {
      persist_batch_once(cfg, batch);
   }

   json::Value out = json::Value::object();
   out["ok"] = true;
   out["dut_name"] = dut.name;
   out["dut_ip"] = dut.dut_ip;
   out["operation"] = "set";
   out["oid"] = oid;
   out["set_type"] = set_type;
   out["set_value"] = set_value;
   out["latency_ms"] = latency_ms;
   out["recorded_to_sinks"] = record_to_sinks;
   out["batch"] = json::Value::parse(batch.to_json(-1));
   out["samples"] = out["batch"]["samples"];
   return out.dump(indent);
}

std::string DataLogger::inspect_json(std::string const& json_config,
                                     int limit,
                                     std::string const& dut_filter,
                                     std::string const& metric_filter,
                                     int indent) {
   LoggerConfig cfg = LoggerConfig::from_json(json_config);
   auto rows = inspect_configured_sinks(cfg, limit, dut_filter, metric_filter);

   json::Value out = json::Value::object();
   out["ok"] = true;
   out["count"] = static_cast<int64_t>(rows.size());
   json::Value arr = json::Value::array();
   for (auto const& r : rows) {
      arr.push_back(json::Value::parse(r.to_json(-1)));
   }
   out["rows"] = std::move(arr);
   return out.dump(indent);
}

} // namespace ezsnmp_data_logger
