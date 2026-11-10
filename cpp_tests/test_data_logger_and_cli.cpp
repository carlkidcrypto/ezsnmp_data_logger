#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "ezsnmp_data_logger/cli_runner.h"
#include "ezsnmp_data_logger/data_logger.h"
#include "ezsnmp_data_logger/exceptions.h"
#include "ezsnmp_data_logger/json_utils.h"
#include "ezsnmp_data_logger/snmp_poller.h"
#include "test_common.h"

using namespace ezsnmp_data_logger;
using ezsnmp_data_logger::testing_support::ScopedTempDir;

namespace {

std::string make_test_config_json(std::filesystem::path const& dir, int max_cycles = 3) {
   LoggerConfig cfg;
   cfg.logger.poll_interval_ms = 25;
   cfg.logger.max_cycles = max_cycles;
   cfg.logger.stats_interval_ms = 250;
   cfg.logger.status_file = (dir / "status.json").string();

   DutConfig d1;
   d1.name = "switch-a";
   d1.dut_ip = "127.0.0.1";
   d1.snmp.mode = "stub";
   d1.metrics = {
       MetricConfig{"sysUpTime", "1.3.6.1.2.1.1.3.0", "get", 10, "s", ""},
       MetricConfig{"sysName", "1.3.6.1.2.1.1.5.0", "get", 10, "s", ""},
   };
   cfg.duts.push_back(d1);

   SinkConfig sq;
   sq.type = "sqlite3";
   sq.sqlite_path = (dir / "logger.sqlite").string();
   sq.sqlite_commit_every_n = 1;
   cfg.sinks.push_back(sq);

   SinkConfig csv;
   csv.type = "csv";
   csv.dir = (dir / "csv").string();
   csv.flush_every_n = 1;
   cfg.sinks.push_back(csv);

   return cfg.to_json(2);
}

} // namespace

TEST(DataLoggerEngineTest, RunsScheduledCyclesAndPushesExternalJsonBatches) {
   ScopedTempDir tmp("ezsnmp_engine_");
   std::string cfg_json = make_test_config_json(tmp.path(), 3);

   DataLogger logger(cfg_json);
   logger.run();

   LoggerStats st = logger.stats();
   EXPECT_FALSE(st.running);
   EXPECT_GE(st.cycles_completed, 3);
   EXPECT_GE(st.processed_batches, 3);
   EXPECT_GE(st.processed_samples, 6);
   EXPECT_TRUE(std::filesystem::exists(tmp.path() / "status.json"));

   // Push external JSON batch directly (e.g. from SWIG target language producer)
   std::string const ext_batch = R"({
      "dut_name": "external-go-producer",
      "dut_ip": "127.0.0.1",
      "samples": [{
         "metric_name": "ifInOctets",
         "oid": "1.3.6.1.2.1.2.2.1.10.1",
         "index": "1",
         "type": "COUNTER",
         "value": "777888",
         "converted_value": 777888
      }]
   })";
   EXPECT_TRUE(logger.push_batch_json(ext_batch));

   json::Value inspected =
       json::Value::parse(DataLogger::inspect_json(cfg_json, 20, "external-go-producer", "", 2));
   EXPECT_TRUE(inspected["ok"].as_bool());
   ASSERT_EQ(inspected["count"].as_int(), 1);
   EXPECT_EQ(inspected["rows"][0]["i64"].as_int64(), 777888);
}

