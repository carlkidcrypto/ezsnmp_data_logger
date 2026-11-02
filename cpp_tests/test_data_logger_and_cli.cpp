#include <filesystem>
#include <fstream>

#include "ezsnmp_data_logger/cli_runner.h"
#include "ezsnmp_data_logger/data_logger.h"
#include "ezsnmp_data_logger/json_utils.h"
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
      "dut_ip": "10.9.9.9",
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
