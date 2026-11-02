#include "ezsnmp_data_logger/datatypes.h"

#include <iomanip>
#include <sstream>

#include "ezsnmp_data_logger/exceptions.h"
#include "ezsnmp_data_logger/json_utils.h"

namespace ezsnmp_data_logger {

std::string type_tag_to_string(TypeTag tag) {
   switch (tag) {
      case TypeTag::Int64:
         return "i64";
      case TypeTag::Float64:
         return "f64";
      case TypeTag::Str:
         return "str";
      case TypeTag::Null:
      default:
         return "null";
   }
}

namespace {

TypeTag parse_type_tag(json::Value const& v) {
   if (v.is_int()) {
      int code = v.as_int();
      if (code >= 0 && code <= 3) {
         return static_cast<TypeTag>(code);
      }
      throw ConfigError("Invalid numeric TypeTag value: " + std::to_string(code));
   }
   if (v.is_string()) {
      std::string const& s = v.as_string();
      if (s == "null") {
         return TypeTag::Null;
      }
      if (s == "i64" || s == "int" || s == "int64") {
         return TypeTag::Int64;
      }
      if (s == "f64" || s == "float" || s == "float64") {
         return TypeTag::Float64;
      }
      if (s == "str" || s == "string") {
         return TypeTag::Str;
      }
      throw ConfigError("Unknown string TypeTag value: " + s);
   }
   return TypeTag::Null;
}

json::Value sample_to_json_value(Sample const& s) {
   json::Value j = json::Value::object();
   j["metric_name"] = s.metric_name;
   j["oid"] = s.oid;
   j["oid_index"] = s.oid_index;
   j["snmp_type"] = s.snmp_type;
   j["raw_value"] = s.raw_value;
   j["type_tag"] = static_cast<int>(s.type_tag);
   j["type_name"] = type_tag_to_string(s.type_tag);
   j["i64"] = s.i64;
   j["f64"] = s.f64;
   j["str_val"] = s.str_val;
   j["status"] = s.status;
   return j;
}

Sample sample_from_json_value(json::Value const& j) {
   if (j.is_array()) {
      auto const& arr = j.as_array();
      if (arr.size() < 7) {
         throw ConfigError("Compact Sample array must contain 7 elements");
      }
      Sample s;
      s.metric_name = arr[0].as_string();
      s.oid = arr[1].as_string();
      s.type_tag = parse_type_tag(arr[2]);
      s.i64 = arr[3].as_int64();
      s.f64 = arr[4].as_double();
      s.str_val = arr[5].as_string();
      s.status = arr[6].as_int();
      return s;
   }
   if (!j.is_object()) {
      throw ConfigError("Sample JSON must be an object or 7-element array");
   }
   Sample s;
   s.metric_name = j.value_string("metric_name", j.value_string("oid", ""));
   s.oid = j.value_string("oid", "");
   s.oid_index = j.value_string("oid_index", j.value_string("index", ""));
   s.snmp_type = j.value_string("snmp_type", j.value_string("type", ""));
   s.raw_value = j.value_string("raw_value", j.value_string("value", ""));

   if (auto const* tt = j.find("type_tag")) {
      s.type_tag = parse_type_tag(*tt);
      s.i64 = j.value_int64("i64", 0);
      s.f64 = j.value_double("f64", 0.0);
      s.str_val = j.value_string("str_val", "");
   } else if (auto const* cv = j.find("converted_value")) {
      // Seamless ingestion of ezsnmp Result.to_dict() JSON objects!
      if (cv->is_int() || cv->is_bool()) {
         s.type_tag = TypeTag::Int64;
         s.i64 = cv->is_bool() ? (cv->as_bool() ? 1 : 0) : cv->as_int64();
      } else if (cv->is_double()) {
         s.type_tag = TypeTag::Float64;
         s.f64 = cv->as_double();
      } else if (cv->is_string()) {
         s.type_tag = TypeTag::Str;
         s.str_val = cv->as_string();
      }
   }
   s.status = j.value_int("status", 0);
   return s;
}

} // namespace

Sample Sample::make_int(std::string const& name,
                        std::string const& oid_str,
                        int64_t val,
                        int status_code) {
   Sample s;
   s.metric_name = name;
   s.oid = oid_str;
   s.snmp_type = "INTEGER";
   s.raw_value = std::to_string(val);
   s.type_tag = TypeTag::Int64;
   s.i64 = val;
   s.status = status_code;
   return s;
}

Sample Sample::make_float(std::string const& name,
                          std::string const& oid_str,
                          double val,
                          int status_code) {
   Sample s;
   s.metric_name = name;
   s.oid = oid_str;
   s.snmp_type = "OPAQUE_FLOAT";
   std::ostringstream oss;
   oss << std::setprecision(12) << val;
   s.raw_value = oss.str();
   s.type_tag = TypeTag::Float64;
   s.f64 = val;
   s.status = status_code;
   return s;
}

Sample Sample::make_string(std::string const& name,
                           std::string const& oid_str,
                           std::string const& val,
                           int status_code) {
   Sample s;
   s.metric_name = name;
   s.oid = oid_str;
   s.snmp_type = "OCTETSTR";
   s.raw_value = val;
   s.type_tag = TypeTag::Str;
   s.str_val = val;
   s.status = status_code;
   return s;
}

std::string Sample::display_value() const {
   switch (type_tag) {
      case TypeTag::Int64:
         return std::to_string(i64);
      case TypeTag::Float64: {
         std::ostringstream oss;
         oss << std::setprecision(12) << f64;
         return oss.str();
      }
      case TypeTag::Str:
         return str_val;
      case TypeTag::Null:
      default:
         return "null";
   }
}

std::string Sample::to_json(int indent) const { return sample_to_json_value(*this).dump(indent); }

Sample Sample::from_json(std::string const& json_text) {
   return sample_from_json_value(json::Value::parse(json_text));
}

std::string Batch::to_json(int indent) const {
   json::Value j = json::Value::object();
   j["ts_unix_ns"] = ts_unix_ns;
   j["dut_name"] = dut_name;
   j["dut_ip"] = dut_ip;
   json::Value arr = json::Value::array();
   for (auto const& s : samples) {
      arr.push_back(sample_to_json_value(s));
   }
   j["samples"] = std::move(arr);
   return j.dump(indent);
}

Batch Batch::from_json(std::string const& json_text) {
   json::Value j = json::Value::parse(json_text);
   if (!j.is_object()) {
      throw ConfigError("Batch JSON must be an object");
   }
   Batch b;
   b.ts_unix_ns = j.value_int64("ts_unix_ns", 0);
   b.dut_name = j.value_string("dut_name", "");
   b.dut_ip = j.value_string("dut_ip", "");
   if (auto const* sarr = j.find("samples")) {
      for (auto const& item : sarr->as_array()) {
         b.samples.push_back(sample_from_json_value(item));
      }
   }
   return b;
}

std::string DutProbeResult::to_json(int indent) const {
   json::Value j = json::Value::object();
   j["dut_name"] = dut_name;
   j["dut_ip"] = dut_ip;
   j["reachable"] = reachable;
   j["latency_ms"] = latency_ms;
   j["error"] = error;
   json::Value arr = json::Value::array();
   for (auto const& s : samples) {
      arr.push_back(sample_to_json_value(s));
   }
   j["samples"] = std::move(arr);
   return j.dump(indent);
}

std::string LoggerStats::to_json(int indent) const {
   json::Value j = json::Value::object();
   j["running"] = running;
   j["cycles_completed"] = cycles_completed;
   j["processed_batches"] = processed_batches;
   j["processed_samples"] = processed_samples;
   j["dropped_batches"] = dropped_batches;
   j["poll_errors"] = poll_errors;
   j["sink_errors"] = sink_errors;
   j["queue_depth"] = queue_depth;
   j["uptime_ms"] = uptime_ms;
   j["last_error"] = last_error;
   return j.dump(indent);
}

std::string InspectRow::to_json(int indent) const {
   json::Value j = json::Value::object();
   j["ts_unix_ns"] = ts_unix_ns;
   j["dut_name"] = dut_name;
   j["dut_ip"] = dut_ip;
   j["metric_name"] = metric_name;
   j["oid"] = oid;
   j["oid_index"] = oid_index;
   j["snmp_type"] = snmp_type;
   j["raw_value"] = raw_value;
   j["type_tag"] = static_cast<int>(type_tag);
   j["type_name"] = type_tag_to_string(type_tag);
   j["i64"] = i64;
   j["f64"] = f64;
   j["str_val"] = str_val;
   j["status"] = status;
   j["sink_type"] = sink_type;
   return j.dump(indent);
}

} // namespace ezsnmp_data_logger
