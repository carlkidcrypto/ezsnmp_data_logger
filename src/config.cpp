#include "ezsnmp_data_logger/config.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

#include "ezsnmp_data_logger/exceptions.h"
#include "ezsnmp_data_logger/json_utils.h"
#include "ezsnmp_data_logger/version.h"

namespace ezsnmp_data_logger {

namespace {

bool is_one_of(std::string const& val, std::initializer_list<char const*> allowed) {
   for (char const* item : allowed) {
      if (val == item) {
         return true;
      }
   }
   return false;
}

std::string normalize_snmp_version(std::string const& v) {
   if (v == "2") {
      return "2c";
   }
   return v;
}

std::string normalize_snmp_mode(std::string const& m) {
   if (m == "netsnmp") {
      return "ezsnmp";
   }
   return m;
}

LoggerConfig parse_from_value(json::Value const& root, ValidationReport& report) {
   if (!root.is_object()) {
      report.errors.push_back("Top-level JSON configuration must be an object");
      return {};
   }

   LoggerConfig cfg;

   cfg.logger.poll_interval_ms = root.value_int("poll_interval_ms", cfg.logger.poll_interval_ms);
   cfg.logger.queue_depth = root.value_int("queue_depth", cfg.logger.queue_depth);
   cfg.logger.worker_threads = root.value_int("worker_threads", cfg.logger.worker_threads);
   cfg.logger.stats_interval_ms = root.value_int("stats_interval_ms", cfg.logger.stats_interval_ms);
   cfg.logger.max_cycles = root.value_int("max_cycles", cfg.logger.max_cycles);
   cfg.logger.run_duration_seconds =
       root.value_int("run_duration_seconds", cfg.logger.run_duration_seconds);
   cfg.logger.log_level = root.value_string("log_level", cfg.logger.log_level);
   cfg.logger.status_file = root.value_string("status_file", cfg.logger.status_file);
   cfg.logger.zmq_endpoint = root.value_string("zmq_endpoint", cfg.logger.zmq_endpoint);
   cfg.logger.zmq_hwm = root.value_int("zmq_hwm", cfg.logger.zmq_hwm);
   cfg.logger.process_start_method =
       root.value_string("process_start_method", cfg.logger.process_start_method);

   if (auto const* lobj = root.find("logger")) {
      if (!lobj->is_object()) {
         report.errors.push_back("'logger' section must be a JSON object");
      } else {
         cfg.logger.poll_interval_ms =
             lobj->value_int("poll_interval_ms", cfg.logger.poll_interval_ms);
         cfg.logger.queue_depth = lobj->value_int("queue_depth", cfg.logger.queue_depth);
         cfg.logger.worker_threads = lobj->value_int("worker_threads", cfg.logger.worker_threads);
         cfg.logger.stats_interval_ms =
             lobj->value_int("stats_interval_ms", cfg.logger.stats_interval_ms);
         cfg.logger.max_cycles = lobj->value_int("max_cycles", cfg.logger.max_cycles);
         cfg.logger.run_duration_seconds =
             lobj->value_int("run_duration_seconds", cfg.logger.run_duration_seconds);
         cfg.logger.log_level = lobj->value_string("log_level", cfg.logger.log_level);
         cfg.logger.status_file = lobj->value_string("status_file", cfg.logger.status_file);
         cfg.logger.zmq_endpoint = lobj->value_string("zmq_endpoint", cfg.logger.zmq_endpoint);
         cfg.logger.zmq_hwm = lobj->value_int("zmq_hwm", cfg.logger.zmq_hwm);
         cfg.logger.process_start_method =
             lobj->value_string("process_start_method", cfg.logger.process_start_method);
      }
   }

   if (cfg.logger.poll_interval_ms < 10) {
      report.errors.push_back("logger.poll_interval_ms must be >= 10");
   }
   if (cfg.logger.queue_depth < 1) {
      report.errors.push_back("logger.queue_depth must be >= 1");
   }
   if (cfg.logger.worker_threads < 1) {
      report.errors.push_back("logger.worker_threads must be >= 1");
   }
   if (cfg.logger.stats_interval_ms < 50) {
      report.errors.push_back("logger.stats_interval_ms must be >= 50");
   }
   if (!is_one_of(cfg.logger.log_level, {"debug", "info", "warn", "warning", "error"})) {
      report.errors.push_back("Invalid logger.log_level: '" + cfg.logger.log_level + "'");
   }

   std::set<std::string> dut_names;
   if (auto const* darr = root.find("duts")) {
      if (!darr->is_array()) {
         report.errors.push_back("'duts' must be a JSON array");
      } else {
         for (size_t di = 0; di < darr->size(); ++di) {
            auto const& dj = (*darr)[di];
            std::string prefix = "duts[" + std::to_string(di) + "]";
            if (!dj.is_object()) {
               report.errors.push_back(prefix + " must be a JSON object");
               continue;
            }
            DutConfig dc;
            dc.name = dj.value_string("name", "");
            dc.dut_ip = dj.value_string("dut_ip", dj.value_string("hostname", ""));
            dc.enabled = dj.value_bool("enabled", true);
            dc.poll_interval_ms = dj.value_int("poll_interval_ms", 0);

            if (dc.name.empty()) {
               report.errors.push_back(prefix + ".name must not be empty");
            } else if (!dut_names.insert(dc.name).second) {
               report.errors.push_back("Duplicate DUT name: '" + dc.name + "'");
            }
            if (dc.dut_ip.empty()) {
               report.errors.push_back(prefix + ".dut_ip must not be empty");
            }
            if (dc.poll_interval_ms != 0 && dc.poll_interval_ms < 10) {
               report.errors.push_back(prefix + ".poll_interval_ms must be 0 (inherit) or >= 10");
            }

            if (auto const* sobj = dj.find("snmp")) {
               if (!sobj->is_object()) {
                  report.errors.push_back(prefix + ".snmp must be a JSON object");
               } else {
                  dc.snmp.mode = normalize_snmp_mode(sobj->value_string("mode", dc.snmp.mode));
                  dc.snmp.version =
                      normalize_snmp_version(sobj->value_string("version", dc.snmp.version));
                  dc.snmp.community = sobj->value_string("community", dc.snmp.community);
                  dc.snmp.port_number = sobj->value_int("port_number", dc.snmp.port_number);
                  int legacy_timeout = sobj->value_int("timeout", dc.snmp.timeout_seconds);
                  dc.snmp.timeout_seconds = sobj->value_int("timeout_seconds", legacy_timeout);
                  dc.snmp.retries = sobj->value_int("retries", dc.snmp.retries);
                  dc.snmp.security_level =
                      sobj->value_string("security_level", dc.snmp.security_level);
                  dc.snmp.security_username =
                      sobj->value_string("security_username", dc.snmp.security_username);
                  dc.snmp.auth_protocol =
                      sobj->value_string("auth_protocol", dc.snmp.auth_protocol);
                  dc.snmp.auth_passphrase =
                      sobj->value_string("auth_passphrase", dc.snmp.auth_passphrase);
                  dc.snmp.privacy_protocol =
                      sobj->value_string("privacy_protocol", dc.snmp.privacy_protocol);
                  dc.snmp.privacy_passphrase =
                      sobj->value_string("privacy_passphrase", dc.snmp.privacy_passphrase);
                  dc.snmp.context = sobj->value_string("context", dc.snmp.context);
                  dc.snmp.security_engine_id =
                      sobj->value_string("security_engine_id", dc.snmp.security_engine_id);
                  dc.snmp.context_engine_id =
                      sobj->value_string("context_engine_id", dc.snmp.context_engine_id);
                  dc.snmp.boots_time = sobj->value_string("boots_time", dc.snmp.boots_time);
                  dc.snmp.load_mibs = sobj->value_string("load_mibs", dc.snmp.load_mibs);
                  dc.snmp.mib_directories =
                      sobj->value_string("mib_directories", dc.snmp.mib_directories);
                  dc.snmp.print_enums_numerically =
                      sobj->value_bool("print_enums_numerically", dc.snmp.print_enums_numerically);
                  dc.snmp.print_full_oids =
                      sobj->value_bool("print_full_oids", dc.snmp.print_full_oids);
                  dc.snmp.print_oids_numerically =
                      sobj->value_bool("print_oids_numerically", dc.snmp.print_oids_numerically);
                  dc.snmp.print_timeticks_numerically = sobj->value_bool(
                      "print_timeticks_numerically", dc.snmp.print_timeticks_numerically);
                  dc.snmp.print_hex_strings =
                      sobj->value_bool("print_hex_strings", dc.snmp.print_hex_strings);
               }
            }

            if (!is_one_of(dc.snmp.mode, {"ezsnmp", "stub"})) {
               report.errors.push_back(prefix + ".snmp.mode must be 'ezsnmp' or 'stub'");
            }
            if (!is_one_of(dc.snmp.version, {"1", "2c", "3"})) {
               report.errors.push_back(prefix + ".snmp.version must be '1', '2c', or '3'");
            }
            if (dc.snmp.port_number < 1 || dc.snmp.port_number > 65535) {
               report.errors.push_back(prefix + ".snmp.port_number must be in range 1..65535");
            }
            if (dc.snmp.timeout_seconds < 1) {
               report.errors.push_back(prefix + ".snmp.timeout_seconds must be >= 1");
            }
            if (dc.snmp.retries < 0) {
               report.errors.push_back(prefix + ".snmp.retries must be >= 0");
            }
            if (dc.snmp.version == "3") {
               if (!is_one_of(dc.snmp.security_level, {"noAuthNoPriv", "authNoPriv", "authPriv"})) {
                  report.errors.push_back(prefix +
                                          ".snmp.security_level must be noAuthNoPriv, authNoPriv, "
                                          "or authPriv");
               }
               if (dc.snmp.security_username.empty()) {
                  report.errors.push_back(prefix +
                                          ".snmp.security_username is required for SNMPv3");
               }
               if (dc.snmp.security_level == "authNoPriv" || dc.snmp.security_level == "authPriv") {
                  if (!is_one_of(dc.snmp.auth_protocol, {"MD5", "SHA", "SHA-1", "SHA-224",
                                                         "SHA-256", "SHA-384", "SHA-512"})) {
                     report.errors.push_back(prefix + ".snmp.auth_protocol '" +
                                             dc.snmp.auth_protocol + "' is unsupported");
                  }
                  if (dc.snmp.auth_passphrase.size() < 8) {
                     report.errors.push_back(
                         prefix + ".snmp.auth_passphrase must be at least 8 characters for USM");
                  }
               }
               if (dc.snmp.security_level == "authPriv") {
                  if (!is_one_of(dc.snmp.privacy_protocol,
                                 {"DES", "AES", "AES-128", "AES-192", "AES-256"})) {
                     report.errors.push_back(prefix + ".snmp.privacy_protocol '" +
                                             dc.snmp.privacy_protocol + "' is unsupported");
                  }
                  if (dc.snmp.privacy_passphrase.size() < 8) {
                     report.errors.push_back(
                         prefix + ".snmp.privacy_passphrase must be at least 8 characters for USM");
                  }
               }
            } else if (dc.snmp.community.empty()) {
               report.errors.push_back(prefix + ".snmp.community is required for SNMPv1/v2c");
            }

            if (dc.enabled && dc.snmp.mode == "ezsnmp" && !has_ezsnmp_support()) {
               report.warnings.push_back(
                   prefix + " uses mode='ezsnmp', which was not compiled into this binary");
            }

            std::set<std::string> metric_names;
            if (auto const* marr = dj.find("metrics")) {
               if (!marr->is_array()) {
                  report.errors.push_back(prefix + ".metrics must be a JSON array");
               } else {
                  for (size_t mi = 0; mi < marr->size(); ++mi) {
                     auto const& mj = (*marr)[mi];
                     std::string mprefix = prefix + ".metrics[" + std::to_string(mi) + "]";
                     if (!mj.is_object()) {
                        report.errors.push_back(mprefix + " must be a JSON object");
                        continue;
                     }
                     MetricConfig mc;
                     mc.name = mj.value_string("name", "");
                     mc.oid = mj.value_string("oid", "");
                     mc.operation = mj.value_string("operation", "get");
                     mc.max_repeaters = mj.value_int("max_repeaters", 10);
                     mc.set_type = mj.value_string("set_type", "s");
                     mc.set_value = mj.value_string("set_value", "");

                     if (mc.name.empty()) {
                        report.errors.push_back(mprefix + ".name must not be empty");
                     } else if (!metric_names.insert(mc.name).second) {
                        report.errors.push_back("Duplicate metric name '" + mc.name + "' in DUT '" +
                                                dc.name + "'");
                     }
                     if (mc.oid.empty()) {
                        report.errors.push_back(mprefix + ".oid must not be empty");
                     }
                     if (!is_one_of(mc.operation,
                                    {"get", "getnext", "walk", "bulkget", "bulkwalk", "set"})) {
                        report.errors.push_back(mprefix + ".operation '" + mc.operation +
                                                "' is invalid");
                     }
                     if ((mc.operation == "bulkget" || mc.operation == "bulkwalk") &&
                         dc.snmp.version == "1") {
                        report.errors.push_back(mprefix + " cannot use '" + mc.operation +
                                                "' with SNMPv1");
                     }
                     if (mc.operation == "set" && mc.set_type.empty()) {
                        report.errors.push_back(mprefix +
                                                ".set_type is required when operation='set'");
                     }
                     if (mc.max_repeaters < 1) {
                        report.errors.push_back(mprefix + ".max_repeaters must be >= 1");
                     }
                     dc.metrics.push_back(std::move(mc));
                  }
               }
            }
            if (dc.enabled && dc.metrics.empty()) {
               report.warnings.push_back("DUT '" + dc.name + "' is enabled but has no metrics");
            }
            if (dc.enabled) {
               ++report.enabled_duts;
               report.total_metrics += static_cast<int>(dc.metrics.size());
            }
            cfg.duts.push_back(std::move(dc));
         }
      }
   }

   if (auto const* sarr = root.find("sinks")) {
      if (!sarr->is_array()) {
         report.errors.push_back("'sinks' must be a JSON array");
      } else {
         for (size_t si = 0; si < sarr->size(); ++si) {
            auto const& sj = (*sarr)[si];
            std::string sprefix = "sinks[" + std::to_string(si) + "]";
            if (!sj.is_object()) {
               report.errors.push_back(sprefix + " must be a JSON object");
               continue;
            }
            SinkConfig sc;
            sc.type = sj.value_string("type", "");
            sc.enabled = sj.value_bool("enabled", true);

            if (sc.type == "csv") {
               sc.dir = sj.value_string("dir", "./data");
               sc.flush_every_n = sj.value_int("flush_every_n", 64);
               if (sc.dir.empty()) {
                  report.errors.push_back(sprefix + ".dir must not be empty for csv sink");
               }
               if (sc.flush_every_n < 1) {
                  report.errors.push_back(sprefix + ".flush_every_n must be >= 1");
               }
            } else if (sc.type == "jsonl") {
               sc.dir = sj.value_string("dir", "./data");
               sc.flush_every_n = sj.value_int("flush_every_n", 64);
               if (sc.dir.empty()) {
                  report.errors.push_back(sprefix + ".dir must not be empty for jsonl sink");
               }
               if (sc.flush_every_n < 1) {
                  report.errors.push_back(sprefix + ".flush_every_n must be >= 1");
               }
            } else if (sc.type == "sqlite3") {
               sc.sqlite_path = sj.value_string("path", "./data/logger.sqlite");
               sc.sqlite_commit_every_n = sj.value_int("commit_every_n", 100);
               sc.sqlite_commit_every_ms = sj.value_int("commit_every_ms", 500);
               if (sc.sqlite_path.empty()) {
                  report.errors.push_back(sprefix + ".path must not be empty for sqlite3 sink");
               }
               if (sc.sqlite_commit_every_n < 1) {
                  report.errors.push_back(sprefix + ".commit_every_n must be >= 1");
               }
               if (sc.sqlite_commit_every_ms < 1) {
                  report.errors.push_back(sprefix + ".commit_every_ms must be >= 1");
               }
            } else if (sc.type == "psql") {
               sc.psql_conninfo = sj.value_string("conninfo", "");
               sc.psql_batch_size = sj.value_int("batch_size", 100);
               sc.psql_reconnect_delay_ms = sj.value_int("reconnect_delay_ms", 2000);
               sc.psql_enable_timescaledb = sj.value_bool("enable_timescaledb", false);
               if (sc.enabled && sc.psql_conninfo.empty()) {
                  report.errors.push_back(sprefix +
                                          ".conninfo must not be empty for enabled psql sink");
               }
               if (sc.psql_batch_size < 1) {
                  report.errors.push_back(sprefix + ".batch_size must be >= 1");
               }
               if (sc.enabled && !has_psql_support()) {
                  report.warnings.push_back(
                      sprefix +
                      " (psql) is enabled, but libpq support was not compiled into this binary");
               }
            } else if (sc.type == "prometheus") {
               sc.prometheus_host = sj.value_string("host", "127.0.0.1");
               sc.prometheus_port = sj.value_int("port", 9761);
               sc.prometheus_prefix = sj.value_string("metric_prefix", "ezsnmp");
               if (sc.prometheus_port < 0 || sc.prometheus_port > 65535) {
                  report.errors.push_back(sprefix + ".port must be in range 0..65535");
               }
               if (sc.prometheus_prefix.empty()) {
                  report.errors.push_back(sprefix + ".metric_prefix must not be empty");
               }
            } else {
               report.errors.push_back("Unknown sink type in " + sprefix + ": '" + sc.type + "'");
            }

            if (sc.enabled) {
               ++report.enabled_sinks;
            }
            cfg.sinks.push_back(std::move(sc));
         }
      }
   }

   if (report.enabled_sinks == 0) {
      report.warnings.push_back(
          "No enabled storage sinks configured; samples will not be persisted");
   }

   if (auto const* gobj = root.find("gui")) {
      if (!gobj->is_object()) {
         report.errors.push_back("'gui' must be a JSON object");
      } else {
         cfg.gui.frontend = gobj->value_string("frontend", cfg.gui.frontend);
         cfg.gui.host = gobj->value_string("host", cfg.gui.host);
         cfg.gui.port = gobj->value_int("port", cfg.gui.port);
         cfg.gui.theme = gobj->value_string("theme", cfg.gui.theme);
         cfg.gui.window_width = gobj->value_int("window_width", cfg.gui.window_width);
         cfg.gui.window_height = gobj->value_int("window_height", cfg.gui.window_height);
         cfg.gui.refresh_interval_ms =
             gobj->value_int("refresh_interval_ms", cfg.gui.refresh_interval_ms);
         cfg.gui.open_browser = gobj->value_bool("open_browser", cfg.gui.open_browser);
         cfg.gui.cli_executable = gobj->value_string("cli_executable", cfg.gui.cli_executable);
         cfg.gui.max_log_lines = gobj->value_int("max_log_lines", cfg.gui.max_log_lines);

         if (!is_one_of(cfg.gui.frontend, {"auto", "tk", "web"})) {
            report.errors.push_back("gui.frontend must be 'auto', 'tk', or 'web'");
         }
         if (cfg.gui.port < 0 || cfg.gui.port > 65535) {
            report.errors.push_back("gui.port must be in range 0..65535");
         }
         if (!is_one_of(cfg.gui.theme, {"dark", "light"})) {
            report.errors.push_back("gui.theme must be 'dark' or 'light'");
         }
         if (cfg.gui.window_width < 400 || cfg.gui.window_height < 300) {
            report.errors.push_back("gui.window_width/window_height must be >= 400x300");
         }
         if (cfg.gui.refresh_interval_ms < 100) {
            report.errors.push_back("gui.refresh_interval_ms must be >= 100");
         }
         if (cfg.gui.max_log_lines < 10) {
            report.errors.push_back("gui.max_log_lines must be >= 10");
         }
      }
   }

   report.valid = report.errors.empty();
   return cfg;
}

} // namespace

std::string LoggerConfig::to_json(int indent) const {
   json::Value root = json::Value::object();

   json::Value lobj = json::Value::object();
   lobj["poll_interval_ms"] = logger.poll_interval_ms;
   lobj["queue_depth"] = logger.queue_depth;
   lobj["worker_threads"] = logger.worker_threads;
   lobj["stats_interval_ms"] = logger.stats_interval_ms;
   lobj["max_cycles"] = logger.max_cycles;
   lobj["run_duration_seconds"] = logger.run_duration_seconds;
   lobj["log_level"] = logger.log_level;
   lobj["status_file"] = logger.status_file;
   root["logger"] = std::move(lobj);

   json::Value darr = json::Value::array();
   for (auto const& d : duts) {
      json::Value dj = json::Value::object();
      dj["name"] = d.name;
      dj["dut_ip"] = d.dut_ip;
      dj["enabled"] = d.enabled;
      dj["poll_interval_ms"] = d.poll_interval_ms;

      json::Value sj = json::Value::object();
      sj["mode"] = d.snmp.mode;
      sj["version"] = d.snmp.version;
      sj["community"] = d.snmp.community;
      sj["port_number"] = d.snmp.port_number;
      sj["timeout_seconds"] = d.snmp.timeout_seconds;
      sj["retries"] = d.snmp.retries;
      sj["load_mibs"] = d.snmp.load_mibs;
      sj["mib_directories"] = d.snmp.mib_directories;
      sj["print_enums_numerically"] = d.snmp.print_enums_numerically;
      sj["print_full_oids"] = d.snmp.print_full_oids;
      sj["print_oids_numerically"] = d.snmp.print_oids_numerically;
      sj["print_timeticks_numerically"] = d.snmp.print_timeticks_numerically;
      sj["print_hex_strings"] = d.snmp.print_hex_strings;
      if (d.snmp.version == "3") {
         sj["security_level"] = d.snmp.security_level;
         sj["security_username"] = d.snmp.security_username;
         sj["auth_protocol"] = d.snmp.auth_protocol;
         sj["auth_passphrase"] = d.snmp.auth_passphrase;
         sj["privacy_protocol"] = d.snmp.privacy_protocol;
         sj["privacy_passphrase"] = d.snmp.privacy_passphrase;
         sj["context"] = d.snmp.context;
         sj["security_engine_id"] = d.snmp.security_engine_id;
         sj["context_engine_id"] = d.snmp.context_engine_id;
         sj["boots_time"] = d.snmp.boots_time;
      }
      dj["snmp"] = std::move(sj);

      json::Value marr = json::Value::array();
      for (auto const& m : d.metrics) {
         json::Value mj = json::Value::object();
         mj["name"] = m.name;
         mj["oid"] = m.oid;
         mj["operation"] = m.operation;
         if (m.operation == "bulkget" || m.operation == "bulkwalk") {
            mj["max_repeaters"] = m.max_repeaters;
         }
         if (m.operation == "set") {
            mj["set_type"] = m.set_type;
            mj["set_value"] = m.set_value;
         }
         marr.push_back(std::move(mj));
      }
      dj["metrics"] = std::move(marr);
      darr.push_back(std::move(dj));
   }
   root["duts"] = std::move(darr);

   json::Value sarr = json::Value::array();
   for (auto const& s : sinks) {
      json::Value sj = json::Value::object();
      sj["type"] = s.type;
      sj["enabled"] = s.enabled;
      if (s.type == "csv" || s.type == "jsonl") {
         sj["dir"] = s.dir;
         sj["flush_every_n"] = s.flush_every_n;
      } else if (s.type == "sqlite3") {
         sj["path"] = s.sqlite_path;
         sj["commit_every_n"] = s.sqlite_commit_every_n;
         sj["commit_every_ms"] = s.sqlite_commit_every_ms;
      } else if (s.type == "psql") {
         sj["conninfo"] = s.psql_conninfo;
         sj["batch_size"] = s.psql_batch_size;
         sj["reconnect_delay_ms"] = s.psql_reconnect_delay_ms;
         sj["enable_timescaledb"] = s.psql_enable_timescaledb;
      } else if (s.type == "prometheus") {
         sj["host"] = s.prometheus_host;
         sj["port"] = s.prometheus_port;
         sj["metric_prefix"] = s.prometheus_prefix;
      }
      sarr.push_back(std::move(sj));
   }
   root["sinks"] = std::move(sarr);

   json::Value gobj = json::Value::object();
   gobj["frontend"] = gui.frontend;
   gobj["host"] = gui.host;
   gobj["port"] = gui.port;
   gobj["theme"] = gui.theme;
   gobj["window_width"] = gui.window_width;
   gobj["window_height"] = gui.window_height;
   gobj["refresh_interval_ms"] = gui.refresh_interval_ms;
   gobj["open_browser"] = gui.open_browser;
   gobj["cli_executable"] = gui.cli_executable;
   gobj["max_log_lines"] = gui.max_log_lines;
   root["gui"] = std::move(gobj);

   return root.dump(indent);
}

LoggerConfig LoggerConfig::from_json(std::string const& json_text) {
   return parse_config(json_text);
}

LoggerConfig LoggerConfig::from_file(std::string const& file_path) {
   return parse_config_file(file_path);
}

std::string ValidationReport::to_json(int indent) const {
   json::Value root = json::Value::object();
   root["valid"] = valid;

   json::Value errs = json::Value::array();
   for (auto const& e : errors) {
      errs.push_back(e);
   }
   root["errors"] = std::move(errs);

   json::Value warns = json::Value::array();
   for (auto const& w : warnings) {
      warns.push_back(w);
   }
   root["warnings"] = std::move(warns);

   json::Value summary = json::Value::object();
   summary["enabled_duts"] = enabled_duts;
   summary["total_metrics"] = total_metrics;
   summary["enabled_sinks"] = enabled_sinks;
   root["summary"] = std::move(summary);

   return root.dump(indent);
}

ValidationReport validate_config(std::string const& json_text) {
   ValidationReport report;
   try {
      json::Value root = json::Value::parse(json_text);
      parse_from_value(root, report);
   } catch (std::exception const& e) {
      report.valid = false;
      report.errors.push_back(e.what());
   }
   return report;
}

ValidationReport validate_config_file(std::string const& file_path) {
   std::ifstream ifs(file_path, std::ios::in | std::ios::binary);
   if (!ifs) {
      ValidationReport report;
      report.valid = false;
      report.errors.push_back("Cannot open configuration file: " + file_path);
      return report;
   }
   std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
   return validate_config(content);
}

LoggerConfig parse_config(std::string const& json_text) {
   json::Value root = json::Value::parse(json_text);
   ValidationReport report;
   LoggerConfig cfg = parse_from_value(root, report);
   if (!report.valid) {
      std::ostringstream oss;
      oss << "Configuration validation failed: ";
      for (size_t i = 0; i < report.errors.size(); ++i) {
         if (i > 0) {
            oss << "; ";
         }
         oss << report.errors[i];
      }
      throw ConfigError(oss.str());
   }
   return cfg;
}

LoggerConfig parse_config_file(std::string const& file_path) {
   json::Value root = json::Value::parse_file(file_path);
   ValidationReport report;
   LoggerConfig cfg = parse_from_value(root, report);
   if (!report.valid) {
      std::ostringstream oss;
      oss << "Configuration validation failed (" << file_path << "): ";
      for (size_t i = 0; i < report.errors.size(); ++i) {
         if (i > 0) {
            oss << "; ";
         }
         oss << report.errors[i];
      }
      throw ConfigError(oss.str());
   }
   return cfg;
}

std::string generate_template_config_json(std::string const& template_name, int indent) {
   LoggerConfig cfg;
   if (template_name == "minimal") {
      DutConfig dut;
      dut.name = "lab_router_1";
      dut.dut_ip = "127.0.0.1";
      dut.snmp.mode = "stub";
      dut.snmp.version = "2c";
      dut.snmp.community = "public";
      dut.metrics = {
          {"sysUpTime", ".1.3.6.1.2.1.1.3.0", "get", 10, "s", ""},
          {"sysName", ".1.3.6.1.2.1.1.5.0", "get", 10, "s", ""},
          {"cpuLoad1", ".1.3.6.1.4.1.2021.10.1.3.1", "get", 10, "s", ""},
      };
      cfg.duts.push_back(std::move(dut));

      SinkConfig csv;
      csv.type = "csv";
      csv.dir = "./data/csv";
      cfg.sinks.push_back(std::move(csv));

      SinkConfig sqlite;
      sqlite.type = "sqlite3";
      sqlite.sqlite_path = "./data/logger.sqlite";
      cfg.sinks.push_back(std::move(sqlite));
   } else if (template_name == "demo") {
      cfg.logger.poll_interval_ms = 1000;
      cfg.logger.stats_interval_ms = 1000;

      for (int i = 1; i <= 3; ++i) {
         DutConfig dut;
         dut.name = "spine_sw_" + std::to_string(i);
         dut.dut_ip = "10.10.0." + std::to_string(10 + i);
         dut.snmp.mode = "stub";
         dut.snmp.version = "2c";
         dut.snmp.community = "public";
         dut.metrics = {
             {"sysUpTime", ".1.3.6.1.2.1.1.3.0", "get", 10, "s", ""},
             {"sysName", ".1.3.6.1.2.1.1.5.0", "get", 10, "s", ""},
             {"cpuLoad1", ".1.3.6.1.4.1.2021.10.1.3.1", "get", 10, "s", ""},
             {"ifInOctets1", ".1.3.6.1.2.1.2.2.1.10.1", "get", 10, "s", ""},
             {"ifOutOctets1", ".1.3.6.1.2.1.2.2.1.16.1", "get", 10, "s", ""},
             {"chassisTempC", ".1.3.6.1.4.1.9.9.13.1.3.1.3.1", "get", 10, "s", ""},
         };
         cfg.duts.push_back(std::move(dut));
      }

      SinkConfig csv;
      csv.type = "csv";
      csv.dir = "./data/csv";
      cfg.sinks.push_back(std::move(csv));

      SinkConfig jsonl;
      jsonl.type = "jsonl";
      jsonl.dir = "./data/jsonl";
      cfg.sinks.push_back(std::move(jsonl));

      SinkConfig sqlite;
      sqlite.type = "sqlite3";
      sqlite.sqlite_path = "./data/logger.sqlite";
      cfg.sinks.push_back(std::move(sqlite));

      SinkConfig prom;
      prom.type = "prometheus";
      prom.enabled = false;
      prom.prometheus_host = "127.0.0.1";
      prom.prometheus_port = 9761;
      cfg.sinks.push_back(std::move(prom));
   } else if (template_name == "enterprise") {
      for (int i = 1; i <= 2; ++i) {
         DutConfig dut;
         dut.name = "core_sw_" + std::to_string(i);
         dut.dut_ip = "10.0.0." + std::to_string(9 + i);
         dut.snmp.mode = "ezsnmp";
         dut.snmp.version = "2c";
         dut.snmp.community = "public";
         dut.metrics = {
             {"sysUpTime", "sysUpTime.0", "get", 10, "s", ""},
             {"sysName", "sysName.0", "get", 10, "s", ""},
             {"ifDescr", "ifDescr", "bulkwalk", 20, "s", ""},
         };
         cfg.duts.push_back(std::move(dut));
      }

      SinkConfig csv;
      csv.type = "csv";
      csv.dir = "./data/csv";
      cfg.sinks.push_back(std::move(csv));

      SinkConfig sqlite;
      sqlite.type = "sqlite3";
      sqlite.sqlite_path = "./data/logger.sqlite";
      cfg.sinks.push_back(std::move(sqlite));

      SinkConfig psql;
      psql.type = "psql";
      psql.enabled = false;
      psql.psql_conninfo = "host=127.0.0.1 port=5432 dbname=snmp user=snmp password=snmp";
      cfg.sinks.push_back(std::move(psql));
   } else if (template_name == "snmpv3") {
      DutConfig dut;
      dut.name = "core_fw_v3";
      dut.dut_ip = "10.20.0.1";
      dut.snmp.mode = "ezsnmp";
      dut.snmp.version = "3";
      dut.snmp.security_level = "authPriv";
      dut.snmp.security_username = "netops_monitor";
      dut.snmp.auth_protocol = "SHA-256";
      dut.snmp.auth_passphrase = "replace_with_auth_secret";
      dut.snmp.privacy_protocol = "AES";
      dut.snmp.privacy_passphrase = "replace_with_priv_secret";
      dut.metrics = {
          {"sysUpTime", ".1.3.6.1.2.1.1.3.0", "get", 10, "s", ""},
          {"sysDescr", ".1.3.6.1.2.1.1.1.0", "get", 10, "s", ""},
      };
      cfg.duts.push_back(std::move(dut));

      SinkConfig sqlite;
      sqlite.type = "sqlite3";
      sqlite.sqlite_path = "./data/snmpv3_logger.sqlite";
      cfg.sinks.push_back(std::move(sqlite));
   } else {
      throw ConfigError("Unknown configuration template '" + template_name +
                        "'. Supported: minimal, demo, enterprise, snmpv3");
   }

   return cfg.to_json(indent);
}

std::string config_json_schema(int indent) {
   json::Value schema = json::Value::object();
   schema["$schema"] = "https://json-schema.org/draft/2020-12/schema";
   schema["$id"] = "https://carlkidcrypto.github.io/ezsnmp_data_logger/config.schema.json";
   schema["title"] = "EzSnmp Data Logger Configuration";
   schema["description"] =
       "Unified JSON configuration governing the C++17 data logger engine, ezsnmp SessionBase "
       "SNMP operations, storage sinks, CLI runtime, and cross-platform GUI preferences.";
   schema["type"] = "object";

   json::Value props = json::Value::object();

   json::Value logger_prop = json::Value::object();
   logger_prop["type"] = "object";
   json::Value lp = json::Value::object();
   auto make_int_prop = [](int def_val, int min_val, char const* desc) {
      json::Value p = json::Value::object();
      p["type"] = "integer";
      p["default"] = def_val;
      p["minimum"] = min_val;
      p["description"] = desc;
      return p;
   };
   lp["poll_interval_ms"] = make_int_prop(1000, 10, "Global polling interval in milliseconds.");
   lp["queue_depth"] = make_int_prop(100000, 1, "Bounded in-memory batch queue capacity.");
   lp["worker_threads"] = make_int_prop(4, 1, "Maximum concurrent DUT polling threads.");
   lp["stats_interval_ms"] = make_int_prop(2000, 50, "Telemetry stats emission interval in ms.");
   lp["max_cycles"] = make_int_prop(0, 0, "Stop after N polling cycles (0 = continuous).");
   lp["run_duration_seconds"] =
       make_int_prop(0, 0, "Stop after N seconds (0 = run until interrupted).");
   json::Value ll = json::Value::object();
   ll["type"] = "string";
   ll["default"] = "info";
   json::Value llenum = json::Value::array();
   for (char const* v : {"debug", "info", "warn", "warning", "error"}) {
      llenum.push_back(v);
   }
   ll["enum"] = std::move(llenum);
   lp["log_level"] = std::move(ll);
   logger_prop["properties"] = std::move(lp);
   props["logger"] = std::move(logger_prop);

   json::Value duts_prop = json::Value::object();
   duts_prop["type"] = "array";
   duts_prop["description"] =
       "List of SNMP Devices Under Test (DUTs) queried via ezsnmp SessionBase.";
   props["duts"] = std::move(duts_prop);

   json::Value sinks_prop = json::Value::object();
   sinks_prop["type"] = "array";
   sinks_prop["description"] =
       "List of output sinks (csv, jsonl, sqlite3, psql, prometheus) receiving batches.";
   props["sinks"] = std::move(sinks_prop);

   json::Value gui_prop = json::Value::object();
   gui_prop["type"] = "object";
   gui_prop["description"] =
       "Cross-platform GUI preferences (frontend: auto|tk|web, theme: dark|light, host, port).";
   props["gui"] = std::move(gui_prop);

   schema["properties"] = std::move(props);
   return schema.dump(indent);
}

} // namespace ezsnmp_data_logger