TEST(DataLoggerEngineTest, ExhaustiveRuntimeAndAccessors) {
   ScopedTempDir tmp("ezsnmp_dl_acc_");
   std::string const cfg_str = R"({
      "logger": {"poll_interval_ms": 20, "max_cycles": 1, "worker_threads": 2},
      "duts": [{"name": "d1", "dut_ip": "127.0.0.1", "snmp": {"mode": "stub"}, "metrics": [{"name": "m", "oid": "1.1"}]}],
      "sinks": [{"type": "csv", "dir": ")" +
                               (tmp.path() / "csv").string() + R"("}]
   })";

   DataLogger dl(cfg_str);
   EXPECT_FALSE(dl.is_running());
   EXPECT_NE(dl.config_json().find("poll_interval_ms"), std::string::npos);

   // Copy & move constructors & assignment for DataLogger
   DataLogger dl_copy(dl);
   EXPECT_FALSE(dl_copy.is_running());
   DataLogger dl_move(std::move(dl_copy));
   EXPECT_FALSE(dl_move.is_running());

   DataLogger dl_assign = dl;
   dl_assign = dl;
   DataLogger dl_massign = dl;
   dl_massign = std::move(dl_assign);
   dl.flush();

   // dl.start() when already running
   dl.start();
   dl.start();
   EXPECT_TRUE(dl.is_running());

   // Batch callback
   std::atomic<int> cb_calls{0};
   dl.set_batch_callback([&cb_calls](Batch const&) { cb_calls.fetch_add(1); });

   Batch b;
   b.ts_unix_ns = 1700000000000LL;
   b.dut_name = "d1";
   b.dut_ip = "127.0.0.1";
   b.samples.push_back(Sample::make_int("m", "1.1", 123));
   EXPECT_TRUE(dl.push_batch(b));

   std::this_thread::sleep_for(std::chrono::milliseconds(50));
   dl.stop();
   EXPECT_FALSE(dl.is_running());
   EXPECT_GE(cb_calls.load(), 1);

   // Callback throwing exception in write_batch_to_sinks
   dl.set_batch_callback([](Batch const&) { throw std::runtime_error("test cb throw"); });
   dl.push_batch(b);

   // push_batch_json with empty ts_unix_ns
   EXPECT_TRUE(dl.push_batch_json("{\"dut_name\":\"d1\",\"samples\":[]}"));

   // Run once with error sample status to exercise poll_errors increment
   Batch b_err;
   b_err.dut_name = "d1";
   b_err.samples.push_back(Sample::make_int("err_m", "1.2", 0, -1));
   dl.push_batch(b_err);

   // dl.run() with run_duration_seconds = 1
   std::string dur_cfg = R"({
      "logger": {"poll_interval_ms": 20, "run_duration_seconds": 1},
      "duts": [{"name": "d1", "dut_ip": "127.0.0.1", "snmp": {"mode": "stub"}, "metrics": [{"name": "m", "oid": "1.1"}]}]
   })";
   DataLogger dl_dur(dur_cfg);
   dl_dur.run();
   EXPECT_FALSE(dl_dur.is_running());

   // DataLogger probe with disabled dut and filter
   LoggerConfig p_cfg;
   DutConfig d_disabled;
   d_disabled.name = "d_dis";
   d_disabled.enabled = false;
   p_cfg.duts.push_back(d_disabled);

   DutConfig d_probe_active;
   d_probe_active.name = "d_act";
   d_probe_active.dut_ip = "127.0.0.1";
   d_probe_active.snmp.mode = "stub";
   d_probe_active.metrics.push_back(MetricConfig{"m1", "1.1", "get", 10, "s", ""});
   p_cfg.duts.push_back(d_probe_active);

   // Probe with unreachable DUT
   DutConfig d_unreach;
   d_unreach.name = "d_unreach";
   d_unreach.dut_ip = "127.0.0.1";
   d_unreach.snmp.mode = "ezsnmp";
   d_unreach.snmp.port_number = 59998;
   d_unreach.snmp.timeout_seconds = 1;
   d_unreach.snmp.retries = 0;
   d_unreach.metrics.push_back(MetricConfig{"m1", "1.1", "get", 10, "s", ""});
   p_cfg.duts.push_back(d_unreach);

   auto res_filt = DataLogger::probe(p_cfg, "non_existent");
   EXPECT_TRUE(res_filt.empty());
   auto res_all = DataLogger::probe(p_cfg, "d_act");
   EXPECT_EQ(res_all.size(), 1u);
   auto res_unreach = DataLogger::probe(p_cfg, "d_unreach");
   ASSERT_EQ(res_unreach.size(), 1u);
   EXPECT_FALSE(res_unreach[0].reachable);

   // DataLogger from_file
   std::string dl_cfg_f = (tmp.path() / "dl_from_f.json").string();
   {
      std::ofstream out(dl_cfg_f);
      out << cfg_str;
   }
   DataLogger dl_f = DataLogger::from_file(dl_cfg_f);

   // DataLogger(bad_cfg_obj)
   LoggerConfig invalid_cfg_obj;
   invalid_cfg_obj.duts.push_back(DutConfig{});
   EXPECT_THROW({ DataLogger dl_bad(invalid_cfg_obj); }, ConfigError);

   // Status file write failure
   LoggerConfig status_bad_cfg = LoggerConfig::from_json(cfg_str);
   status_bad_cfg.logger.status_file = "/dev/null/sub/status.json";
   DataLogger dl_status_bad(status_bad_cfg);
   dl_status_bad.run_once();

   // Push batch with queue full
   LoggerConfig queue_full_cfg = LoggerConfig::from_json(cfg_str);
   queue_full_cfg.logger.queue_depth = 1;
   DataLogger dl_q_full(queue_full_cfg);
   EXPECT_TRUE(dl_q_full.push_batch(b));
   EXPECT_FALSE(dl_q_full.push_batch(b));
}

