#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

#include "ezsnmp_data_logger/cli_runner.h"
#include "ezsnmp_data_logger/config.h"
#include "ezsnmp_data_logger/csv_sink.h"
#include "ezsnmp_data_logger/data_logger.h"
#include "ezsnmp_data_logger/datatypes.h"
#include "ezsnmp_data_logger/exceptions.h"
#include "ezsnmp_data_logger/json_utils.h"
#include "ezsnmp_data_logger/jsonl_sink.h"
#include "ezsnmp_data_logger/prometheus_sink.h"
#include "ezsnmp_data_logger/psql_sink.h"
#include "ezsnmp_data_logger/queue.h"
#include "ezsnmp_data_logger/sink.h"
#include "ezsnmp_data_logger/snmp_poller.h"
#include "ezsnmp_data_logger/sqlite_sink.h"
#include "ezsnmp_data_logger/version.h"
#include "test_common.h"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace ezsnmp_data_logger;
using ezsnmp_data_logger::testing_support::ScopedTempDir;

TEST(FullCoverageTest, VersionAndCapabilitiesExhaustive) {
   EXPECT_FALSE(version().empty());
   EXPECT_FALSE(ezsnmp_target_version().empty());
   EXPECT_TRUE(is_valid_calsemver("2026.11.7"));
   EXPECT_TRUE(is_valid_calsemver("v2026.11.7"));
   EXPECT_TRUE(is_valid_calsemver("2026.11.7a0"));
   EXPECT_TRUE(is_valid_calsemver("2026.11.7-alpha.1"));
   EXPECT_FALSE(is_valid_calsemver(""));
   EXPECT_FALSE(is_valid_calsemver("invalid"));
   EXPECT_FALSE(is_valid_calsemver("2026.01.01"));
   EXPECT_FALSE(is_valid_calsemver("2026.13.1"));
   EXPECT_FALSE(is_valid_calsemver("1.2.3"));

   EXPECT_EQ(has_netsnmp_support(), has_ezsnmp_support());
   (void)has_sqlite3_support();
   (void)has_embedded_sqlite3();
   (void)has_psql_support();

   auto langs = supported_swig_languages();
   ASSERT_EQ(langs.size(), 4u);
   EXPECT_EQ(langs[0], "python");
   EXPECT_EQ(langs[1], "javascript");
   EXPECT_EQ(langs[2], "csharp");
   EXPECT_EQ(langs[3], "go");

   std::string info_compact = build_info_json(-1);
   EXPECT_NE(info_compact.find("ezsnmp_data_logger"), std::string::npos);
   std::string info_indented = build_info_json(2);
   EXPECT_NE(info_indented.find("\n"), std::string::npos);
}

