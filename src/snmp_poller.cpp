#include "ezsnmp_data_logger/snmp_poller.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>

#include "ezsnmp_data_logger/exceptions.h"

#if defined(EZSNMP_DL_HAVE_EZSNMP) && EZSNMP_DL_HAVE_EZSNMP
#include "datatypes.h"
#include "exceptionsbase.h"
#include "sessionbase.h"
#endif

namespace ezsnmp_data_logger {

namespace {

int64_t current_unix_ns() {
   auto now = std::chrono::system_clock::now().time_since_epoch();
   return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}

uint32_t stable_hash(std::string const& s) {
   uint32_t h = 2166136261u;
   for (unsigned char c : s) {
      h ^= c;
      h *= 16777619u;
   }
   return h;
}

Sample synthesize_stub_sample(std::string const& dut_name,
                              std::string const& dut_ip,
                              std::string const& metric_name,
                              std::string const& oid_str,
                              uint64_t tick) {
   uint32_t const dut_seed = stable_hash(dut_name);
   uint32_t const m_seed = stable_hash(metric_name);
   double const phase =
       static_cast<double>(tick) * 0.25 + static_cast<double>((dut_seed ^ m_seed) % 100) * 0.05;

   if (metric_name.find("Name") != std::string::npos ||
       metric_name.find("Descr") != std::string::npos ||
       metric_name.find("Location") != std::string::npos) {
      std::string val = dut_name + " (" + dut_ip + ") EzSnmp Simulated Agent";
      return Sample::make_string(metric_name, oid_str, val, 0);
   }
   if (metric_name.find("Load") != std::string::npos ||
       metric_name.find("Temp") != std::string::npos ||
       metric_name.find("Util") != std::string::npos) {
      double base = (metric_name.find("Temp") != std::string::npos) ? 36.5 : 1.25;
      double amp = (metric_name.find("Temp") != std::string::npos) ? 4.2 : 0.85;
      double val = base + amp * std::sin(phase);
      return Sample::make_float(metric_name, oid_str, val, 0);
   }
   if (metric_name.find("UpTime") != std::string::npos) {
      int64_t ticks = static_cast<int64_t>(8640000 + tick * 100 + (dut_seed % 5000));
      return Sample::make_int(metric_name, oid_str, ticks, 0);
   }
   int64_t counter = static_cast<int64_t>(100000 + tick * (512 + (m_seed % 1024)));
   return Sample::make_int(metric_name, oid_str, counter, 0);
}

} // namespace

StubSnmpPoller::StubSnmpPoller(DutConfig dut) : dut_(std::move(dut)) {}

Batch StubSnmpPoller::poll() {
   ++tick_;
   Batch batch;
   batch.ts_unix_ns = current_unix_ns();
   batch.dut_name = dut_.name;
   batch.dut_ip = dut_.dut_ip;
   batch.samples.reserve(dut_.metrics.size());

   for (auto const& m : dut_.metrics) {
      if (m.operation == "set") {
         if (m.set_type == "i" || m.set_type == "u" || m.set_type == "t") {
            batch.samples.push_back(
                Sample::make_int(m.name, m.oid, std::atoll(m.set_value.c_str()), 0));
         } else {
            batch.samples.push_back(Sample::make_string(m.name, m.oid, m.set_value, 0));
         }
      } else {
         batch.samples.push_back(
             synthesize_stub_sample(dut_.name, dut_.dut_ip, m.name, m.oid, tick_));
      }
   }
   return batch;
}

Batch StubSnmpPoller::request(std::string const& operation,
                              std::vector<std::string> const& oids,
                              int max_repeaters) {
   ++tick_;
   Batch batch;
   batch.ts_unix_ns = current_unix_ns();
   batch.dut_name = dut_.name;
   batch.dut_ip = dut_.dut_ip;

   int count = (operation == "walk" || operation == "bulkwalk") ? std::min(max_repeaters, 4) : 1;
   for (auto const& oid_str : oids) {
      for (int i = 0; i < count; ++i) {
         std::string resolved_oid = (count > 1) ? (oid_str + "." + std::to_string(i + 1)) : oid_str;
         batch.samples.push_back(
             synthesize_stub_sample(dut_.name, dut_.dut_ip, resolved_oid, resolved_oid, tick_ + i));
      }
   }
   return batch;
}

Batch StubSnmpPoller::set_oid(std::string const& oid,
                              std::string const& set_type,
                              std::string const& set_value) {
   ++tick_;
   Batch batch;
   batch.ts_unix_ns = current_unix_ns();
   batch.dut_name = dut_.name;
   batch.dut_ip = dut_.dut_ip;
   if (set_type == "i" || set_type == "u" || set_type == "t") {
      batch.samples.push_back(Sample::make_int(oid, oid, std::atoll(set_value.c_str()), 0));
   } else {
      batch.samples.push_back(Sample::make_string(oid, oid, set_value, 0));
   }
   return batch;
}

#if defined(EZSNMP_DL_HAVE_EZSNMP) && EZSNMP_DL_HAVE_EZSNMP

namespace {

int map_ezsnmp_exception_status(::GenericErrorBase const& err) {
   if (dynamic_cast<::TimeoutErrorBase const*>(&err)) {
      return -1;
   }
   if (dynamic_cast<::ConnectionErrorBase const*>(&err)) {
      return -2;
   }
   if (dynamic_cast<::UnknownObjectIDErrorBase const*>(&err)) {
      return -3;
   }
   if (dynamic_cast<::NoSuchObjectErrorBase const*>(&err)) {
      return 128;
   }
   if (dynamic_cast<::NoSuchInstanceErrorBase const*>(&err)) {
      return 129;
   }
   if (dynamic_cast<::NoSuchNameErrorBase const*>(&err)) {
      return 2;
   }
   if (dynamic_cast<::ParseErrorBase const*>(&err)) {
      return -4;
   }
   if (dynamic_cast<::PacketErrorBase const*>(&err)) {
      return -5;
   }
   return -6;
}

std::string bytes_to_hex(std::vector<unsigned char> const& bytes) {
   std::ostringstream oss;
   oss << std::hex << std::setfill('0');
   for (size_t i = 0; i < bytes.size(); ++i) {
      if (i > 0) {
         oss << ':';
      }
      oss << std::setw(2) << static_cast<unsigned int>(bytes[i]);
   }
   return oss.str();
}

Sample result_to_sample(std::string const& metric_name, ::Result const& r) {
   Sample s;
   s.metric_name = metric_name;
   s.oid = r.index.empty() ? r.oid : (r.oid + "." + r.index);
   s.oid_index = r.index;
   s.snmp_type = r.type;
   s.raw_value = r.value;
   s.status = 0;

   if (r.type == "NOSUCHOBJECT") {
      s.type_tag = TypeTag::Null;
      s.status = 128;
      return s;
   }
   if (r.type == "NOSUCHINSTANCE") {
      s.type_tag = TypeTag::Null;
      s.status = 129;
      return s;
   }
   if (r.type == "ENDOFMIBVIEW") {
      s.type_tag = TypeTag::Null;
      s.status = 130;
      return s;
   }

   // Extract strongly-typed value from ezsnmp Result::converted_value std::variant
   // Use std::get_if (matching ezsnmp's macOS compatibility best practice!)
   if (auto const* iv = std::get_if<int>(&r.converted_value)) {
      s.type_tag = TypeTag::Int64;
      s.i64 = static_cast<int64_t>(*iv);
   } else if (auto const* u32 = std::get_if<uint32_t>(&r.converted_value)) {
      s.type_tag = TypeTag::Int64;
      s.i64 = static_cast<int64_t>(*u32);
   } else if (auto const* u64 = std::get_if<uint64_t>(&r.converted_value)) {
      s.type_tag = TypeTag::Int64;
      s.i64 = static_cast<int64_t>(*u64);
   } else if (auto const* dv = std::get_if<double>(&r.converted_value)) {
      s.type_tag = TypeTag::Float64;
      s.f64 = *dv;
   } else if (auto const* sv = std::get_if<std::string>(&r.converted_value)) {
      // Check if an OCTETSTR holds a floating-point string (e.g. UCD-SNMP laLoad "0.42")
      if (!sv->empty() && sv->find('.') != std::string::npos) {
         char* end_ptr = nullptr;
         double parsed_d = std::strtod(sv->c_str(), &end_ptr);
         if (end_ptr == sv->c_str() + sv->size() && std::isfinite(parsed_d)) {
            s.type_tag = TypeTag::Float64;
            s.f64 = parsed_d;
            return s;
         }
      }
      s.type_tag = TypeTag::Str;
      s.str_val = *sv;
   } else if (auto const* bv = std::get_if<std::vector<unsigned char>>(&r.converted_value)) {
      s.type_tag = TypeTag::Str;
      s.str_val = bytes_to_hex(*bv);
   } else {
      s.type_tag = TypeTag::Str;
      s.str_val = r.value;
   }

   return s;
}

Sample error_sample(std::string const& metric_name,
                    std::string const& oid_str,
                    ::GenericErrorBase const& err) {
   Sample s;
   s.metric_name = metric_name;
   s.oid = oid_str;
   s.type_tag = TypeTag::Str;
   s.str_val = err.what();
   s.raw_value = err.what();
   s.snmp_type = "ERROR";
   s.status = map_ezsnmp_exception_status(err);
   return s;
}

struct ScopedMaxRepeaters {
   ::SessionBase& session;
   ScopedMaxRepeaters(::SessionBase& s, int max_rep) : session(s) {
      session._set_max_repeaters_to_num(std::to_string(max_rep > 0 ? max_rep : 10));
   }
   ~ScopedMaxRepeaters() { session._set_max_repeaters_to_num(""); }
};

} // namespace

EzSnmpPoller::EzSnmpPoller(DutConfig dut) : dut_(std::move(dut)) {
   auto const& sc = dut_.snmp;
   session_ = std::make_unique<::SessionBase>(
       dut_.dut_ip, std::to_string(sc.port_number), sc.version, sc.community,
       sc.version == "3" ? sc.auth_protocol : "", sc.version == "3" ? sc.auth_passphrase : "",
       sc.version == "3" ? sc.security_engine_id : "",
       sc.version == "3" ? sc.context_engine_id : "", sc.version == "3" ? sc.security_level : "",
       sc.version == "3" ? sc.context : "", sc.version == "3" ? sc.security_username : "",
       sc.version == "3" ? sc.privacy_protocol : "", sc.version == "3" ? sc.privacy_passphrase : "",
       sc.version == "3" ? sc.boots_time : "", std::to_string(sc.retries),
       std::to_string(sc.timeout_seconds), sc.load_mibs, sc.mib_directories,
       sc.print_enums_numerically, sc.print_full_oids, sc.print_oids_numerically,
       sc.print_timeticks_numerically, sc.print_hex_strings, "");
}

EzSnmpPoller::~EzSnmpPoller() = default;

Batch EzSnmpPoller::poll() {
   Batch batch;
   batch.ts_unix_ns = current_unix_ns();
   batch.dut_name = dut_.name;
   batch.dut_ip = dut_.dut_ip;

   if (dut_.metrics.empty()) {
      return batch;
   }

   // Coalesce simple "get" metrics into a single multi-OID SessionBase::get(vector) call
   std::vector<MetricConfig const*> get_metrics;
   std::vector<std::string> get_oids;
   std::vector<MetricConfig const*> individual_metrics;

   for (auto const& m : dut_.metrics) {
      if (m.operation == "get") {
         get_metrics.push_back(&m);
         get_oids.push_back(m.oid);
      } else {
         individual_metrics.push_back(&m);
      }
   }

   if (!get_oids.empty()) {
      try {
         std::vector<::Result> results = session_->get(get_oids);
         for (size_t i = 0; i < results.size() && i < get_metrics.size(); ++i) {
            batch.samples.push_back(result_to_sample(get_metrics[i]->name, results[i]));
         }
      } catch (::GenericErrorBase const& err) {
         for (auto const* mc : get_metrics) {
            batch.samples.push_back(error_sample(mc->name, mc->oid, err));
         }
      }
   }

   for (auto const* mc : individual_metrics) {
      try {
         std::vector<::Result> results;
         if (mc->operation == "getnext") {
            results = session_->get_next(mc->oid);
         } else if (mc->operation == "walk") {
            results = session_->walk(mc->oid);
         } else if (mc->operation == "bulkget") {
            ScopedMaxRepeaters guard(*session_, mc->max_repeaters);
            results = session_->bulk_get(mc->oid);
         } else if (mc->operation == "bulkwalk") {
            ScopedMaxRepeaters guard(*session_, mc->max_repeaters);
            results = session_->bulk_walk(mc->oid);
         } else if (mc->operation == "set") {
            results = session_->set({mc->oid, mc->set_type, mc->set_value});
         }

         if (results.size() == 1) {
            batch.samples.push_back(result_to_sample(mc->name, results[0]));
         } else {
            for (size_t idx = 0; idx < results.size(); ++idx) {
               std::string sname = results[idx].index.empty()
                                       ? (mc->name + "." + std::to_string(idx))
                                       : (mc->name + "." + results[idx].index);
               batch.samples.push_back(result_to_sample(sname, results[idx]));
            }
         }
      } catch (::GenericErrorBase const& err) {
         batch.samples.push_back(error_sample(mc->name, mc->oid, err));
      }
   }

   return batch;
}

Batch EzSnmpPoller::request(std::string const& operation,
                            std::vector<std::string> const& oids,
                            int max_repeaters) {
   Batch batch;
   batch.ts_unix_ns = current_unix_ns();
   batch.dut_name = dut_.name;
   batch.dut_ip = dut_.dut_ip;

   try {
      std::vector<::Result> results;
      if (operation == "get") {
         results = session_->get(oids);
      } else if (operation == "getnext") {
         results = session_->get_next(oids);
      } else if (operation == "walk") {
         for (auto const& oid_str : oids) {
            auto sub = session_->walk(oid_str);
            results.insert(results.end(), sub.begin(), sub.end());
         }
      } else if (operation == "bulkget") {
         ScopedMaxRepeaters guard(*session_, max_repeaters);
         results = session_->bulk_get(oids);
      } else if (operation == "bulkwalk") {
         ScopedMaxRepeaters guard(*session_, max_repeaters);
         results = session_->bulk_walk(oids);
      } else {
         throw SnmpError("Unsupported ezsnmp request operation: '" + operation + "'");
      }

      for (auto const& r : results) {
         std::string name = r.index.empty() ? r.oid : (r.oid + "." + r.index);
         batch.samples.push_back(result_to_sample(name, r));
      }
   } catch (::GenericErrorBase const& err) {
      throw SnmpError(std::string("ezsnmp ") + operation + " failed on DUT '" + dut_.name +
                      "': " + err.what());
   }

   return batch;
}

Batch EzSnmpPoller::set_oid(std::string const& oid,
                            std::string const& set_type,
                            std::string const& set_value) {
   Batch batch;
   batch.ts_unix_ns = current_unix_ns();
   batch.dut_name = dut_.name;
   batch.dut_ip = dut_.dut_ip;

   try {
      std::vector<::Result> results = session_->set({oid, set_type, set_value});
      for (auto const& r : results) {
         std::string name = r.index.empty() ? r.oid : (r.oid + "." + r.index);
         batch.samples.push_back(result_to_sample(name, r));
      }
   } catch (::GenericErrorBase const& err) {
      throw SnmpError(std::string("ezsnmp set failed on DUT '") + dut_.name + "': " + err.what());
   }

   return batch;
}

#else

EzSnmpPoller::EzSnmpPoller(DutConfig dut) : dut_(std::move(dut)) {
   throw SnmpError(
       "ezsnmp C++ SessionBase support was not compiled into ezsnmp_data_logger. "
       "Rebuild with ezsnmp + Net-SNMP available or set snmp.mode to 'stub'.");
}

EzSnmpPoller::~EzSnmpPoller() = default;
Batch EzSnmpPoller::poll() { return {}; }
Batch EzSnmpPoller::request(std::string const&, std::vector<std::string> const&, int) { return {}; }
Batch EzSnmpPoller::set_oid(std::string const&, std::string const&, std::string const&) {
   return {};
}

#endif

std::unique_ptr<ISnmpPoller> create_snmp_poller(DutConfig const& dut) {
   if (dut.snmp.mode == "stub") {
      return std::make_unique<StubSnmpPoller>(dut);
   }
   return std::make_unique<EzSnmpPoller>(dut);
}

#if defined(EZSNMP_DL_HAVE_EZSNMP) && EZSNMP_DL_HAVE_EZSNMP
Sample test_result_to_sample(std::string const& metric_name, ::Result const& r) {
   return result_to_sample(metric_name, r);
}

int test_map_ezsnmp_exception(::GenericErrorBase const& err) {
   return map_ezsnmp_exception_status(err);
}
#endif

} // namespace ezsnmp_data_logger