TEST(DataLoggerEngineTest, StubPollerExhaustive) {
   DutConfig dut;
   dut.name = "core-switch";
   dut.dut_ip = "127.0.0.1";
   dut.snmp.mode = "stub";
   dut.metrics = {MetricConfig{"sysDescr", "1.1", "get", 10, "s", ""},
                  MetricConfig{"sysLocation", "1.2", "get", 10, "s", ""},
                  MetricConfig{"cpuUtil", "1.3", "get", 10, "s", ""},
                  MetricConfig{"switchTemp", "1.4", "get", 10, "s", ""},
                  MetricConfig{"sysUpTime", "1.5", "get", 10, "s", ""},
                  MetricConfig{"packetCount", "1.6", "get", 10, "s", ""},
                  MetricConfig{"setMetricInt", "1.7", "set", 10, "i", "9999"},
                  MetricConfig{"setMetricStr", "1.8", "set", 10, "s", "new_val"}};

   auto poller = create_snmp_poller(dut);
   Batch b = poller->poll();
   ASSERT_EQ(b.samples.size(), 8u);

   Batch b_walk = poller->request("walk", {"1.3.6.1.2.1.2.2.1"}, 3);
   EXPECT_EQ(b_walk.samples.size(), 3u);

   Batch b_set_i = poller->set_oid("1.7", "i", "42");
   ASSERT_EQ(b_set_i.samples.size(), 1u);
   EXPECT_EQ(b_set_i.samples[0].i64, 42);

   Batch b_set_s = poller->set_oid("1.8", "s", "updated_str");
   ASSERT_EQ(b_set_s.samples.size(), 1u);
   EXPECT_EQ(b_set_s.samples[0].str_val, "updated_str");
}

TEST(CliRunnerTest, ExecutesAllSubcommandsWithJsonContract) {
   ScopedTempDir tmp("ezsnmp_cli_");
   std::string cfg_path = (tmp.path() / "config.json").string();

   // 1. init-config
   auto r_init =
       CliRunner::execute({"init-config", "--template", "demo", "--output", cfg_path, "--json"});
   EXPECT_EQ(r_init.exit_code, 0);
   EXPECT_TRUE(std::filesystem::exists(cfg_path));

   // Write custom test config pointing sinks inside tmp
   {
      std::ofstream out(cfg_path);
      out << make_test_config_json(tmp.path(), 2);
   }

   // 2. validate --json
   auto r_val = CliRunner::execute({"validate", "--config", cfg_path, "--json"});
   EXPECT_EQ(r_val.exit_code, 0);
   EXPECT_TRUE(json::Value::parse(r_val.stdout_text)["valid"].as_bool());

   // 3. probe --json
   auto r_probe = CliRunner::execute({"probe", "--config", cfg_path, "--json"});
   EXPECT_EQ(r_probe.exit_code, 0);
   EXPECT_EQ(json::Value::parse(r_probe.stdout_text)["count"].as_int(), 1);

   // 4. request --json --record
   auto r_req = CliRunner::execute({"request", "--config", cfg_path, "--dut", "switch-a", "--op",
                                    "get", "--oid", "1.3.6.1.2.1.1.3.0", "--record", "--json"});
   EXPECT_EQ(r_req.exit_code, 0) << r_req.stderr_text;
   EXPECT_TRUE(json::Value::parse(r_req.stdout_text)["recorded_to_sinks"].as_bool());

   // 5. set --json --record
   auto r_set = CliRunner::execute({"set", "--config", cfg_path, "--dut", "switch-a", "--oid",
                                    "1.3.6.1.2.1.1.5.0", "--type", "s", "--value",
                                    "gui-configured-switch", "--record", "--json"});
   EXPECT_EQ(r_set.exit_code, 0) << r_set.stderr_text;
   EXPECT_TRUE(json::Value::parse(r_set.stdout_text)["ok"].as_bool());

   // 6. run --once --json
   auto r_once = CliRunner::execute({"run", "--config", cfg_path, "--once", "--json"});
   EXPECT_EQ(r_once.exit_code, 0) << r_once.stderr_text;
   EXPECT_EQ(json::Value::parse(r_once.stdout_text)["mode"].as_string(), "once");

   // 7. inspect --json
   auto r_insp = CliRunner::execute({"inspect", "--config", cfg_path, "--limit", "50", "--json"});
   EXPECT_EQ(r_insp.exit_code, 0) << r_insp.stderr_text;
   EXPECT_GE(json::Value::parse(r_insp.stdout_text)["count"].as_int(), 4);

   // 8. version --json & schema
   auto r_ver = CliRunner::execute({"version", "--json"});
   EXPECT_EQ(r_ver.exit_code, 0);
   EXPECT_FALSE(json::Value::parse(r_ver.stdout_text)["version"].as_string().empty());

   auto r_schema = CliRunner::execute({"schema"});
   EXPECT_EQ(r_schema.exit_code, 0);
   EXPECT_TRUE(json::Value::parse(r_schema.stdout_text).is_object());
}