TEST(FullCoverageTest, DatatypesExhaustive) {
   EXPECT_EQ(type_tag_to_string(TypeTag::Null), "null");
   EXPECT_EQ(type_tag_to_string(static_cast<TypeTag>(99)), "null");

   // Sample display_value
   Sample sn;
   sn.type_tag = TypeTag::Null;
   EXPECT_EQ(sn.display_value(), "null");

   Sample sf = Sample::make_float("load", "1.1", 3.14159);
   EXPECT_FALSE(sf.display_value().empty());

   Sample ss = Sample::make_string("name", "1.2", "val");
   EXPECT_EQ(ss.display_value(), "val");

   // Compact array Sample parsing
   std::string const compact_json = "[\"m1\", \"1.3.6.1\", 1, 42, 0.0, \"\", 0]";
   Sample from_arr = Sample::from_json(compact_json);
   EXPECT_EQ(from_arr.metric_name, "m1");
   EXPECT_EQ(from_arr.type_tag, TypeTag::Int64);
   EXPECT_EQ(from_arr.i64, 42);

   // Bad compact array (< 7 elements)
   EXPECT_THROW({ Sample::from_json("[\"m1\", \"1.3.6.1\"]"); }, ConfigError);

   // Bad sample JSON (number instead of object/array)
   EXPECT_THROW({ Sample::from_json("12345"); }, ConfigError);

   // TypeTag string variants in JSON
   std::string const tt_null = "{\"metric_name\":\"m\",\"oid\":\"1.1\",\"type_tag\":\"null\"}";
   EXPECT_EQ(Sample::from_json(tt_null).type_tag, TypeTag::Null);

   std::string const tt_int =
       "{\"metric_name\":\"m\",\"oid\":\"1.1\",\"type_tag\":\"int\",\"i64\":10}";
   EXPECT_EQ(Sample::from_json(tt_int).type_tag, TypeTag::Int64);

   std::string const tt_int64 =
       "{\"metric_name\":\"m\",\"oid\":\"1.1\",\"type_tag\":\"int64\",\"i64\":20}";
   EXPECT_EQ(Sample::from_json(tt_int64).type_tag, TypeTag::Int64);

   std::string const tt_float =
       "{\"metric_name\":\"m\",\"oid\":\"1.1\",\"type_tag\":\"float\",\"f64\":1.5}";
   EXPECT_EQ(Sample::from_json(tt_float).type_tag, TypeTag::Float64);

   std::string const tt_float64 =
       "{\"metric_name\":\"m\",\"oid\":\"1.1\",\"type_tag\":\"float64\",\"f64\":2.5}";
   EXPECT_EQ(Sample::from_json(tt_float64).type_tag, TypeTag::Float64);

   std::string const tt_str =
       "{\"metric_name\":\"m\",\"oid\":\"1.1\",\"type_tag\":\"string\",\"str_val\":\"hi\"}";
   EXPECT_EQ(Sample::from_json(tt_str).type_tag, TypeTag::Str);

   std::string const tt_unknown = "{\"metric_name\":\"m\",\"oid\":\"1.1\",\"type_tag\":\"bogus\"}";
   EXPECT_THROW({ Sample::from_json(tt_unknown); }, ConfigError);

   std::string const tt_bad_num = "{\"metric_name\":\"m\",\"oid\":\"1.1\",\"type_tag\":999}";
   EXPECT_THROW({ Sample::from_json(tt_bad_num); }, ConfigError);

   // converted_value variants
   std::string const cv_bool = "{\"oid\":\"1.1\",\"converted_value\":true}";
   Sample s_bool = Sample::from_json(cv_bool);
   EXPECT_EQ(s_bool.type_tag, TypeTag::Int64);
   EXPECT_EQ(s_bool.i64, 1);

   std::string const cv_bool_f = "{\"oid\":\"1.1\",\"converted_value\":false}";
   Sample s_bool_f = Sample::from_json(cv_bool_f);
   EXPECT_EQ(s_bool_f.i64, 0);

   std::string const cv_double = "{\"oid\":\"1.1\",\"converted_value\":42.5}";
   Sample s_double = Sample::from_json(cv_double);
   EXPECT_EQ(s_double.type_tag, TypeTag::Float64);
   EXPECT_DOUBLE_EQ(s_double.f64, 42.5);

   std::string const cv_str = "{\"oid\":\"1.1\",\"converted_value\":\"hello\"}";
   Sample s_str = Sample::from_json(cv_str);
   EXPECT_EQ(s_str.type_tag, TypeTag::Str);
   EXPECT_EQ(s_str.str_val, "hello");

   // Batch from_json errors
   EXPECT_THROW({ Batch::from_json("[1, 2, 3]"); }, ConfigError);

   // type_tag on boolean or other value in JSON
   Sample s_tt_bool = Sample::from_json("{\"oid\":\"1.1\",\"type_tag\":true}");
   EXPECT_EQ(s_tt_bool.type_tag, TypeTag::Null);

   // Sample::to_json
   Sample s_dump = Sample::make_int("test_metric", "1.1.0", 555);
   EXPECT_NE(s_dump.to_json(-1).find("test_metric"), std::string::npos);

   // MetricConfig and SinkConfig equality and copy
   MetricConfig mc1{"m", "1.1", "get", 10, "s", ""};
   MetricConfig mc2 = mc1;
   EXPECT_TRUE(mc1 == mc2);
   SinkConfig sc1;
   sc1.type = "csv";
   SinkConfig sc2 = sc1;
   EXPECT_EQ(sc2.type, "csv");
}

