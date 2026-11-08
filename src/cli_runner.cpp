#include "ezsnmp_data_logger/cli_runner.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

#include "ezsnmp_data_logger/config.h"
#include "ezsnmp_data_logger/data_logger.h"
#include "ezsnmp_data_logger/exceptions.h"
#include "ezsnmp_data_logger/json_utils.h"
#include "ezsnmp_data_logger/version.h"

namespace ezsnmp_data_logger {

namespace {

std::atomic<bool> g_cli_stop_requested{false};

std::string read_file_contents(std::string const& path) {
   std::ifstream in(path, std::ios::in | std::ios::binary);
   if (!in.is_open()) {
      throw ConfigError("Cannot open JSON configuration file: '" + path + "'");
   }
   std::ostringstream ss;
   ss << in.rdbuf();
   return ss.str();
}

void write_file_contents(std::string const& path, std::string const& text) {
   std::ofstream out(path, std::ios::out | std::ios::trunc | std::ios::binary);
   if (!out.is_open()) {
      throw CliError("Cannot open output file for writing: '" + path + "'");
   }
   out << text << "\n";
}

void print_help(std::ostream& out) {
   out << "ezsnmp-data-logger v" << version()
       << " — High-throughput C++17 SNMP Data Logger powered by ezsnmp\n\n"
       << "Usage:\n"
       << "  ezsnmp-data-logger <subcommand> [options]\n\n"
       << "Subcommands:\n"
       << "  run          Poll configured DUTs via ezsnmp and persist samples to JSON-configured "
          "sinks\n"
       << "  validate     Validate a JSON configuration file or string and report diagnostics\n"
       << "  probe        Execute a single live SNMP probe across configured DUTs without storing\n"
       << "  request      Execute an on-demand ezsnmp GET/GETNEXT/WALK/BULKGET/BULKWALK request\n"
       << "  set          Execute an on-demand ezsnmp SET request against a configured DUT\n"
       << "  inspect      Query recent stored telemetry samples from configured local sinks\n"
       << "  init-config  Generate a ready-to-use JSON configuration file from a built-in "
          "template\n"
       << "  schema       Output the JSON Schema (Draft 2020-12) for ezsnmp_data_logger configs\n"
       << "  version      Print version and compiled backend capability metadata\n"
       << "  help         Show this help text\n\n"
       << "Common Options:\n"
       << "  -c, --config <path>         Path to JSON configuration file\n"
       << "      --config-string <json>  Inline JSON configuration string\n"
       << "      --json                  Emit structured JSON output (used by the GUI behind the "
          "scenes)\n"
       << "  -h, --help                  Show help\n\n"
       << "Run Options:\n"
       << "      --once                  Execute a single polling cycle across enabled DUTs and "
          "exit\n"
       << "      --max-cycles <N>        Override logger.max_cycles\n"
       << "      --duration <sec>        Override logger.run_duration_seconds\n\n"
       << "Request / Set / Probe / Inspect Options:\n"
       << "      --dut <name>            Target or filter by DUT name\n"
       << "      --op <operation>        SNMP operation: get|getnext|walk|bulkget|bulkwalk\n"
       << "      --oid <oid>             Target OID (repeatable for request)\n"
       << "      --max-repeaters <N>     Bulk max-repeaters (default: 10)\n"
       << "      --type <char>           SNMP SET type character (s, i, u, t, a, o, x)\n"
       << "      --value <val>           SNMP SET value string\n"
       << "      --record                Also persist request/set result to enabled sinks\n"
       << "      --limit <N>             Maximum rows for inspect (default: 50)\n"
       << "      --metric <name>         Filter inspect results by metric_name\n"
       << "      --template <name>       Template for init-config: minimal|demo|enterprise|snmpv3\n"
       << "  -o, --output <path>         Output file path for init-config\n";
}

std::string load_config_json_arg(std::string const& config_path, std::string const& config_string) {
   if (!config_string.empty()) {
      return config_string;
   }
   if (!config_path.empty()) {
      return read_file_contents(config_path);
   }
   throw CliError("Missing configuration: pass --config <file.json> or --config-string '<json>'");
}

int dispatch_cli(std::vector<std::string> const& args, std::ostream& out, std::ostream& err) {
   if (args.empty()) {
      print_help(out);
      return 0;
   }

   std::string subcmd;
   size_t start_idx = 0;
   if (!args[0].empty() && args[0][0] != '-') {
      subcmd = args[0];
      start_idx = 1;
   } else {
      // Backward compatibility with legacy `ezsnmp_loggerd --config <file>`
      for (auto const& a : args) {
         if (a == "-h" || a == "--help") {
            print_help(out);
            return 0;
         }
         if (a == "-v" || a == "--version") {
            subcmd = "version";
            break;
         }
      }
      if (subcmd.empty()) {
         subcmd = "run";
      }
   }

   if (subcmd == "help" || subcmd == "--help" || subcmd == "-h") {
      print_help(out);
      return 0;
   }

   std::string config_path;
   std::string config_string;
   std::string dut_name;
   std::string metric_name;
   std::string operation = "get";
   std::vector<std::string> oids;
   std::string set_type = "s";
   std::string set_value;
   bool has_set_value = false;
   std::string template_name = "minimal";
   std::string output_path;
   bool json_output = false;
   bool once = false;
   bool record_to_sinks = false;
   int limit = 50;
   int max_repeaters = 10;
   int max_cycles_override = -1;
   int duration_override = -1;

   auto need_val = [&](size_t& idx, std::string const& flag) -> std::string {
      if (idx + 1 >= args.size()) {
         throw CliError("Option '" + flag + "' requires an argument");
      }
      return args[++idx];
   };

   for (size_t i = start_idx; i < args.size(); ++i) {
      std::string const& a = args[i];
      if (a == "-h" || a == "--help") {
         print_help(out);
         return 0;
      } else if (a == "-v" || a == "--version") {
         subcmd = "version";
      } else if (a == "-c" || a == "--config") {
         config_path = need_val(i, a);
      } else if (a == "--config-string") {
         config_string = need_val(i, a);
      } else if (a == "--json") {
         json_output = true;
      } else if (a == "--once") {
         once = true;
      } else if (a == "--record") {
         record_to_sinks = true;
      } else if (a == "--dut") {
         dut_name = need_val(i, a);
      } else if (a == "--metric") {
         metric_name = need_val(i, a);
      } else if (a == "--op" || a == "--operation") {
         operation = need_val(i, a);
      } else if (a == "--oid") {
         oids.push_back(need_val(i, a));
      } else if (a == "--max-repeaters") {
         max_repeaters = std::stoi(need_val(i, a));
      } else if (a == "--type") {
         set_type = need_val(i, a);
      } else if (a == "--value") {
         set_value = need_val(i, a);
         has_set_value = true;
      } else if (a == "--limit") {
         limit = std::stoi(need_val(i, a));
      } else if (a == "--max-cycles") {
         max_cycles_override = std::stoi(need_val(i, a));
      } else if (a == "--duration") {
         duration_override = std::stoi(need_val(i, a));
      } else if (a == "--template") {
         template_name = need_val(i, a);
      } else if (a == "-o" || a == "--output") {
         output_path = need_val(i, a);
      } else {
         throw CliError("Unknown option: '" + a + "'");
      }
   }

   if (subcmd == "version") {
      if (json_output) {
         out << build_info_json(2) << "\n";
      } else {
         out << "ezsnmp-data-logger " << version()
             << " (ezsnmp=" << (has_ezsnmp_support() ? "yes" : "no")
             << ", sqlite3=" << (has_sqlite3_support() ? "yes" : "no")
             << ", psql=" << (has_psql_support() ? "yes" : "no") << ")\n";
      }
      return 0;
   }

   if (subcmd == "schema") {
      out << config_json_schema(2) << "\n";
      return 0;
   }

   if (subcmd == "init-config") {
      std::string rendered = generate_template_config_json(template_name, 2);
      if (!output_path.empty()) {
         write_file_contents(output_path, rendered);
         if (json_output) {
            json::Value resp = json::Value::object();
            resp["ok"] = true;
            resp["template"] = template_name;
            resp["output"] = output_path;
            out << resp.dump(2) << "\n";
         } else {
            out << "Wrote '" << template_name << "' JSON configuration to " << output_path << "\n";
         }
      } else {
         out << rendered << "\n";
      }
      return 0;
   }

   if (subcmd == "validate") {
      std::string raw = load_config_json_arg(config_path, config_string);
      ValidationReport rep = validate_config(raw);
      if (json_output) {
         out << rep.to_json(2) << "\n";
      } else if (rep.valid) {
         out << "Configuration is VALID (" << rep.enabled_duts << " enabled DUTs, "
             << rep.total_metrics << " metrics, " << rep.enabled_sinks << " enabled sinks).\n";
         for (auto const& w : rep.warnings) {
            out << "  Warning: " << w << "\n";
         }
      } else {
         err << "Configuration is INVALID:\n";
         for (auto const& e : rep.errors) {
            err << "  Error: " << e << "\n";
         }
         for (auto const& w : rep.warnings) {
            err << "  Warning: " << w << "\n";
         }
      }
      return rep.valid ? 0 : 2;
   }

   if (subcmd == "probe") {
      std::string raw = load_config_json_arg(config_path, config_string);
      if (json_output) {
         out << DataLogger::probe_json(raw, dut_name, 2) << "\n";
         return 0;
      }
      LoggerConfig cfg = LoggerConfig::from_json(raw);
      auto results = DataLogger::probe(cfg, dut_name);
      bool all_ok = true;
      for (auto const& r : results) {
         out << "[" << (r.reachable ? "OK" : "FAIL") << "] " << r.dut_name << " (" << r.dut_ip
             << ") latency=" << r.latency_ms << " ms, samples=" << r.samples.size() << "\n";
         if (!r.error.empty()) {
            out << "  error: " << r.error << "\n";
         }
         for (auto const& s : r.samples) {
            out << "  - " << s.metric_name << " [" << s.oid << "] (" << s.snmp_type
                << ") = " << s.display_value() << " (status=" << s.status << ")\n";
         }
         if (!r.reachable) {
            all_ok = false;
         }
      }
      return all_ok ? 0 : 1;
   }

   if (subcmd == "request") {
      std::string raw = load_config_json_arg(config_path, config_string);
      if (dut_name.empty()) {
         throw CliError("Subcommand 'request' requires --dut <name>");
      }
      if (oids.empty()) {
         throw CliError("Subcommand 'request' requires at least one --oid <oid>");
      }
      std::string res_json = DataLogger::request_json(raw, dut_name, operation, oids, max_repeaters,
                                                      record_to_sinks, json_output ? 2 : -1);
      if (json_output) {
         out << res_json << "\n";
      } else {
         json::Value parsed = json::Value::parse(res_json);
         out << "SNMP " << operation << " on " << dut_name << " (" << parsed["dut_ip"].as_string()
             << ") in " << parsed["latency_ms"].as_double() << " ms:\n";
         for (auto const& s : parsed["samples"].as_array()) {
            out << "  " << s.value_string("oid", "") << "." << s.value_string("oid_index", "")
                << " [" << s.value_string("snmp_type", "")
                << "] = " << s.value_string("raw_value", "") << "\n";
         }
      }
      return 0;
   }

   if (subcmd == "set") {
      std::string raw = load_config_json_arg(config_path, config_string);
      if (dut_name.empty()) {
         throw CliError("Subcommand 'set' requires --dut <name>");
      }
      if (oids.size() != 1) {
         throw CliError("Subcommand 'set' requires exactly one --oid <oid>");
      }
      if (!has_set_value) {
         throw CliError("Subcommand 'set' requires --value <val>");
      }
      std::string res_json = DataLogger::set_json(raw, dut_name, oids[0], set_type, set_value,
                                                  record_to_sinks, json_output ? 2 : -1);
      if (json_output) {
         out << res_json << "\n";
      } else {
         json::Value parsed = json::Value::parse(res_json);
         out << "SNMP SET on " << dut_name << " (" << oids[0] << " = " << set_value
             << ") completed in " << parsed["latency_ms"].as_double() << " ms.\n";
      }
      return 0;
   }

   if (subcmd == "inspect") {
      std::string raw = load_config_json_arg(config_path, config_string);
      if (json_output) {
         out << DataLogger::inspect_json(raw, limit, dut_name, metric_name, 2) << "\n";
      } else {
         json::Value parsed =
             json::Value::parse(DataLogger::inspect_json(raw, limit, dut_name, metric_name, -1));
         auto const& rows = parsed["rows"].as_array();
         out << "Inspected " << rows.size() << " stored sample(s):\n";
         for (auto const& r : rows) {
            out << "  ts=" << r["ts_unix_ns"].as_int64() << " dut=" << r["dut_name"].as_string()
                << " metric=" << r["metric_name"].as_string() << " oid=" << r["oid"].as_string()
                << " type=" << r["type_name"].as_string() << " raw=" << r["raw_value"].as_string()
                << " sink=" << r["sink_type"].as_string() << "\n";
         }
      }
      return 0;
   }

   if (subcmd == "run") {
      std::string raw = load_config_json_arg(config_path, config_string);
      LoggerConfig cfg = LoggerConfig::from_json(raw);
      if (max_cycles_override >= 0) {
         cfg.logger.max_cycles = max_cycles_override;
      }
      if (duration_override >= 0) {
         cfg.logger.run_duration_seconds = duration_override;
      }

      DataLogger engine(cfg);

      if (once) {
         auto batches = engine.run_once();
         if (json_output) {
            json::Value ev = json::Value::object();
            ev["ok"] = true;
            ev["mode"] = "once";
            ev["stats"] = json::Value::parse(engine.stats_json(-1));
            json::Value barr = json::Value::array();
            for (auto const& b : batches) {
               barr.push_back(json::Value::parse(b.to_json(-1)));
            }
            ev["batches"] = std::move(barr);
            out << ev.dump(2) << "\n";
         } else {
            LoggerStats st = engine.stats();
            out << "Completed 1 cycle: batches=" << st.processed_batches
                << " samples=" << st.processed_samples << " poll_errors=" << st.poll_errors << "\n";
         }
         return 0;
      }

      g_cli_stop_requested.store(false);
      std::mutex out_mutex;

      if (json_output) {
         engine.set_batch_callback([&out, &out_mutex](Batch const& b) {
            json::Value ev = json::Value::object();
            ev["event"] = "batch";
            ev["batch"] = json::Value::parse(b.to_json(-1));
            std::lock_guard<std::mutex> lock(out_mutex);
            out << ev.dump(-1) << "\n";
            out.flush();
         });
      }

      engine.start();
      {
         std::lock_guard<std::mutex> lock(out_mutex);
         if (json_output) {
            json::Value ev = json::Value::object();
            ev["event"] = "started";
            ev["version"] = version();
            ev["stats"] = json::Value::parse(engine.stats_json(-1));
            out << ev.dump(-1) << "\n";
            out.flush();
         } else {
            out << "ezsnmp-data-logger started (" << cfg.duts.size() << " DUTs, "
                << cfg.sinks.size() << " sinks).\n";
            out.flush();
         }
      }

      auto start_tp = std::chrono::steady_clock::now();
      auto last_stats_tp = start_tp;
      int stats_ms = std::max(200, cfg.logger.stats_interval_ms);

      while (engine.is_running() && !g_cli_stop_requested.load()) {
         auto now = std::chrono::steady_clock::now();
         if (cfg.logger.run_duration_seconds > 0 &&
             std::chrono::duration_cast<std::chrono::seconds>(now - start_tp).count() >=
                 cfg.logger.run_duration_seconds) {
            break;
         }
         if (cfg.logger.max_cycles > 0 &&
             engine.stats().cycles_completed >= cfg.logger.max_cycles) {
            break;
         }
         if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_stats_tp).count() >=
             stats_ms) {
            LoggerStats st = engine.stats();
            std::lock_guard<std::mutex> lock(out_mutex);
            if (json_output) {
               json::Value ev = json::Value::object();
               ev["event"] = "stats";
               ev["stats"] = json::Value::parse(st.to_json(-1));
               out << ev.dump(-1) << "\n";
               out.flush();
            } else if (cfg.logger.log_level != "error") {
               out << "[stats] cycles=" << st.cycles_completed
                   << " batches=" << st.processed_batches << " samples=" << st.processed_samples
                   << " queue=" << st.queue_depth << " errors=" << st.poll_errors << "\n";
               out.flush();
            }
            last_stats_tp = now;
         }
         std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }

