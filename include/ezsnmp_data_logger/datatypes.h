#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ezsnmp_data_logger {

/**
 * @brief Discriminator tag identifying which typed field of Sample is active.
 */
enum class TypeTag : int { Null = 0, Int64 = 1, Float64 = 2, Str = 3 };

/**
 * @brief Convert a TypeTag enum value into a human-readable string ("null", "i64", "f64", "str").
 * @param tag TypeTag value.
 * @return Short lowercase type name.
 */
std::string type_tag_to_string(TypeTag tag);

/**
 * @brief Single SNMP metric reading captured via ezsnmp for a Device Under Test (DUT).
 *
 * Preserves the full ezsnmp Result metadata (`oid`, `oid_index`, `snmp_type`, `raw_value`)
 * alongside normalized typed storage fields (`type_tag`, `i64`, `f64`, `str_val`, `status`).
 */
struct Sample {
   std::string metric_name;
   std::string oid;
   std::string oid_index;
   std::string snmp_type;
   std::string raw_value;
   TypeTag type_tag = TypeTag::Null;
   int64_t i64 = 0;
   double f64 = 0.0;
   std::string str_val;
   int status = 0;

   /**
    * @brief Construct an empty null sample.
    */
   Sample() = default;

   /**
    * @brief Create an integer sample.
    */
   static Sample make_int(std::string const& name,
                          std::string const& oid_str,
                          int64_t val,
                          int status_code = 0);

   /**
    * @brief Create a floating-point sample.
    */
   static Sample make_float(std::string const& name,
                            std::string const& oid_str,
                            double val,
                            int status_code = 0);

   /**
    * @brief Create a string sample.
    */
   static Sample make_string(std::string const& name,
                             std::string const& oid_str,
                             std::string const& val,
                             int status_code = 0);

   /**
    * @brief Render the active value as a display string regardless of type_tag.
    * @return String representation of the sample value.
    */
   std::string display_value() const;

   /**
    * @brief Serialize this sample to a JSON string.
    * @param indent Indentation spaces (-1 for compact).
    * @return JSON object string.
    */
   std::string to_json(int indent = -1) const;

   /**
    * @brief Parse a Sample from a JSON object string (including ezsnmp Result.to_dict() JSON).
    * @param json_text JSON string representing a sample or ezsnmp Result.
    * @return Parsed Sample.
    */
   static Sample from_json(std::string const& json_text);

#ifndef SWIG
   bool operator==(Sample const& o) const {
      return metric_name == o.metric_name && oid == o.oid && oid_index == o.oid_index &&
             type_tag == o.type_tag && i64 == o.i64 && f64 == o.f64 && str_val == o.str_val &&
             status == o.status;
   }
#endif
};

/**
 * @brief Timestamped collection of SNMP samples polled from a single DUT.
 */
struct Batch {
   int64_t ts_unix_ns = 0;
   std::string dut_name;
   std::string dut_ip;
   std::vector<Sample> samples;

   /**
    * @brief Serialize this Batch to a JSON string.
    * @param indent Indentation spaces (-1 for compact).
    * @return JSON object string.
    */
   std::string to_json(int indent = -1) const;

   /**
    * @brief Parse a Batch from a JSON object string.
    * @param json_text JSON string representing a batch.
    * @return Parsed Batch.
    */
   static Batch from_json(std::string const& json_text);

#ifndef SWIG
   bool operator==(Batch const& o) const {
      return ts_unix_ns == o.ts_unix_ns && dut_name == o.dut_name && dut_ip == o.dut_ip &&
             samples == o.samples;
   }
#endif
};

/**
 * @brief Diagnostic result from probing or requesting a DUT via ezsnmp.
 */
struct DutProbeResult {
   std::string dut_name;
   std::string dut_ip;
   bool reachable = false;
   double latency_ms = 0.0;
   std::string error;
   std::vector<Sample> samples;

   /**
    * @brief Serialize this DutProbeResult to a JSON string.
    * @param indent Indentation spaces (-1 for compact).
    * @return JSON object string.
    */
   std::string to_json(int indent = -1) const;

#ifndef SWIG
   bool operator==(DutProbeResult const& o) const {
      return dut_name == o.dut_name && dut_ip == o.dut_ip && reachable == o.reachable &&
             error == o.error;
   }
#endif
};

/**
 * @brief Live operational telemetry counters for a DataLogger instance.
 */
struct LoggerStats {
   bool running = false;
   int64_t cycles_completed = 0;
   int64_t processed_batches = 0;
   int64_t processed_samples = 0;
   int64_t dropped_batches = 0;
   int64_t poll_errors = 0;
   int64_t sink_errors = 0;
   int64_t queue_depth = 0;
   int64_t uptime_ms = 0;
   std::string last_error;

   /**
    * @brief Serialize LoggerStats to a JSON string.
    * @param indent Indentation spaces (-1 for compact).
    * @return JSON object string.
    */
   std::string to_json(int indent = -1) const;

#ifndef SWIG
   bool operator==(LoggerStats const& o) const {
      return running == o.running && cycles_completed == o.cycles_completed &&
             processed_batches == o.processed_batches && processed_samples == o.processed_samples;
   }
#endif
};

/**
 * @brief Flattened stored telemetry record returned when inspecting configured sinks.
 */
struct InspectRow {
   int64_t ts_unix_ns = 0;
   std::string dut_name;
   std::string dut_ip;
   std::string metric_name;
   std::string oid;
   std::string oid_index;
   std::string snmp_type;
   std::string raw_value;
   TypeTag type_tag = TypeTag::Null;
   int64_t i64 = 0;
   double f64 = 0.0;
   std::string str_val;
   int status = 0;
   std::string sink_type;

   /**
    * @brief Serialize InspectRow to a JSON string.
    * @param indent Indentation spaces (-1 for compact).
    * @return JSON object string.
    */
   std::string to_json(int indent = -1) const;

#ifndef SWIG
   bool operator==(InspectRow const& o) const {
      return ts_unix_ns == o.ts_unix_ns && dut_name == o.dut_name && metric_name == o.metric_name &&
             oid == o.oid && sink_type == o.sink_type;
   }
#endif
};

} // namespace ezsnmp_data_logger
