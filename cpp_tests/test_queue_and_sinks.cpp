#include <cstring>
#include <thread>

#include "ezsnmp_data_logger/csv_sink.h"
#include "ezsnmp_data_logger/jsonl_sink.h"
#include "ezsnmp_data_logger/prometheus_sink.h"
#include "ezsnmp_data_logger/queue.h"
#include "ezsnmp_data_logger/sink.h"
#include "ezsnmp_data_logger/sqlite_sink.h"
#include "test_common.h"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace ezsnmp_data_logger;
using ezsnmp_data_logger::testing_support::ScopedTempDir;

TEST(BoundedQueueTest, EnforcesCapacityAndDrainsBeforeShutdown) {
   BoundedQueue<int> q(3);
   EXPECT_TRUE(q.try_push(10));
   EXPECT_TRUE(q.try_push(20));
   EXPECT_TRUE(q.try_push(30));
   EXPECT_FALSE(q.try_push(40));
   EXPECT_EQ(q.size(), 3u);

   q.shutdown();
   EXPECT_FALSE(q.try_push(50));

   auto v1 = q.pop_blocking();
   auto v2 = q.pop_blocking();
   auto v3 = q.pop_blocking();
   auto v4 = q.pop_blocking();
   ASSERT_TRUE(v1.has_value());
   ASSERT_TRUE(v2.has_value());
   ASSERT_TRUE(v3.has_value());
   EXPECT_FALSE(v4.has_value());
   EXPECT_EQ(*v1 + *v2 + *v3, 60);
}

TEST(SinksAndInspectorTest, WritesAndInspectsSqliteJsonlAndCsvSinks) {
   ScopedTempDir tmp("ezsnmp_sinks_");

   Batch b1;
   b1.ts_unix_ns = 1700000000000000100LL;
   b1.dut_name = "lab-router-1";
   b1.dut_ip = "192.168.10.1";
   Sample s1 = Sample::make_int("sysUpTime", "1.3.6.1.2.1.1.3.0", 456789);
   s1.oid_index = "0";
   s1.snmp_type = "TIMETICKS";
   Sample s2 = Sample::make_string("sysName", "1.3.6.1.2.1.1.5.0", "lab,router\"1\"");
   s2.oid_index = "0";
   b1.samples = {s1, s2};

   // 1. SQLite3 Sink + Inspector
   SinkConfig sqlite_cfg;
   sqlite_cfg.type = "sqlite3";
   sqlite_cfg.sqlite_path = (tmp.path() / "telemetry.sqlite").string();
   sqlite_cfg.sqlite_commit_every_n = 1;
   {
      SqliteSink sq(sqlite_cfg);
      sq.write(b1);
      sq.flush();
      sq.close();
   }

   LoggerConfig lcfg_sq;
   lcfg_sq.sinks = {sqlite_cfg};
   auto sq_rows = inspect_configured_sinks(lcfg_sq, 10, "lab-router-1", "");
   ASSERT_EQ(sq_rows.size(), 2u);
   EXPECT_EQ(sq_rows[0].sink_type, "sqlite3");

   // 2. JSONL Sink + Inspector
   SinkConfig jsonl_cfg;
   jsonl_cfg.type = "jsonl";
   jsonl_cfg.dir = (tmp.path() / "jsonl").string();
   jsonl_cfg.flush_every_n = 1;
   {
      JsonlSink js(jsonl_cfg);
      js.write(b1);
      js.close();
   }

   LoggerConfig lcfg_js;
   lcfg_js.sinks = {jsonl_cfg};
   auto js_rows = inspect_configured_sinks(lcfg_js, 10, "", "sysUpTime");
   ASSERT_EQ(js_rows.size(), 1u);
   EXPECT_EQ(js_rows[0].i64, 456789);
   EXPECT_EQ(js_rows[0].snmp_type, "TIMETICKS");
   EXPECT_EQ(js_rows[0].sink_type, "jsonl");

   // 3. CSV Sink + Inspector (verifies RFC-4180 escaping of commas and quotes)
   SinkConfig csv_cfg;
   csv_cfg.type = "csv";
   csv_cfg.dir = (tmp.path() / "csv").string();
   csv_cfg.flush_every_n = 1;
   {
      CsvSink cs(csv_cfg);
      cs.write(b1);
      cs.close();
   }

   LoggerConfig lcfg_csv;
   lcfg_csv.sinks = {csv_cfg};
   auto csv_rows = inspect_configured_sinks(lcfg_csv, 10, "", "sysName");
   ASSERT_EQ(csv_rows.size(), 1u);
   EXPECT_EQ(csv_rows[0].str_val, "lab,router\"1\"");
   EXPECT_EQ(csv_rows[0].sink_type, "csv");
}

TEST(PrometheusSinkTest, ServesExpositionFormatOverHttp) {
   SinkConfig prom_cfg;
   prom_cfg.type = "prometheus";
   prom_cfg.prometheus_host = "127.0.0.1";
   prom_cfg.prometheus_port = 0;
   prom_cfg.prometheus_prefix = "ezsnmp_test";

   PrometheusSink prom(prom_cfg);
   ASSERT_GT(prom.bound_port(), 0);

   Batch b;
   b.ts_unix_ns = 1700000000000000500LL;
   b.dut_name = "edge-sw";
   b.dut_ip = "10.1.1.1";
   b.samples.push_back(Sample::make_float("opticalRxDbm", "1.3.6.1.4.1.1.1.0", -2.75));
   prom.write(b);

   std::string rendered = prom.render_metrics();
   EXPECT_NE(rendered.find("ezsnmp_test_sample_value"), std::string::npos);
   EXPECT_NE(rendered.find("opticalRxDbm"), std::string::npos);
   EXPECT_NE(rendered.find("-2.75"), std::string::npos);

#ifndef _WIN32
   int fd = ::socket(AF_INET, SOCK_STREAM, 0);
   ASSERT_GE(fd, 0);
   sockaddr_in addr{};
   addr.sin_family = AF_INET;
   addr.sin_port = htons(static_cast<uint16_t>(prom.bound_port()));
   ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
   ASSERT_EQ(::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

   char const* req = "GET /metrics HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
   ASSERT_GT(::send(fd, req, std::strlen(req), 0), 0);

   std::string http_resp;
   char buf[1024];
   ssize_t n = 0;
   while ((n = ::recv(fd, buf, sizeof(buf), 0)) > 0) {
      http_resp.append(buf, static_cast<size_t>(n));
   }
   ::close(fd);

   EXPECT_NE(http_resp.find("HTTP/1.1 200 OK"), std::string::npos);
   EXPECT_NE(http_resp.find("ezsnmp_test_batches_exported_total 1"), std::string::npos);
#endif

   prom.close();
}