TEST(FullCoverageTest, JsonUtilsExhaustive) {
   using namespace ezsnmp_data_logger::json;

   Value v_null;
   EXPECT_TRUE(v_null.is_null());
   EXPECT_EQ(v_null.size(), 0u);

   Value v_null2(nullptr);
   EXPECT_TRUE(v_null2.is_null());

   Value v_bool(true);
   EXPECT_TRUE(v_bool.is_bool());
   EXPECT_TRUE(v_bool.as_bool());
   EXPECT_THROW({ v_null.as_bool(); }, ConfigError);

   Value v_int(42);
   EXPECT_TRUE(v_int.is_int());
   EXPECT_EQ(v_int.as_int(), 42);
   EXPECT_EQ(v_int.as_int64(), 42LL);
   EXPECT_DOUBLE_EQ(v_int.as_double(), 42.0);

   Value v_double(3.14);
   EXPECT_TRUE(v_double.is_double());
   EXPECT_DOUBLE_EQ(v_double.as_double(), 3.14);
   EXPECT_EQ(v_double.as_int64(), 3);
   EXPECT_THROW({ v_bool.as_int64(); }, ConfigError);
   EXPECT_THROW({ v_bool.as_double(); }, ConfigError);

   Value v_str("test string");
   EXPECT_TRUE(v_str.is_string());
   EXPECT_EQ(v_str.as_string(), "test string");
   EXPECT_THROW({ v_int.as_string(); }, ConfigError);

   Value v_arr = Value::array();
   EXPECT_TRUE(v_arr.is_array());
   v_arr.push_back(1);
   v_arr.push_back("two");
   EXPECT_EQ(v_arr.size(), 2u);
   EXPECT_EQ(v_arr[0].as_int(), 1);
   EXPECT_EQ(v_arr[1].as_string(), "two");
   EXPECT_THROW({ (void)v_arr[5]; }, std::out_of_range);
   EXPECT_THROW({ (void)v_str[0]; }, ConfigError);
   EXPECT_THROW({ (void)v_str.as_array(); }, ConfigError);
   Value const& c_arr = v_arr;
   EXPECT_EQ(c_arr[0].as_int(), 1);
   EXPECT_THROW({ (void)c_arr[5]; }, std::out_of_range);
   EXPECT_THROW({ (void)v_str.as_array(); }, ConfigError);

   Value v_obj = Value::object();
   EXPECT_TRUE(v_obj.is_object());
   v_obj["key"] = 123;
   EXPECT_EQ(v_obj.size(), 1u);
   EXPECT_EQ(v_obj["key"].as_int(), 123);
   EXPECT_TRUE(v_obj.contains("key"));
   EXPECT_FALSE(v_obj.contains("missing"));
   EXPECT_NE(v_obj.find("key"), nullptr);
   EXPECT_EQ(v_obj.find("missing"), nullptr);
   EXPECT_EQ(v_int.find("key"), nullptr);

   Value const& c_obj = v_obj;
   EXPECT_EQ(c_obj["key"].as_int(), 123);
   EXPECT_THROW({ (void)c_obj["missing"]; }, ConfigError);
   Value const const_int(99);
   EXPECT_THROW({ (void)const_int["key"]; }, ConfigError);
   EXPECT_THROW({ (void)const_int.as_object(); }, ConfigError);

   EXPECT_EQ(c_obj.value_string("missing", "def"), "def");
   EXPECT_EQ(c_obj.value_int("missing", 99), 99);
   EXPECT_EQ(c_obj.value_int64("missing", 88LL), 88LL);
   EXPECT_DOUBLE_EQ(c_obj.value_double("missing", 7.7), 7.7);
   EXPECT_TRUE(c_obj.value_bool("missing", true));

   // Non-const as_array / as_object type assertion errors
   Value v_not_arr(123);
   EXPECT_THROW({ (void)v_not_arr.as_array(); }, ConfigError);
   Value v_not_obj(123);
   EXPECT_THROW({ (void)v_not_obj.as_object(); }, ConfigError);

   // empty() check
   EXPECT_TRUE(v_null.empty());
   EXPECT_FALSE(v_obj.empty());

   // push_back & operator[] mutation from non-array / non-object
   Value v_mutate_arr(42);
   v_mutate_arr.push_back("first");
   EXPECT_EQ(v_mutate_arr.size(), 1u);

   Value v_mutate_obj(42);
   v_mutate_obj["key1"] = 100;
   EXPECT_EQ(v_mutate_obj["key1"].as_int(), 100);

   // value_string with int and bool
   Value v_typed_kv = Value::object();
   v_typed_kv["int_key"] = 456;
   v_typed_kv["bool_key"] = true;
   EXPECT_EQ(v_typed_kv.value_string("int_key", ""), "456");
   EXPECT_EQ(v_typed_kv.value_string("bool_key", "fallback"), "fallback");

   // \b, \f, \r serialization and non-finite double serialization
   Value v_special_chars("esc\b\f\r");
   EXPECT_NE(v_special_chars.dump(-1).find("\\b"), std::string::npos);
   Value v_inf(std::numeric_limits<double>::infinity());
   EXPECT_EQ(v_inf.dump(-1), "null");

   // Parse error conditions
   EXPECT_THROW({ Value::parse(""); }, ConfigError);
   EXPECT_THROW({ Value::parse("   "); }, ConfigError);
   EXPECT_THROW({ Value::parse("{"); }, ConfigError);
   EXPECT_THROW({ Value::parse("["); }, ConfigError);
   EXPECT_THROW({ Value::parse("{\"k\""); }, ConfigError);
   EXPECT_THROW({ Value::parse("{\"k\":}"); }, ConfigError);
   EXPECT_THROW({ Value::parse("[1, 2,]"); }, ConfigError);
   EXPECT_THROW({ Value::parse("{\"k\": 1,}"); }, ConfigError);
   EXPECT_THROW({ Value::parse("\"unterminated"); }, ConfigError);
   EXPECT_THROW({ Value::parse("\"bad escape \\x\""); }, ConfigError);
   EXPECT_THROW({ Value::parse("\"bad unicode \\u123z\""); }, ConfigError);
   EXPECT_THROW({ Value::parse("{\"k\" 1}"); }, ConfigError);
   EXPECT_THROW({ Value::parse("invalid_token"); }, ConfigError);
   EXPECT_THROW({ Value::parse("-"); }, ConfigError);
   EXPECT_THROW({ Value::parse("1.2.3"); }, ConfigError);
   EXPECT_THROW({ Value::parse("1e"); }, ConfigError);
   EXPECT_THROW({ Value::parse("[1 2]"); }, ConfigError);

   // Literal errors and additional parsing branches
   EXPECT_THROW({ Value::parse("trux"); }, ConfigError);
   EXPECT_THROW({ Value::parse("falsy"); }, ConfigError);
   EXPECT_THROW({ Value::parse("nulll"); }, ConfigError);
   EXPECT_NO_THROW({ (void)Value::parse("\"\\u00A9 \\u20AC \\/\""); });
   EXPECT_THROW({ Value::parse("\"\x01\""); }, ConfigError);
   EXPECT_THROW({ Value::parse("\"abc\\"); }, ConfigError);
   EXPECT_THROW({ Value::parse("\"\\uD800\\u0000\""); }, ConfigError);
   EXPECT_THROW({ Value::parse("\"\\uD800abc\""); }, ConfigError);
   EXPECT_THROW({ Value::parse("0123"); }, ConfigError);
   EXPECT_THROW({ Value::parse("-a"); }, ConfigError);
   EXPECT_THROW({ Value::parse("1."); }, ConfigError);
   EXPECT_NO_THROW({ (void)Value::parse("1e+2"); });
   EXPECT_THROW({ Value::parse("1e+"); }, ConfigError);
   EXPECT_NO_THROW({ (void)Value::parse("99999999999999999999999999999999999999"); });
   EXPECT_THROW({ Value::parse("[1, 2"); }, ConfigError);
   EXPECT_THROW({ Value::parse("{\"key\" \"val\"}"); }, ConfigError);
   EXPECT_THROW({ Value::parse("{\"key\": 1"); }, ConfigError);
   EXPECT_THROW({ Value::parse("{\"key\": 1 2}"); }, ConfigError);

   // Value::parse_file
   ScopedTempDir tmp_json("ezsnmp_json_file_");
   std::string json_f = (tmp_json.path() / "test.json").string();
   {
      std::ofstream out(json_f);
      out << "{\"hello\": \"world\"}";
   }
   EXPECT_EQ(Value::parse_file(json_f)["hello"].as_string(), "world");
   EXPECT_THROW({ Value::parse_file("/non/existent/file.json"); }, ConfigError);

   // Parsing valid strings with all escape sequences
   std::string const escaped_json = R"("line1\nline2\t\"quote\"\\slash\b\f\r\u0041")";
   Value parsed_esc = Value::parse(escaped_json);
   EXPECT_NE(parsed_esc.as_string().find("line1\nline2\t\"quote\"\\slash"), std::string::npos);

   // Dump variants
   Value dump_obj = Value::object();
   dump_obj["null_v"] = nullptr;
   dump_obj["bool_v"] = false;
   dump_obj["int_v"] = -500;
   dump_obj["dbl_v"] = -12.345;
   dump_obj["str_v"] = "quote\"and\nnewline\tand\x01ctrl";
   dump_obj["arr_v"] = Value::array();
   dump_obj["arr_v"].push_back(1);
   dump_obj["nested_obj"] = Value::object();

   std::string compact_d = dump_obj.dump(-1);
   EXPECT_EQ(compact_d.find("\n"), std::string::npos);

   std::string indented_d = dump_obj.dump(2);
   EXPECT_NE(indented_d.find("\n"), std::string::npos);

   // Value::parse roundtrip
   Value rt = Value::parse(compact_d);
   EXPECT_EQ(rt["int_v"].as_int(), -500);
   EXPECT_FALSE(rt["bool_v"].as_bool());
}

