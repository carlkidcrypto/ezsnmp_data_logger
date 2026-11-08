#include <cstring>
#include <thread>

#include "ezsnmp_data_logger/csv_sink.h"
#include "ezsnmp_data_logger/exceptions.h"
#include "ezsnmp_data_logger/jsonl_sink.h"
#include "ezsnmp_data_logger/prometheus_sink.h"
#include "ezsnmp_data_logger/psql_sink.h"
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

TEST(SinksAndQueueTest, ExhaustiveQueueAndSinks) {
   ScopedTempDir tmp("ezsnmp_sink_cov_");

   // BoundedQueue<Batch> instantiation coverage
   BoundedQueue<Batch> q_batch(1);
   EXPECT_TRUE(q_batch.try_push(Batch{}));
   EXPECT_FALSE(q_batch.try_push(Batch{}));
   EXPECT_EQ(q_batch.dropped(), 1u);
   q_batch.shutdown();
   EXPECT_FALSE(q_batch.try_push(Batch{}));

   // BoundedQueue methods
   BoundedQueue<std::string> q(2);
   EXPECT_EQ(q.size(), 0u);
   EXPECT_EQ(q.dropped(), 0u);
   EXPECT_TRUE(q.try_push("item1"));
   EXPECT_EQ(q.size(), 1u);
   EXPECT_TRUE(q.try_push("item2"));
   EXPECT_FALSE(q.try_push("item3"));
   EXPECT_EQ(q.dropped(), 1u);

   q.shutdown();
   EXPECT_FALSE(q.try_push("item4"));
   bool shutdown_seen = false;
   auto popped1 = q.pop(shutdown_seen);
   ASSERT_TRUE(popped1.has_value());
   EXPECT_EQ(*popped1, "item1");
   EXPECT_FALSE(shutdown_seen);

   auto popped2 = q.pop_blocking();
   ASSERT_TRUE(popped2.has_value());
   EXPECT_EQ(*popped2, "item2");

   auto popped3 = q.pop(shutdown_seen);
   EXPECT_FALSE(popped3.has_value());
   EXPECT_TRUE(shutdown_seen);

   q.reopen();
   EXPECT_TRUE(q.try_push("item5"));

   // CsvSink with buffering and sanitize_filename
   SinkConfig csv_cfg;
   csv_cfg.type = "csv";
   csv_cfg.dir = (tmp.path() / "csv").string();
   csv_cfg.flush_every_n = 10;
   {
      CsvSink cs(csv_cfg);
      EXPECT_EQ(cs.name(), "csv");
      Batch b;
      b.ts_unix_ns = 1700000000000LL;
      b.dut_name = "router/with:special\\chars";
      b.dut_ip = "192.168.1.1";
      b.samples.push_back(Sample::make_string("msg", "1.1", "quotes \"and\" commas,"));
      cs.write(b);
      cs.flush();
      cs.close();

      // CsvSink create_directories failure
      SinkConfig bad_csv_dir;
      bad_csv_dir.type = "csv";
      bad_csv_dir.dir = "/dev/null/sub";
      EXPECT_THROW({ CsvSink cs_bad(bad_csv_dir); }, SinkError);

      // CsvSink file open failure when file path is a directory
      std::filesystem::create_directories(tmp.path() / "csv" / "dir_clash.csv");
      Batch b_clash;
      b_clash.dut_name = "dir_clash";
      CsvSink cs2(csv_cfg);
      EXPECT_THROW({ cs2.write(b_clash); }, SinkError);
   }

   // JsonlSink with buffering and sanitize_filename
   SinkConfig jsonl_cfg;
   jsonl_cfg.type = "jsonl";
   jsonl_cfg.dir = (tmp.path() / "jsonl").string();
   jsonl_cfg.flush_every_n = 10;
   {
      JsonlSink js(jsonl_cfg);
      EXPECT_EQ(js.name(), "jsonl");
      Batch b;
      b.ts_unix_ns = 1700000000001LL;
      b.dut_name = "switch/test:1";
      b.dut_ip = "192.168.1.2";
      b.samples.push_back(Sample::make_int("val", "1.2", 999));
      js.write(b);
      // Repeated write exercises cache hit
      js.write(b);
      js.flush();
      js.close();

      // JsonlSink create_directories failure
      SinkConfig bad_jsonl_dir;
      bad_jsonl_dir.type = "jsonl";
      bad_jsonl_dir.dir = "/dev/null/sub";
      EXPECT_THROW({ JsonlSink js_bad(bad_jsonl_dir); }, SinkError);

      // JsonlSink file open failure when path is a directory
      std::filesystem::create_directories(tmp.path() / "jsonl" / "dir_clash.jsonl");
      Batch b_jclash;
      b_jclash.dut_name = "dir_clash";
      JsonlSink js2(jsonl_cfg);
      EXPECT_THROW({ js2.write(b_jclash); }, SinkError);
   }

   // SqliteSink with :memory: and batch buffering
   SinkConfig sq_mem;
   sq_mem.type = "sqlite3";
   sq_mem.sqlite_path = ":memory:";
   sq_mem.sqlite_commit_every_n = 10;
   {
      SqliteSink ss(sq_mem);
      EXPECT_EQ(ss.name(), "sqlite3");
      Batch b;
      b.ts_unix_ns = 1700000000002LL;
      b.dut_name = "mem-dut";
      b.dut_ip = "127.0.0.1";
      b.samples.push_back(Sample::make_int("count", "1.1", 100));
      b.samples.push_back(Sample::make_float("temp", "1.2", 23.5));
      b.samples.push_back(Sample::make_string("str", "1.3", "active"));
      Sample s_null;
      s_null.metric_name = "null_val";
      s_null.oid = "1.4";
      s_null.type_tag = TypeTag::Null;
      b.samples.push_back(s_null);
      ss.write(b);
      // Repeated write exercises upsert_series cache hit
      ss.write(b);
      ss.flush();
      ss.close();

      // SqliteSink write after close error
      EXPECT_THROW({ ss.write(b); }, SinkError);

      // SqliteSink directory creation failure
      SinkConfig bad_sq_dir;
      bad_sq_dir.type = "sqlite3";
      bad_sq_dir.sqlite_path = "/dev/null/sub/db.sqlite";
      EXPECT_THROW({ SqliteSink ss_bad(bad_sq_dir); }, SinkError);

      // SqliteSink cannot open file when path is a directory
      std::filesystem::path sq_dir_p = tmp.path() / "is_dir_sqlite";
      std::filesystem::create_directories(sq_dir_p);
      SinkConfig bad_sq_p;
      bad_sq_p.type = "sqlite3";
      bad_sq_p.sqlite_path = sq_dir_p.string();
      EXPECT_THROW({ SqliteSink ss_d(bad_sq_p); }, SinkError);
   }

   // PsqlSink stub and error handling
   SinkConfig psql_cfg;
   psql_cfg.type = "psql";
   psql_cfg.psql_conninfo = "host=localhost dbname=ezsnmp_test_not_running";
#if !defined(EZSNMP_DL_HAVE_LIBPQ) || !EZSNMP_DL_HAVE_LIBPQ
   EXPECT_THROW({ PsqlSink ps(psql_cfg); }, SinkError);
   // Exercise stub implementation
   psql_cfg.psql_conninfo = "stub_for_testing";
   {
      PsqlSink ps_stub(psql_cfg);
      EXPECT_EQ(ps_stub.name(), "psql");
      Batch b_stub;
      ps_stub.write(b_stub);
      ps_stub.flush();
      ps_stub.close();
   }
#endif

   // PrometheusSink endpoints and label escaping
   SinkConfig prom_cfg;
   prom_cfg.type = "prometheus";
   prom_cfg.prometheus_host = "127.0.0.1";
   prom_cfg.prometheus_port = 0;
   prom_cfg.prometheus_prefix = "eth0_ezsnmp";
   {
      PrometheusSink prom(prom_cfg);
      EXPECT_EQ(prom.name(), "prometheus");
      ASSERT_GT(prom.bound_port(), 0);

      Batch b;
      b.ts_unix_ns = 1700000000003LL;
      b.dut_name = "switch\"special\\name\nline";
      b.dut_ip = "10.0.0.1";
      b.samples.push_back(Sample::make_int("ifInOctets", "1.3.6.1.2.1.2.2.1.10.1", 12345678));
      b.samples.push_back(Sample::make_string("descr", "1.3.6.1.2.1.1.1.0", "gigabit"));
      prom.write(b);
      prom.flush();

#ifndef _WIN32
      auto send_req = [&](char const* path) -> std::string {
         int fd = ::socket(AF_INET, SOCK_STREAM, 0);
         if (fd < 0) {
            return "";
         }
         sockaddr_in addr{};
         addr.sin_family = AF_INET;
         addr.sin_port = htons(static_cast<uint16_t>(prom.bound_port()));
         ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
         if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            ::close(fd);
            return "";
         }
         std::string req = std::string("GET ") + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
         ::send(fd, req.data(), req.size(), 0);
         std::string resp;
         char buf[1024];
         ssize_t n = 0;
         while ((n = ::recv(fd, buf, sizeof(buf), 0)) > 0) {
            resp.append(buf, static_cast<size_t>(n));
         }
         ::close(fd);
         return resp;
      };

      std::string resp_health = send_req("/healthz");
      EXPECT_NE(resp_health.find("{\"status\":\"ok\"}"), std::string::npos);

      std::string resp_root = send_req("/");
      EXPECT_NE(resp_root.find("eth0_ezsnmp"), std::string::npos);

      std::string resp_404 = send_req("/not_found");
      EXPECT_NE(resp_404.find("404 Not Found"), std::string::npos);
#endif
      prom.close();
   }

   // PrometheusSink with 0.0.0.0 host and special character metric name sanitization
   SinkConfig prom_zero = prom_cfg;
   prom_zero.prometheus_host = "0.0.0.0";
   prom_zero.prometheus_port = 0;
   {
      PrometheusSink p0(prom_zero);
      Batch b_p0;
      b_p0.dut_name = "d1";
      b_p0.samples.push_back(Sample::make_int("test-dash.dot:colon", "1.1", 100));
      p0.write(b_p0);
      p0.flush();
      p0.close();
   }

   // PrometheusSink bad IPv4 address
   SinkConfig bad_prom_cfg;
   bad_prom_cfg.type = "prometheus";
   bad_prom_cfg.prometheus_host = "999.999.999.999";
   bad_prom_cfg.prometheus_port = 0;
   EXPECT_THROW({ PrometheusSink bp(bad_prom_cfg); }, SinkError);

   // Sink Factory create_sink for all types
   SinkConfig fc_cfg;
   fc_cfg.type = "jsonl";
   fc_cfg.dir = (tmp.path() / "fc_jsonl").string();
   EXPECT_NE(create_sink(fc_cfg), nullptr);

   fc_cfg.type = "sqlite3";
   fc_cfg.sqlite_path = ":memory:";
   EXPECT_NE(create_sink(fc_cfg), nullptr);

   fc_cfg.type = "psql";
   fc_cfg.psql_conninfo = "stub_for_testing";
   EXPECT_NE(create_sink(fc_cfg), nullptr);

   fc_cfg.type = "prometheus";
   fc_cfg.prometheus_port = 0;
   EXPECT_NE(create_sink(fc_cfg), nullptr);

   // Sink Factory unknown type
   SinkConfig bad_sink_type;
   bad_sink_type.type = "unknown_sink_xyz";
   EXPECT_THROW({ create_sink(bad_sink_type); }, SinkError);
}