TEST(CliRunnerTest, ExhaustiveCliRunnerOptionsAndExecution) {
   ScopedTempDir tmp("ezsnmp_cli_full_");
   std::string cfg_path = (tmp.path() / "test_cfg.json").string();

   // Write a full valid config
   std::string const cfg_content = R"({
      "logger": {
         "poll_interval_ms": 20,
         "max_cycles": 1,
         "run_duration_seconds": 1,
         "status_file": ")" + (tmp.path() / "status.json").string() +
                                   R"("
      },
      "duts": [{
         "name": "dut-a",
         "dut_ip": "127.0.0.1",
         "snmp": {"mode": "stub"},
         "metrics": [
            {"name": "sysUpTime", "oid": "1.3.6.1.2.1.1.3.0", "operation": "get"},
            {"name": "sysName", "oid": "1.3.6.1.2.1.1.5.0", "operation": "set", "set_type": "s", "set_value": "test-box"},
            {"name": "ifTable", "oid": "1.3.6.1.2.1.2.2.1.2", "operation": "walk"}
         ]
      }],
      "sinks": [
         {"type": "csv", "dir": ")" +
                                   (tmp.path() / "csv").string() + R"("},
         {"type": "sqlite3", "sqlite_path": ")" +
                                   (tmp.path() / "db.sqlite").string() + R"("}
      ]
   })";

   {
      std::ofstream out(cfg_path);
      out << cfg_content;
   }

   // 1. Invalid config throws ConfigError in DataLogger constructor
   EXPECT_THROW({ DataLogger dl("{\"duts\": \"invalid\"}"); }, ConfigError);

   // 2. DataLogger probe, request, set, inspect static helpers
   std::string probe_res = DataLogger::probe_json(cfg_content, "dut-a", 2);
   EXPECT_NE(probe_res.find("\"count\""), std::string::npos);

   std::string req_res =
       DataLogger::request_json(cfg_content, "dut-a", "walk", {"1.3.6.1.2.1.2.2.1.2"}, 5, true, -1);
   EXPECT_NE(req_res.find("\"samples\""), std::string::npos);

   std::string set_res =
       DataLogger::set_json(cfg_content, "dut-a", "1.3.6.1.2.1.1.5.0", "s", "new-name", true, -1);
   EXPECT_NE(set_res.find("\"ok\":true"), std::string::npos);

   // Unknown DUT throws
   EXPECT_THROW(
       { DataLogger::request_json(cfg_content, "non_existent_dut", "get", {"1.1"}, 1, false, -1); },
       ConfigError);

   // 3. CliRunner subcommands in human-readable / non-json modes
   EXPECT_EQ(CliRunner::run({}), 0);
   EXPECT_EQ(CliRunner::run({"help"}), 0);
   EXPECT_EQ(CliRunner::run({"-h"}), 0);
   EXPECT_EQ(CliRunner::run({"--help"}), 0);
   EXPECT_EQ(CliRunner::run({"version"}), 0);
   EXPECT_EQ(CliRunner::run({"schema"}), 0);

   // init-config human-readable and stdout
   EXPECT_EQ(CliRunner::run({"init-config", "--template", "minimal"}), 0);
   std::string out_init = (tmp.path() / "init_out.json").string();
   EXPECT_EQ(CliRunner::run({"init-config", "--template", "minimal", "-o", out_init}), 0);

   // validate human readable
   EXPECT_EQ(CliRunner::run({"validate", "--config", cfg_path}), 0);
   std::string bad_cfg_path = (tmp.path() / "bad_cfg.json").string();
   {
      std::ofstream out(bad_cfg_path);
      out << "{\"duts\": \"bad\"}";
   }
   EXPECT_EQ(CliRunner::run({"validate", "--config", bad_cfg_path}), 2);

   // probe human readable
   EXPECT_EQ(CliRunner::run({"probe", "--config", cfg_path}), 0);

   // request human readable
   EXPECT_EQ(CliRunner::run({"request", "--config", cfg_path, "--dut", "dut-a", "--op", "get",
                             "--oid", "1.3.6.1.2.1.1.3.0"}),
             0);

   // set human readable
   EXPECT_EQ(CliRunner::run({"set", "--config", cfg_path, "--dut", "dut-a", "--oid",
                             "1.3.6.1.2.1.1.5.0", "--value", "val", "--type", "s"}),
             0);

   // inspect human readable
   EXPECT_EQ(CliRunner::run({"inspect", "--config", cfg_path, "--limit", "10"}), 0);

   // run --once human readable
   EXPECT_EQ(CliRunner::run({"run", "--config", cfg_path, "--once"}), 0);

   // run daemon mode with request_stop
   std::thread stop_thread([]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      CliRunner::request_stop();
   });
   EXPECT_EQ(CliRunner::run({"run", "--config", cfg_path, "--max-cycles", "100"}), 0);
   if (stop_thread.joinable()) {
      stop_thread.join();
   }

   // run with duration override
   EXPECT_EQ(CliRunner::run({"run", "--config-string", cfg_content, "--duration", "1"}), 0);

   // Additional CLI options: -v, --version, --config without subcommand, subcmd --help, request
   // --metric
   EXPECT_EQ(CliRunner::run({"-v"}), 0);
   EXPECT_EQ(CliRunner::run({"--version"}), 0);
   EXPECT_EQ(CliRunner::run({"--config", cfg_path, "--once"}), 0);
   EXPECT_EQ(CliRunner::run({"run", "-h"}), 0);
   EXPECT_EQ(CliRunner::run({"request", "--config", cfg_path, "--dut", "dut-a", "--oid", "1.1",
                             "--metric", "m1", "--max-repeaters", "5"}),
             0);

   // init-config bad path
   auto r_init_bad = CliRunner::execute({"init-config", "-o", "/dev/null/sub/out.json"});
   EXPECT_EQ(r_init_bad.exit_code, 1);

   // set without --oid
   auto r_set_bad =
       CliRunner::execute({"set", "--config", cfg_path, "--dut", "dut-a", "--value", "val"});
   EXPECT_EQ(r_set_bad.exit_code, 1);

   // validate on valid config that has warnings (no sinks configured)
   std::string warn_cfg = (tmp.path() / "warn_cfg.json").string();
   {
      std::ofstream out(warn_cfg);
      out << "{\"duts\": [{\"name\":\"d_w\", \"dut_ip\":\"127.0.0.1\", "
             "\"metrics\":[{\"name\":\"m\",\"oid\":\"1.1\"}]}]}";
   }
   EXPECT_EQ(CliRunner::run({"validate", "--config", warn_cfg}), 0);

   // Probe with unreachable DUT via CLI
   std::string unreach_cli_cfg = R"({
      "duts": [{
         "name": "d_unreach_cli",
         "dut_ip": "127.0.0.1",
         "snmp": {"mode": "ezsnmp", "port_number": 59998, "timeout_seconds": 1, "retries": 0},
         "metrics": [{"name": "m1", "oid": "1.1"}]
      }]
   })";
   EXPECT_EQ(CliRunner::run({"probe", "--config-string", unreach_cli_cfg}), 1);

   // CLI option validation errors
   auto r_err1 = CliRunner::execute({"request", "--config", cfg_path}); // missing --dut
   EXPECT_EQ(r_err1.exit_code, 1);

   auto r_err2 =
       CliRunner::execute({"request", "--config", cfg_path, "--dut", "dut-a"}); // missing --oid
   EXPECT_EQ(r_err2.exit_code, 1);

   auto r_err3 = CliRunner::execute({"set", "--config", cfg_path}); // missing --dut
   EXPECT_EQ(r_err3.exit_code, 1);

   auto r_err4 = CliRunner::execute(
       {"set", "--config", cfg_path, "--dut", "dut-a", "--oid", "1.1"}); // missing --value
   EXPECT_EQ(r_err4.exit_code, 1);

   auto r_err5 = CliRunner::execute({"run", "--unknown-flag"});
   EXPECT_EQ(r_err5.exit_code, 1);

   auto r_err6 = CliRunner::execute({"run", "--config"}); // missing arg to --config
   EXPECT_EQ(r_err6.exit_code, 1);

   auto r_err7 = CliRunner::execute({"run"}); // missing config
   EXPECT_EQ(r_err7.exit_code, 1);
}

