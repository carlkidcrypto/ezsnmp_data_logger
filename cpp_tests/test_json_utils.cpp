#include "ezsnmp_data_logger/exceptions.h"
#include "ezsnmp_data_logger/json_utils.h"
#include "test_common.h"

using ezsnmp_data_logger::ConfigError;
using ezsnmp_data_logger::json::Value;

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