TEST(FullCoverageTest, ConfigValidationExhaustive) {
   // Top-level not an object
   EXPECT_FALSE(validate_config("[]").valid);
   EXPECT_FALSE(validate_config("\"str\"").valid);

   // Bad logger section
   EXPECT_FALSE(validate_config("{\"logger\": 123}").valid);

   // Invalid logger numbers and log_level
   std::string const bad_logger = R"({
      "logger": {
         "poll_interval_ms": 5,
         "queue_depth": 0,
         "worker_threads": 0,
         "stats_interval_ms": 10,
         "log_level": "verbose"
      }
   })";
   ValidationReport r_l = validate_config(bad_logger);
   EXPECT_FALSE(r_l.valid);
   EXPECT_GE(r_l.errors.size(), 4u);

   // Bad duts structure
   EXPECT_FALSE(validate_config("{\"duts\": \"not_an_array\"}").valid);
   EXPECT_FALSE(validate_config("{\"duts\": [123]}").valid);

   // DUT empty name, duplicate name, empty dut_ip, poll_interval_ms < 10
   std::string const bad_duts = R"({
      "duts": [
         {"name": "", "dut_ip": ""},
         {"name": "sw1", "dut_ip": "10.0.0.1", "poll_interval_ms": 5},
         {"name": "sw1", "dut_ip": "10.0.0.2"}
      ]
   })";
   ValidationReport r_d = validate_config(bad_duts);
   EXPECT_FALSE(r_d.valid);
   EXPECT_GE(r_d.errors.size(), 3u);

   // Bad snmp section
   std::string const bad_snmp = R"({
      "duts": [{
         "name": "d1",
         "dut_ip": "10.0.0.1",
         "snmp": "not_an_object"
      }]
   })";
   EXPECT_FALSE(validate_config(bad_snmp).valid);

   // Invalid SNMP mode, version, port, timeout, retries
   std::string const bad_snmp_vals = R"({
      "duts": [{
         "name": "d1",
         "dut_ip": "10.0.0.1",
         "snmp": {
            "mode": "bogus_mode",
            "version": "99",
            "port_number": -1,
            "timeout_seconds": 0,
            "retries": -5
         }
      }]
   })";
   EXPECT_FALSE(validate_config(bad_snmp_vals).valid);

   // SNMP version 2 normalizes to 2c, netsnmp normalizes to ezsnmp
   std::string const norm_snmp = R"({
      "duts": [{
         "name": "d1",
         "dut_ip": "10.0.0.1",
         "snmp": {"mode": "netsnmp", "version": "2", "community": "pub"},
         "metrics": [{"name": "m1", "oid": "1.1"}]
      }],
      "sinks": [{"type": "csv", "dir": "./csv"}]
   })";
   ValidationReport r_norm = validate_config(norm_snmp);
   EXPECT_TRUE(r_norm.valid);

   // SNMPv3 bad security_level
   std::string const bad_v3_sec = R"({
      "duts": [{
         "name": "d1",
         "dut_ip": "10.0.0.1",
         "snmp": {"mode": "ezsnmp", "version": "3", "security_level": "super_secure"}
      }]
   })";
   EXPECT_FALSE(validate_config(bad_v3_sec).valid);

   // SNMPv3 unsupported auth and privacy protocols
   std::string const bad_v3_protocols = R"({
      "duts": [{
         "name": "d_v3_proto",
         "dut_ip": "10.0.0.1",
         "snmp": {
            "version": "3",
            "security_username": "snmpuser",
            "security_level": "authPriv",
            "auth_protocol": "INVALID_AUTH",
            "auth_passphrase": "password123",
            "privacy_protocol": "INVALID_PRIV",
            "privacy_passphrase": "password123"
         }
      }]
   })";
   EXPECT_FALSE(validate_config(bad_v3_protocols).valid);

   // SNMPv1/v2c empty community
   std::string const bad_v2c_comm = R"({
      "duts": [{
         "name": "d_comm",
         "dut_ip": "10.0.0.1",
         "snmp": {"version": "2c", "community": ""}
      }]
   })";
   EXPECT_FALSE(validate_config(bad_v2c_comm).valid);

   // Bad metrics structure
   std::string const bad_metrics = R"({
      "duts": [{
         "name": "d1",
         "dut_ip": "10.0.0.1",
         "metrics": "not_array"
      }]
   })";
   EXPECT_FALSE(validate_config(bad_metrics).valid);

   std::string const bad_metric_items = R"({
      "duts": [{
         "name": "d1",
         "dut_ip": "10.0.0.1",
         "metrics": [
            "not_object",
            {"name": "", "oid": ""},
            {"name": "m1", "oid": "1.1", "operation": "bad_op", "rate_type": "bad_rate", "set_type": "z"},
            {"name": "m1", "oid": "1.2"}
         ]
      }]
   })";
   ValidationReport r_m = validate_config(bad_metric_items);
   EXPECT_FALSE(r_m.valid);
   EXPECT_GE(r_m.errors.size(), 5u);

   // Sinks validation: not array, not object, empty type, unknown type, missing fields, duplicate
   std::string const bad_sinks = R"({
      "sinks": [
         "not_obj",
         {"type": ""},
         {"type": "bogus_sink"},
         {"type": "csv", "dir": ""},
         {"type": "jsonl", "dir": ""},
         {"type": "sqlite3", "sqlite_path": ""},
         {"type": "psql", "psql_conninfo": ""},
         {"type": "prometheus", "prometheus_port": 999999},
         {"type": "csv", "dir": "./csv2"}
      ]
   })";
   ValidationReport r_s = validate_config(bad_sinks);
   EXPECT_FALSE(r_s.valid);
   EXPECT_GE(r_s.errors.size(), 6u);

   // GUI validation: not an object, unknown frontend, unknown locale
   std::string const bad_gui = R"({
      "gui": "not_obj"
   })";
   EXPECT_FALSE(validate_config(bad_gui).valid);

   std::string const bad_gui_vals = R"({
      "gui": {
         "frontend": "qt",
         "theme": "blue",
         "port": 999999,
         "window_width": 100,
         "window_height": 100,
         "refresh_interval_ms": 10,
         "max_log_lines": 2
      }
   })";
   ValidationReport r_g = validate_config(bad_gui_vals);
   EXPECT_FALSE(r_g.valid);
   EXPECT_GE(r_g.errors.size(), 5u);

   // Template config generator error
   EXPECT_THROW({ generate_template_config_json("non_existent_template"); }, ConfigError);

   // validate_config_file with missing file
   ValidationReport r_missing = validate_config_file("/non/existent/config.json");
   EXPECT_FALSE(r_missing.valid);
   EXPECT_FALSE(r_missing.errors.empty());

   // Metric set without set_type and max_repeaters < 1
   std::string const bad_metric_ops = R"({
      "duts": [{
         "name": "d_ops",
         "dut_ip": "10.0.0.1",
         "metrics": [
            {"name": "m_set", "oid": "1.1", "operation": "set", "set_type": ""},
            {"name": "m_rep", "oid": "1.2", "max_repeaters": 0}
         ]
      }]
   })";
   EXPECT_FALSE(validate_config(bad_metric_ops).valid);

   // Sinks structure not array, CSV flush_every_n < 1, JSONL flush_every_n < 1, SQLite bad params,
   // PSQL batch_size < 1, Prometheus port < 0
   EXPECT_FALSE(validate_config("{\"sinks\": \"not_array\"}").valid);
   std::string const bad_sinks_detailed = R"({
      "sinks": [
         {"type": "csv", "dir": "./csv", "flush_every_n": 0},
         {"type": "jsonl", "dir": "./jsonl", "flush_every_n": 0},
         {"type": "sqlite3", "path": "", "commit_every_n": 0, "commit_every_ms": 0},
         {"type": "psql", "conninfo": "host=localhost", "batch_size": 0},
         {"type": "prometheus", "port": -1, "metric_prefix": ""}
      ]
   })";
   EXPECT_FALSE(validate_config(bad_sinks_detailed).valid);

   // Template files & parse_config / parse_config_file
   ScopedTempDir tmp_c("ezsnmp_tmp_conf_");
   std::string cfg_f = (tmp_c.path() / "test_conf.json").string();
   std::string tmpl = generate_template_config_json("minimal", 2);
   {
      std::ofstream out(cfg_f);
      out << tmpl;
   }
   EXPECT_TRUE(validate_config_file(cfg_f).valid);
   EXPECT_NO_THROW({ (void)LoggerConfig::from_file(cfg_f); });
   EXPECT_NO_THROW({ (void)parse_config_file(cfg_f); });
   EXPECT_THROW({ parse_config_file("/non/existent/cfg.json"); }, ConfigError);

   // ValidationReport::to_json serialization
   ValidationReport rep_to_j;
   rep_to_j.valid = false;
   rep_to_j.errors.push_back("test_error");
   rep_to_j.warnings.push_back("test_warning");
   EXPECT_NE(rep_to_j.to_json(-1).find("test_error"), std::string::npos);

   // validate_config on syntax error in JSON
   EXPECT_FALSE(validate_config("{invalid_json").valid);

   // parse_config with multiple errors
   EXPECT_THROW({ parse_config("{\"duts\": [{\"name\":\"\", \"dut_ip\":\"\"}]}"); }, ConfigError);

   // parse_config_file with valid JSON file that fails validation (lines 624-633)
   std::string bad_val_f = (tmp_c.path() / "bad_val.json").string();
   {
      std::ofstream out(bad_val_f);
      out << "{\"duts\": \"invalid_must_be_array\"}";
   }
   EXPECT_THROW({ parse_config_file(bad_val_f); }, ConfigError);
}

