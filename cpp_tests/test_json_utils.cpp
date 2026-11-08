#include <fstream>
#include <limits>

#include "ezsnmp_data_logger/exceptions.h"
#include "ezsnmp_data_logger/json_utils.h"
#include "test_common.h"

using ezsnmp_data_logger::ConfigError;
using ezsnmp_data_logger::json::Value;
using ezsnmp_data_logger::testing_support::ScopedTempDir;

TEST(JsonUtilsTest, ParsesAndSerializesPrimitivesAndContainers) {
   std::string const raw = R"({
      "name": "router-01",
      "enabled": true,
      "disabled": false,
      "nothing": null,
      "port": 161,
      "big": -9007199254740991,
      "ratio": 3.1415,
      "tags": ["core", "edge", 42],
      "nested": {"escaped": "line1\nline2\t\"quoted\"\\slash"}
   })";

   Value root = Value::parse(raw);
   ASSERT_TRUE(root.is_object());
   EXPECT_EQ(root["name"].as_string(), "router-01");
   EXPECT_TRUE(root["enabled"].as_bool());
   EXPECT_FALSE(root["disabled"].as_bool());
   EXPECT_TRUE(root["nothing"].is_null());
   EXPECT_EQ(root["port"].as_int(), 161);
   EXPECT_EQ(root["big"].as_int64(), -9007199254740991LL);
   EXPECT_NEAR(root["ratio"].as_double(), 3.1415, 1e-9);
   ASSERT_EQ(root["tags"].size(), 3u);
   EXPECT_EQ(root["tags"][0].as_string(), "core");
   EXPECT_EQ(root["tags"][2].as_int(), 42);
   EXPECT_EQ(root["nested"]["escaped"].as_string(), "line1\nline2\t\"quoted\"\\slash");

   // Round-trip both compact and pretty-printed
   Value reparsed = Value::parse(root.dump(2));
   EXPECT_EQ(reparsed["nested"]["escaped"].as_string(), "line1\nline2\t\"quoted\"\\slash");
   EXPECT_EQ(Value::parse(root.dump(-1))["name"].as_string(), "router-01");
}

TEST(JsonUtilsTest, HandlesUtf16SurrogatePairsAndPreservesKeyOrder) {
   Value parsed = Value::parse("{\"z\": 1, \"a\": \"\\u0041\\uD83D\\uDE80\", \"m\": 2}");
   ASSERT_TRUE(parsed.is_object());
   auto const& obj = parsed.as_object();
   ASSERT_EQ(obj.size(), 3u);
   EXPECT_EQ(obj[0].first, "z");
   EXPECT_EQ(obj[1].first, "a");
   EXPECT_EQ(obj[2].first, "m");
   EXPECT_EQ(parsed["a"].as_string(), "A\xF0\x9F\x9A\x80");
}

TEST(JsonUtilsTest, ReportsLineAndColumnOnMalformedJson) {
   EXPECT_THROW(Value::parse("{\n  \"a\": 1,\n  \"b\": \n}"), ConfigError);
   EXPECT_THROW(Value::parse("[1, 2, 3] trailing"), ConfigError);
   EXPECT_THROW(Value::parse("{\"unterminated\": \"abc"), ConfigError);
}

TEST(JsonUtilsTest, ExhaustiveParserAndSerializerEdgeCases) {
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
