#pragma once

#ifndef SWIG

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include "ezsnmp_data_logger/config.h"
#include "ezsnmp_data_logger/sink.h"

namespace ezsnmp_data_logger {

/**
 * @brief Embedded HTTP Prometheus exposition format (/metrics) sink.
 *
 * Exposes the latest numeric SNMP readings as Prometheus gauges labeled by
 * dut_name, dut_ip, metric_name, and oid, plus operational scrape counters.
 */
class PrometheusSink : public ISink {
  public:
   explicit PrometheusSink(SinkConfig const& cfg);
   ~PrometheusSink() override;

   void write(Batch const& batch) override;
   void flush() override;
   void close() noexcept override;
   std::string name() const override { return "prometheus"; }

   /**
    * @brief Return the actual bound TCP port (useful when configured port is 0).
    * @return Bound TCP port number.
    */
   int bound_port() const noexcept { return bound_port_; }

   /**
    * @brief Render current Prometheus text exposition payload.
    * @return Prometheus text-format payload.
    */
   std::string render_metrics() const;

  private:
   std::string host_;
   int configured_port_ = 9761;
   int bound_port_ = 0;
   std::string prefix_ = "ezsnmp";

   intptr_t listen_fd_ = -1;
   std::atomic<bool> running_{false};
   std::thread server_thread_;

   struct MetricEntry {
      std::string dut_name;
      std::string dut_ip;
      std::string metric_name;
      std::string oid;
      double value = 0.0;
      int status = 0;
      int64_t ts_unix_ns = 0;
   };

   mutable std::mutex metrics_mutex_;
   std::map<std::string, MetricEntry> latest_metrics_;
   int64_t batches_exported_ = 0;

   void start_server();
   void serve_loop();
};

} // namespace ezsnmp_data_logger

#endif // SWIG
