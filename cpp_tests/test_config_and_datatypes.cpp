#include "ezsnmp_data_logger/config.h"
#include "ezsnmp_data_logger/datatypes.h"
#include "ezsnmp_data_logger/exceptions.h"
#include "ezsnmp_data_logger/version.h"
#include "test_common.h"

using namespace ezsnmp_data_logger;

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

TEST(VersionTest, ExposesCapabilitiesAndSwigLanguages) {
   EXPECT_EQ(version(), "2026.11.6");
   EXPECT_EQ(ezsnmp_target_version(), "2026.11.4a0");
   EXPECT_TRUE(is_valid_calsemver(version()));
   EXPECT_TRUE(is_valid_calsemver(ezsnmp_target_version()));
   EXPECT_TRUE(is_valid_calsemver("v2026.11.6"));
   EXPECT_TRUE(is_valid_calsemver("2026.11.1"));
   EXPECT_TRUE(is_valid_calsemver("2026.11.6"));
   EXPECT_TRUE(is_valid_calsemver("2026.11.6-alpha.0"));
   EXPECT_FALSE(is_valid_calsemver("2026.01.05"));
   EXPECT_FALSE(is_valid_calsemver("0.1.0"));
   EXPECT_EQ(supported_swig_languages().size(), 4u);
   std::string info = build_info_json(2);
   EXPECT_NE(info.find("CalSemVer"), std::string::npos);
   EXPECT_NE(info.find("swig_target_languages"), std::string::npos);
}
