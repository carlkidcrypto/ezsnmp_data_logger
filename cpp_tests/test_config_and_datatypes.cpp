#include <fstream>

#include "ezsnmp_data_logger/config.h"
#include "ezsnmp_data_logger/datatypes.h"
#include "ezsnmp_data_logger/exceptions.h"
#include "ezsnmp_data_logger/version.h"
#include "test_common.h"

using namespace ezsnmp_data_logger;
using ezsnmp_data_logger::testing_support::ScopedTempDir;

TEST(DatatypesTest, SampleSupportsTypedConstructorsAndEzSnmpResultJson) {
   Sample si = Sample::make_int("ifInOctets", "1.3.6.1.2.1.2.2.1.10.1", 9876543210LL);
   EXPECT_EQ(si.type_tag, TypeTag::Int64);
   EXPECT_EQ(si.display_value(), "9876543210");

   Sample sf = Sample::make_float("cpuTemp", "1.3.6.1.4.1.9999.1.0", 48.25);
   EXPECT_EQ(sf.type_tag, TypeTag::Float64);

   Sample ss = Sample::make_string("sysName", "1.3.6.1.2.1.1.5.0", "core-sw-01");
   EXPECT_EQ(ss.type_tag, TypeTag::Str);
   EXPECT_EQ(ss.display_value(), "core-sw-01");

   // Ingest ezsnmp Result.to_dict() JSON format directly
   std::string const ezsnmp_dict_json = R"({
      "oid": "iso.3.6.1.2.1.1.3",
      "index": "0",
      "type": "TIMETICKS",
      "value": "123456",
      "converted_value": 123456
   })";
   Sample from_ez = Sample::from_json(ezsnmp_dict_json);
   EXPECT_EQ(from_ez.oid, "iso.3.6.1.2.1.1.3");
   EXPECT_EQ(from_ez.oid_index, "0");
   EXPECT_EQ(from_ez.snmp_type, "TIMETICKS");
   EXPECT_EQ(from_ez.raw_value, "123456");
   EXPECT_EQ(from_ez.type_tag, TypeTag::Int64);
   EXPECT_EQ(from_ez.i64, 123456);

   Batch b;
   b.ts_unix_ns = 1700000000000000000LL;
   b.dut_name = "dut-1";
   b.dut_ip = "10.0.0.1";
   b.samples = {si, sf, ss, from_ez};

   Batch roundtrip = Batch::from_json(b.to_json(2));
   ASSERT_EQ(roundtrip.samples.size(), 4u);
   EXPECT_EQ(roundtrip.dut_name, "dut-1");
   EXPECT_EQ(roundtrip.samples[3].oid_index, "0");
   EXPECT_EQ(roundtrip.samples[3].snmp_type, "TIMETICKS");
}

TEST(DatatypesTest, DatatypesExhaustive) {
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

TEST(ConfigTest, TemplatesValidateAndRoundTripCleanly) {
   for (char const* tpl : {"minimal", "demo", "enterprise", "snmpv3"}) {
      std::string json_text = generate_template_config_json(tpl, 2);
      ValidationReport rep = validate_config(json_text);
      EXPECT_TRUE(rep.valid) << "Template '" << tpl << "' failed validation";
      EXPECT_GE(rep.enabled_duts, 1);

      LoggerConfig cfg = LoggerConfig::from_json(json_text);
      ValidationReport rep2 = validate_config(cfg.to_json(2));
      EXPECT_TRUE(rep2.valid);
   }

   std::string schema = config_json_schema(2);
   EXPECT_NE(schema.find("https://json-schema.org/draft/2020-12/schema"), std::string::npos);
   EXPECT_NE(schema.find("frontend"), std::string::npos);

   std::string const gui_locale_json = R"({
      "gui": {"frontend": "tk", "locale": "es", "locale_file": "/custom/locale.json"},
      "sinks": [{"type": "csv", "dir": "./data"}]
   })";
   LoggerConfig cfg_loc = LoggerConfig::from_json(gui_locale_json);
   EXPECT_EQ(cfg_loc.gui.locale, "es");
   EXPECT_EQ(cfg_loc.gui.locale_file, "/custom/locale.json");
   std::string const dumped = cfg_loc.to_json(2);
   EXPECT_NE(dumped.find("\"locale\": \"es\""), std::string::npos);
}

TEST(ConfigTest, RejectsInvalidSnmpAndSinkConfigurations) {
   // SNMPv1 + bulkwalk is invalid
   std::string const bad_v1_bulk = R"({
      "duts": [{
         "name": "sw1",
         "dut_ip": "127.0.0.1",
         "snmp": {"mode": "ezsnmp", "version": "1", "community": "public"},
         "metrics": [{"name": "ifDescr", "oid": "1.3.6.1.2.1.2.2.1.2", "operation": "bulkwalk"}]
      }],
      "sinks": [{"type": "csv", "dir": "./data"}]
   })";
   ValidationReport r1 = validate_config(bad_v1_bulk);
   EXPECT_FALSE(r1.valid);
   EXPECT_FALSE(r1.errors.empty());

   // SNMPv3 authPriv missing passphrases
   std::string const bad_v3 = R"({
      "duts": [{
         "name": "sw3",
         "dut_ip": "127.0.0.1",
         "snmp": {"mode": "ezsnmp", "version": "3", "security_level": "authPriv"},
         "metrics": [{"name": "sysName", "oid": "1.3.6.1.2.1.1.5.0"}]
      }],
      "sinks": [{"type": "csv", "dir": "./data"}]
   })";
   ValidationReport r2 = validate_config(bad_v3);
   EXPECT_FALSE(r2.valid);
   EXPECT_GE(r2.errors.size(), 3u);

   // Invalid GUI frontend
   std::string const bad_gui = R"({
      "gui": {"frontend": "qt-nonexistent"},
      "sinks": [{"type": "csv", "dir": "./data"}]
   })";
   EXPECT_FALSE(validate_config(bad_gui).valid);
}

TEST(ConfigTest, ConfigValidationExhaustive) {
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

   // parse_config_file with valid JSON file that fails validation
   std::string bad_val_f = (tmp_c.path() / "bad_val.json").string();
   {
      std::ofstream out(bad_val_f);
      out << "{\"duts\": \"invalid_must_be_array\"}";
   }
   EXPECT_THROW({ parse_config_file(bad_val_f); }, ConfigError);
}

TEST(VersionTest, ExposesCapabilitiesAndSwigLanguages) {
   EXPECT_EQ(version(), "2026.11.7");
   EXPECT_EQ(ezsnmp_target_version(), "2026.11.4a0");
   EXPECT_TRUE(is_valid_calsemver(version()));
   EXPECT_TRUE(is_valid_calsemver(ezsnmp_target_version()));
   EXPECT_TRUE(is_valid_calsemver("v2026.11.7"));
   EXPECT_TRUE(is_valid_calsemver("2026.11.1"));
   EXPECT_TRUE(is_valid_calsemver("2026.11.7"));
   EXPECT_TRUE(is_valid_calsemver("2026.11.7-alpha.0"));
   EXPECT_FALSE(is_valid_calsemver("2026.01.05"));
   EXPECT_FALSE(is_valid_calsemver("0.1.0"));
   EXPECT_EQ(supported_swig_languages().size(), 4u);
   std::string info = build_info_json(2);
   EXPECT_NE(info.find("CalSemVer"), std::string::npos);
   EXPECT_NE(info.find("swig_target_languages"), std::string::npos);
}

TEST(VersionTest, VersionAndCapabilitiesExhaustive) {
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
