#include <filesystem>
#include <fstream>

#include "ezsnmp_data_logger/config.h"
#include "ezsnmp_data_logger/sink.h"
#include "ezsnmp_data_logger/sqlite_sink.h"
#include "test_common.h"

using namespace ezsnmp_data_logger;
using ezsnmp_data_logger::testing_support::ScopedTempDir;

TEST(SinkInspectorTest, HandlesNonexistentDirectoriesAndMalformedRecords) {
   ScopedTempDir tmp("ezsnmp_insp_cov_");

   // Test inspection on non-existent directories
   LoggerConfig empty_cfg;
   SinkConfig bad_csv;
   bad_csv.type = "csv";
   bad_csv.dir = (tmp.path() / "non_existent_csv").string();
   SinkConfig bad_jsonl;
   bad_jsonl.type = "jsonl";
   bad_jsonl.dir = (tmp.path() / "non_existent_jsonl").string();
   SinkConfig bad_sqlite_mem;
   bad_sqlite_mem.type = "sqlite3";
   bad_sqlite_mem.sqlite_path = ":memory:";
   empty_cfg.sinks = {bad_csv, bad_jsonl, bad_sqlite_mem};

   auto r_empty = inspect_configured_sinks(empty_cfg, 10, "", "");
   EXPECT_TRUE(r_empty.empty());

   // Create CSV with 10-column legacy row and 13-column modern row
   std::filesystem::create_directories(tmp.path() / "csv");
   std::filesystem::path csv_file = tmp.path() / "csv" / "router.csv";
   {
      std::ofstream out(csv_file);
      out << "ts_unix_ns,dut_name,dut_ip,metric_name,oid,oid_index,snmp_type,raw_value,type_tag,"
             "i64,f64,str_val,status\n";
      out << "1700000000100,r1,127.0.0.1,sysName,1.1,0,OCTETSTR,my-router,2,,,my-router,0\n";
      out << "1700000000200,r2,127.0.0.2,cpuLoad,1.2,1,5.5,5.5,1,,0\n"; // short 10-col format
      out << "invalid_row\n";
      out << "\n"; // empty line
   }
   std::ofstream(tmp.path() / "csv" / "ignore.txt") << "not a csv";

   LoggerConfig csv_lcfg;
   SinkConfig ccfg;
   ccfg.type = "csv";
   ccfg.dir = (tmp.path() / "csv").string();
   csv_lcfg.sinks = {ccfg};

   auto r_filtered = inspect_configured_sinks(csv_lcfg, 10, "r1", "sysName");
   ASSERT_EQ(r_filtered.size(), 1u);
   EXPECT_EQ(r_filtered[0].dut_name, "r1");
   EXPECT_EQ(r_filtered[0].str_val, "my-router");

   // Truncation limit test
   auto r_trunc = inspect_configured_sinks(csv_lcfg, 1, "", "");
   EXPECT_EQ(r_trunc.size(), 1u);

   // Create JSONL with corrupted line
   std::filesystem::create_directories(tmp.path() / "jsonl");
   std::filesystem::path jsonl_file = tmp.path() / "jsonl" / "sw1.jsonl";
   {
      std::ofstream out(jsonl_file);
      out << "{\"ts_unix_ns\":1700000000300,\"dut_name\":\"sw1\",\"dut_ip\":\"127.0.0.1\",\"metric_"
             "name\":\"temp\",\"oid\":\"1.2\",\"type_tag\":1,\"i64\":55,\"status\":0}\n";
      out << "corrupted_json_line\n";
      out << "[1, 2, 3]\n";
      out << "\n"; // empty line
   }
   std::ofstream(tmp.path() / "jsonl" / "ignore.txt") << "not a jsonl";

   LoggerConfig jsonl_lcfg;
   SinkConfig jcfg;
   jcfg.type = "jsonl";
   jcfg.dir = (tmp.path() / "jsonl").string();
   jsonl_lcfg.sinks = {jcfg};

   auto r_jsonl = inspect_configured_sinks(jsonl_lcfg, 10, "sw1", "temp");
   ASSERT_EQ(r_jsonl.size(), 1u);
   EXPECT_EQ(r_jsonl[0].dut_name, "sw1");
   EXPECT_EQ(r_jsonl[0].i64, 55);

   // Query with mismatch dut filter
   EXPECT_TRUE(inspect_configured_sinks(jsonl_lcfg, 10, "mismatch_dut", "").empty());

   // SQLite sink inspection with metric filter and float column
   SinkConfig sq_cfg;
   sq_cfg.type = "sqlite3";
   sq_cfg.sqlite_path = (tmp.path() / "test.sqlite").string();
   {
      SqliteSink sq(sq_cfg);
      Batch b;
      b.ts_unix_ns = 1700000000400LL;
      b.dut_name = "sw1";
      b.dut_ip = "127.0.0.1";
      b.samples.push_back(Sample::make_int("fanRpm", "1.3.6.1.4.1", 3200));
      b.samples.push_back(Sample::make_float("voltage", "1.3.6.1.4.2", 12.5));
      sq.write(b);
      sq.close();
   }

   LoggerConfig sq_lcfg;
   sq_lcfg.sinks = {sq_cfg};
   auto r_sq = inspect_configured_sinks(sq_lcfg, 10, "sw1", "fanRpm");
   ASSERT_EQ(r_sq.size(), 1u);
   EXPECT_EQ(r_sq[0].i64, 3200);

   auto r_sq_f = inspect_configured_sinks(sq_lcfg, 10, "sw1", "voltage");
   ASSERT_EQ(r_sq_f.size(), 1u);
   EXPECT_DOUBLE_EQ(r_sq_f[0].f64, 12.5);
}