      engine.stop();
      {
         LoggerStats st = engine.stats();
         std::lock_guard<std::mutex> lock(out_mutex);
         if (json_output) {
            json::Value ev = json::Value::object();
            ev["event"] = "stopped";
            ev["stats"] = json::Value::parse(st.to_json(-1));
            out << ev.dump(-1) << "\n";
            out.flush();
         } else {
            out << "ezsnmp-data-logger stopped cleanly (batches=" << st.processed_batches
                << ", samples=" << st.processed_samples << ").\n";
            out.flush();
         }
      }
      return 0;
   }

   throw CliError("Unknown subcommand: '" + subcmd + "' (run --help for usage)");
}

} // namespace

std::string CliResult::to_json(int indent) const {
   json::Value j = json::Value::object();
   j["exit_code"] = exit_code;
   j["stdout_text"] = stdout_text;
   j["stderr_text"] = stderr_text;
   return j.dump(indent);
}

int CliRunner::run(std::vector<std::string> const& args) {
   try {
      return dispatch_cli(args, std::cout, std::cerr);
   } catch (std::exception const& e) {
      bool wants_json = false;
      for (auto const& a : args) {
         if (a == "--json") {
            wants_json = true;
            break;
         }
      }
      if (wants_json) {
         json::Value j = json::Value::object();
         j["ok"] = false;
         j["error"] = e.what();
         std::cerr << j.dump(-1) << "\n";
      } else {
         std::cerr << "Error: " << e.what() << "\n";
      }
      return 1;
   }
}

CliResult CliRunner::execute(std::vector<std::string> const& args) {
   std::ostringstream out_ss;
   std::ostringstream err_ss;
   CliResult res;
   try {
      res.exit_code = dispatch_cli(args, out_ss, err_ss);
   } catch (std::exception const& e) {
      bool wants_json = false;
      for (auto const& a : args) {
         if (a == "--json") {
            wants_json = true;
            break;
         }
      }
      if (wants_json) {
         json::Value j = json::Value::object();
         j["ok"] = false;
         j["error"] = e.what();
         err_ss << j.dump(-1) << "\n";
      } else {
         err_ss << "Error: " << e.what() << "\n";
      }
      res.exit_code = 1;
   }
   res.stdout_text = out_ss.str();
   res.stderr_text = err_ss.str();
   return res;
}

void CliRunner::request_stop() { g_cli_stop_requested.store(true); }

} // namespace ezsnmp_data_logger