TEST(CliRunnerTest, CliRunnerErrorsAndJsonEvents) {
   ScopedTempDir tmp("ezsnmp_cli_events_");
   std::string const cfg_json = R"({
      "logger": {"poll_interval_ms": 20, "max_cycles": 1, "stats_interval_ms": 50},
      "duts": [{"name": "d1", "dut_ip": "127.0.0.1", "snmp": {"mode": "stub"}, "metrics": [{"name": "m", "oid": "1.1"}]}],
      "sinks": [{"type": "csv", "dir": ")" +
                                (tmp.path() / "csv").string() + R"("}]
   })";

   // 1. run command with --json and periodic stats
   auto r_run_json =
       CliRunner::execute({"run", "--config-string", cfg_json, "--json", "--max-cycles", "1"});
   EXPECT_EQ(r_run_json.exit_code, 0);
   EXPECT_NE(r_run_json.stdout_text.find("\"event\":\"started\""), std::string::npos);
   EXPECT_NE(r_run_json.stdout_text.find("\"event\":\"stopped\""), std::string::npos);

   // 2. Unknown subcommand
   auto r_unknown_sub = CliRunner::execute({"bogus_subcommand"});
   EXPECT_EQ(r_unknown_sub.exit_code, 1);
   EXPECT_NE(r_unknown_sub.stderr_text.find("Unknown subcommand"), std::string::npos);

   // 3. Error with --json in CliRunner::run and CliRunner::execute
   int ret_err_json = CliRunner::run({"validate", "--config", "/non/existent.json", "--json"});
   EXPECT_EQ(ret_err_json, 1);

   // 4. Non-json error in CliRunner::run
   int ret_nonjson = CliRunner::run({"validate", "--config", "/non/existent.json"});
   EXPECT_EQ(ret_nonjson, 1);

   auto res_err_json = CliRunner::execute({"validate", "--config", "/non/existent.json", "--json"});
   EXPECT_EQ(res_err_json.exit_code, 1);
   EXPECT_NE(res_err_json.stderr_text.find("\"ok\":false"), std::string::npos);

   // 5. CliResult to_json
   std::string cr_compact = res_err_json.to_json(-1);
   EXPECT_NE(cr_compact.find("\"exit_code\":1"), std::string::npos);
   std::string cr_indented = res_err_json.to_json(2);
   EXPECT_NE(cr_indented.find("\n"), std::string::npos);

   // 6. Human readable stats output during run
   std::string human_stats_cfg = R"({
      "logger": {"poll_interval_ms": 20, "max_cycles": 2, "stats_interval_ms": 50},
      "duts": [{"name": "d1", "dut_ip": "127.0.0.1", "snmp": {"mode": "stub"}, "metrics": [{"name": "m", "oid": "1.1"}]}]
   })";
   auto r_human_run = CliRunner::execute({"run", "--config-string", human_stats_cfg});
   EXPECT_EQ(r_human_run.exit_code, 0);
}
