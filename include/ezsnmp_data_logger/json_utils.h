#pragma once

#ifndef SWIG

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace ezsnmp_data_logger {
namespace json {

/**
 * @brief Enumeration of RFC-8259 JSON value types.
 */
enum class Type { Null, Bool, Int, Double, String, Array, Object };

/**
 * @brief Self-contained, order-preserving RFC-8259 JSON value representation.
 *
 * Designed for C++17 portability across Linux, macOS, Windows, and all SWIG
 * extension toolchains without external header-only library dependencies.
 */
class Value {
  public:
   using Array = std::vector<Value>;
   using Member = std::pair<std::string, Value>;
   using Object = std::vector<Member>;

   Value() noexcept;
   Value(std::nullptr_t) noexcept;
   Value(bool v) noexcept;
   Value(int v) noexcept;
   Value(int64_t v) noexcept;
   Value(double v) noexcept;
   Value(char const* v);
   Value(std::string v) noexcept;
   Value(Array v) noexcept;
   Value(Object v) noexcept;

   static Value array();
   static Value object();

   Type type() const noexcept { return type_; }
   bool is_null() const noexcept { return type_ == Type::Null; }
   bool is_bool() const noexcept { return type_ == Type::Bool; }
   bool is_int() const noexcept { return type_ == Type::Int; }
   bool is_double() const noexcept { return type_ == Type::Double; }
   bool is_number() const noexcept { return type_ == Type::Int || type_ == Type::Double; }
   bool is_string() const noexcept { return type_ == Type::String; }
   bool is_array() const noexcept { return type_ == Type::Array; }
   bool is_object() const noexcept { return type_ == Type::Object; }

   bool as_bool() const;
   int64_t as_int64() const;
   int as_int() const;
   double as_double() const;
   std::string const& as_string() const;
   Array const& as_array() const;
   Array& as_array();
   Object const& as_object() const;
   Object& as_object();

   size_t size() const noexcept;
   bool empty() const noexcept;

   // Array helpers
   void push_back(Value v);
   Value const& operator[](size_t index) const;
   Value& operator[](size_t index);

   // Object helpers (order-preserving)
   bool contains(std::string const& key) const noexcept;
   Value const* find(std::string const& key) const noexcept;
   Value* find(std::string const& key) noexcept;
   Value const& at(std::string const& key) const;
   Value const& operator[](std::string const& key) const;
   Value& operator[](std::string const& key);

   bool value_bool(std::string const& key, bool default_val) const;
   int64_t value_int64(std::string const& key, int64_t default_val) const;
   int value_int(std::string const& key, int default_val) const;
   double value_double(std::string const& key, double default_val) const;
   std::string value_string(std::string const& key, std::string const& default_val) const;

   /**
    * @brief Serialize this JSON value to an RFC-8259 string.
    * @param indent Spaces per indentation level, or -1 for compact single-line JSON.
    * @return Serialized JSON text.
    */
   std::string dump(int indent = -1) const;

   /**
    * @brief Parse an RFC-8259 JSON string into a Value.
    * @param text Input UTF-8 JSON document.
    * @return Parsed JSON Value.
    * @throws ConfigError if syntax is invalid.
    */
   static Value parse(std::string const& text);

   /**
    * @brief Load and parse a JSON file from disk.
    * @param file_path Path to the JSON file.
    * @return Parsed JSON Value.
    * @throws ConfigError if the file cannot be read or contains invalid JSON.
    */
   static Value parse_file(std::string const& file_path);

  private:
   Type type_ = Type::Null;
   bool bool_val_ = false;
   int64_t int_val_ = 0;
   double double_val_ = 0.0;
   std::string str_val_;
   Array arr_val_;
   Object obj_val_;
};

} // namespace json
} // namespace ezsnmp_data_logger

#endif // SWIG