TEST(FullCoverageTest, QueueAndSinksExhaustive) {
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

TEST(FullCoverageTest, SinkInspectorExhaustive) {
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
      out << "1700000000100,r1,10.0.0.1,sysName,1.1,0,OCTETSTR,my-router,2,,,my-router,0\n";
      out << "1700000000200,r2,10.0.0.2,cpuLoad,1.2,1,5.5,5.5,1,,0\n"; // short 10-col format
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
      out << "{\"ts_unix_ns\":1700000000300,\"dut_name\":\"sw1\",\"dut_ip\":\"10.1.1.1\",\"metric_"
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
      b.dut_ip = "10.1.1.1";
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

TEST(FullCoverageTest, DataLoggerAndCliExhaustive) {
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
      out << "{\"duts\": [{\"name\":\"d_w\", \"dut_ip\":\"10.0.0.1\", "
             "\"metrics\":[{\"name\":\"m\",\"oid\":\"1.1\"}]}]}";
   }
   EXPECT_EQ(CliRunner::run({"validate", "--config", warn_cfg}), 0);

   // Probe with unreachable DUT via CLI (lines 272-273, 279-280)
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

TEST(FullCoverageTest, StubPollerExhaustive) {
   DutConfig dut;
   dut.name = "core-switch";
   dut.dut_ip = "192.168.1.100";
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

TEST(FullCoverageTest, MainBinaryExecution) {
   // Locate compiled binaries across execution working directories
   std::string bin = "./ezsnmp-data-logger";
   if (!std::filesystem::exists(bin)) {
      bin = "../ezsnmp-data-logger";
   }
   if (!std::filesystem::exists(bin)) {
      bin = "./build-cov/ezsnmp-data-logger";
   }

   std::string alias_bin = "./ezsnmp_loggerd";
   if (!std::filesystem::exists(alias_bin)) {
      alias_bin = "../ezsnmp_loggerd";
   }
   if (!std::filesystem::exists(alias_bin)) {
      alias_bin = "./build-cov/ezsnmp_loggerd";
   }

   int ret_ver = std::system((bin + " version").c_str());
   EXPECT_EQ(ret_ver, 0);

   int ret_help = std::system((bin + " --help").c_str());
   EXPECT_EQ(ret_help, 0);

   int ret_alias = std::system((alias_bin + " --help").c_str());
   EXPECT_EQ(ret_alias, 0);

#ifndef _WIN32
   // Exercise SIGINT signal handler in src/main.cpp
   pid_t pid = fork();
   if (pid == 0) {
      execl(bin.c_str(), bin.c_str(), "run", "--config-string",
            "{\"logger\":{\"stats_interval_ms\":50},\"duts\":[{\"name\":\"d1\",\"dut_ip\":\"127.0."
            "0.1\",\"snmp\":{\"mode\":\"stub\"},\"metrics\":[{\"name\":\"m\",\"oid\":\"1.1\"}]}]}",
            nullptr);
      _exit(0);
   } else if (pid > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      kill(pid, SIGINT);
      int status = 0;
      waitpid(pid, &status, 0);
   }
#endif
}

TEST(FullCoverageTest, DataLoggerRuntimeAndAccessors) {
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
   dl.start(); // hits line 134
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

   // Probe with unreachable DUT (lines 560-562)
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

   // DataLogger(bad_cfg_obj) hitting lines 83-91 of data_logger.cpp
   LoggerConfig invalid_cfg_obj;
   invalid_cfg_obj.duts.push_back(DutConfig{});
   EXPECT_THROW({ DataLogger dl_bad(invalid_cfg_obj); }, ConfigError);

   // Status file write failure (lines 25, 38)
   LoggerConfig status_bad_cfg = LoggerConfig::from_json(cfg_str);
   status_bad_cfg.logger.status_file = "/dev/null/sub/status.json";
   DataLogger dl_status_bad(status_bad_cfg);
   dl_status_bad.run_once();

   // Push batch with queue full (lines 254-255)
   LoggerConfig queue_full_cfg = LoggerConfig::from_json(cfg_str);
   queue_full_cfg.logger.queue_depth = 1;
   DataLogger dl_q_full(queue_full_cfg);
   dl_q_full.start();
   dl_q_full.push_batch(b);
   EXPECT_FALSE(dl_q_full.push_batch(b));
   dl_q_full.stop();
}

TEST(FullCoverageTest, CliRunnerErrorsAndJsonEvents) {
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

   // 6. Human readable stats output during run (lines 431-446)
   std::string human_stats_cfg = R"({
      "logger": {"poll_interval_ms": 20, "max_cycles": 2, "stats_interval_ms": 50},
      "duts": [{"name": "d1", "dut_ip": "127.0.0.1", "snmp": {"mode": "stub"}, "metrics": [{"name": "m", "oid": "1.1"}]}]
   })";
   auto r_human_run = CliRunner::execute({"run", "--config-string", human_stats_cfg});
   EXPECT_EQ(r_human_run.exit_code, 0);
}
