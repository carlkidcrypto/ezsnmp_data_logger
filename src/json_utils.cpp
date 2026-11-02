#include "ezsnmp_data_logger/json_utils.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "ezsnmp_data_logger/exceptions.h"

namespace ezsnmp_data_logger {
namespace json {

Value::Value() noexcept : type_(Type::Null) {}
Value::Value(std::nullptr_t) noexcept : type_(Type::Null) {}
Value::Value(bool v) noexcept : type_(Type::Bool), bool_val_(v) {}
Value::Value(int v) noexcept : type_(Type::Int), int_val_(static_cast<int64_t>(v)) {}
Value::Value(int64_t v) noexcept : type_(Type::Int), int_val_(v) {}
Value::Value(double v) noexcept : type_(Type::Double), double_val_(v) {}
Value::Value(char const* v) : type_(Type::String), str_val_(v ? v : "") {}
Value::Value(std::string v) noexcept : type_(Type::String), str_val_(std::move(v)) {}
Value::Value(Array v) noexcept : type_(Type::Array), arr_val_(std::move(v)) {}
Value::Value(Object v) noexcept : type_(Type::Object), obj_val_(std::move(v)) {}

Value Value::array() { return Value(Array{}); }

Value Value::object() { return Value(Object{}); }

bool Value::as_bool() const {
   if (type_ != Type::Bool) {
      throw ConfigError("JSON type error: expected boolean");
   }
   return bool_val_;
}

int64_t Value::as_int64() const {
   if (type_ == Type::Int) {
      return int_val_;
   }
   if (type_ == Type::Double) {
      return static_cast<int64_t>(double_val_);
   }
   throw ConfigError("JSON type error: expected integer");
}

int Value::as_int() const { return static_cast<int>(as_int64()); }

double Value::as_double() const {
   if (type_ == Type::Double) {
      return double_val_;
   }
   if (type_ == Type::Int) {
      return static_cast<double>(int_val_);
   }
   throw ConfigError("JSON type error: expected number");
}

std::string const& Value::as_string() const {
   if (type_ != Type::String) {
      throw ConfigError("JSON type error: expected string");
   }
   return str_val_;
}

Value::Array const& Value::as_array() const {
   if (type_ != Type::Array) {
      throw ConfigError("JSON type error: expected array");
   }
   return arr_val_;
}

Value::Array& Value::as_array() {
   if (type_ != Type::Array) {
      throw ConfigError("JSON type error: expected array");
   }
   return arr_val_;
}

Value::Object const& Value::as_object() const {
   if (type_ != Type::Object) {
      throw ConfigError("JSON type error: expected object");
   }
   return obj_val_;
}

Value::Object& Value::as_object() {
   if (type_ != Type::Object) {
      throw ConfigError("JSON type error: expected object");
   }
   return obj_val_;
}

size_t Value::size() const noexcept {
   if (type_ == Type::Array) {
      return arr_val_.size();
   }
   if (type_ == Type::Object) {
      return obj_val_.size();
   }
   return 0;
}

bool Value::empty() const noexcept { return size() == 0; }

void Value::push_back(Value v) {
   if (type_ != Type::Array) {
      type_ = Type::Array;
      arr_val_.clear();
   }
   arr_val_.push_back(std::move(v));
}

Value const& Value::operator[](size_t index) const { return as_array().at(index); }

Value& Value::operator[](size_t index) { return as_array().at(index); }

bool Value::contains(std::string const& key) const noexcept { return find(key) != nullptr; }

Value const* Value::find(std::string const& key) const noexcept {
   if (type_ != Type::Object) {
      return nullptr;
   }
   for (auto const& kv : obj_val_) {
      if (kv.first == key) {
         return &kv.second;
      }
   }
   return nullptr;
}

Value* Value::find(std::string const& key) noexcept {
   if (type_ != Type::Object) {
      return nullptr;
   }
   for (auto& kv : obj_val_) {
      if (kv.first == key) {
         return &kv.second;
      }
   }
   return nullptr;
}

Value const& Value::at(std::string const& key) const {
   Value const* ptr = find(key);
   if (!ptr) {
      throw ConfigError("Missing required JSON key: '" + key + "'");
   }
   return *ptr;
}

Value const& Value::operator[](std::string const& key) const { return at(key); }

Value& Value::operator[](std::string const& key) {
   if (type_ != Type::Object) {
      type_ = Type::Object;
      obj_val_.clear();
   }
   for (auto& kv : obj_val_) {
      if (kv.first == key) {
         return kv.second;
      }
   }
   obj_val_.emplace_back(key, Value());
   return obj_val_.back().second;
}

bool Value::value_bool(std::string const& key, bool default_val) const {
   Value const* ptr = find(key);
   if (!ptr || ptr->is_null()) {
      return default_val;
   }
   return ptr->as_bool();
}

int64_t Value::value_int64(std::string const& key, int64_t default_val) const {
   Value const* ptr = find(key);
   if (!ptr || ptr->is_null()) {
      return default_val;
   }
   return ptr->as_int64();
}

int Value::value_int(std::string const& key, int default_val) const {
   Value const* ptr = find(key);
   if (!ptr || ptr->is_null()) {
      return default_val;
   }
   return ptr->as_int();
}

double Value::value_double(std::string const& key, double default_val) const {
   Value const* ptr = find(key);
   if (!ptr || ptr->is_null()) {
      return default_val;
   }
   return ptr->as_double();
}

std::string Value::value_string(std::string const& key, std::string const& default_val) const {
   Value const* ptr = find(key);
   if (!ptr || ptr->is_null()) {
      return default_val;
   }
   if (ptr->is_string()) {
      return ptr->as_string();
   }
   if (ptr->is_int()) {
      return std::to_string(ptr->as_int64());
   }
   return default_val;
}

namespace {

void escape_json_string(std::string const& s, std::string& out) {
   out.push_back('"');
   for (unsigned char c : s) {
      switch (c) {
         case '"':
            out += "\\\"";
            break;
         case '\\':
            out += "\\\\";
            break;
         case '\b':
            out += "\\b";
            break;
         case '\f':
            out += "\\f";
            break;
         case '\n':
            out += "\\n";
            break;
         case '\r':
            out += "\\r";
            break;
         case '\t':
            out += "\\t";
            break;
         default:
            if (c < 0x20) {
               char buf[8];
               std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(c));
               out += buf;
            } else {
               out.push_back(static_cast<char>(c));
            }
            break;
      }
   }
   out.push_back('"');
}

void dump_impl(Value const& val, int indent, int depth, std::string& out) {
   switch (val.type()) {
      case Type::Null:
         out += "null";
         break;
      case Type::Bool:
         out += val.as_bool() ? "true" : "false";
         break;
      case Type::Int:
         out += std::to_string(val.as_int64());
         break;
      case Type::Double: {
         double d = val.as_double();
         if (!std::isfinite(d)) {
            out += "null";
         } else {
            std::ostringstream oss;
            oss << std::setprecision(15) << d;
            std::string rendered = oss.str();
            if (rendered.find_first_of(".eE") == std::string::npos) {
               rendered += ".0";
            }
            out += rendered;
         }
         break;
      }
      case Type::String:
         escape_json_string(val.as_string(), out);
         break;
      case Type::Array: {
         auto const& arr = val.as_array();
         if (arr.empty()) {
            out += "[]";
            break;
         }
         out.push_back('[');
         if (indent >= 0) {
            out.push_back('\n');
         }
         for (size_t i = 0; i < arr.size(); ++i) {
            if (indent >= 0) {
               out.append(static_cast<size_t>((depth + 1) * indent), ' ');
            }
            dump_impl(arr[i], indent, depth + 1, out);
            if (i + 1 < arr.size()) {
               out.push_back(',');
            }
            if (indent >= 0) {
               out.push_back('\n');
            }
         }
         if (indent >= 0) {
            out.append(static_cast<size_t>(depth * indent), ' ');
         }
         out.push_back(']');
         break;
      }
      case Type::Object: {
         auto const& obj = val.as_object();
         if (obj.empty()) {
            out += "{}";
            break;
         }
         out.push_back('{');
         if (indent >= 0) {
            out.push_back('\n');
         }
         for (size_t i = 0; i < obj.size(); ++i) {
            if (indent >= 0) {
               out.append(static_cast<size_t>((depth + 1) * indent), ' ');
            }
            escape_json_string(obj[i].first, out);
            out.push_back(':');
            if (indent >= 0) {
               out.push_back(' ');
            }
            dump_impl(obj[i].second, indent, depth + 1, out);
            if (i + 1 < obj.size()) {
               out.push_back(',');
            }
            if (indent >= 0) {
               out.push_back('\n');
            }
         }
         if (indent >= 0) {
            out.append(static_cast<size_t>(depth * indent), ' ');
         }
         out.push_back('}');
         break;
      }
   }
}

class Parser {
  public:
   explicit Parser(std::string const& text) : text_(text) {}

   Value parse_document() {
      skip_ws();
      if (eof()) {
         error("empty JSON document");
      }
      Value v = parse_value(0);
      skip_ws();
      if (!eof()) {
         error("unexpected trailing characters after JSON value");
      }
      return v;
   }

  private:
   std::string const& text_;
   size_t pos_ = 0;
   size_t line_ = 1;
   size_t col_ = 1;

   bool eof() const noexcept { return pos_ >= text_.size(); }
   char peek() const noexcept { return eof() ? '\0' : text_[pos_]; }

   char advance() noexcept {
      char c = text_[pos_++];
      if (c == '\n') {
         ++line_;
         col_ = 1;
      } else {
         ++col_;
      }
      return c;
   }

   [[noreturn]] void error(std::string const& msg) const {
      std::ostringstream oss;
      oss << "JSON parse error at line " << line_ << ", column " << col_ << ": " << msg;
      throw ConfigError(oss.str());
   }

   void skip_ws() noexcept {
      while (!eof()) {
         char c = peek();
         if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
         } else {
            break;
         }
      }
   }

   Value parse_value(int depth) {
      if (depth > 128) {
         error("maximum JSON nesting depth exceeded");
      }
      skip_ws();
      if (eof()) {
         error("unexpected end of input");
      }
      char c = peek();
      switch (c) {
         case '{':
            return parse_object(depth + 1);
         case '[':
            return parse_array(depth + 1);
         case '"':
            return Value(parse_string());
         case 't':
            consume_literal("true");
            return Value(true);
         case 'f':
            consume_literal("false");
            return Value(false);
         case 'n':
            consume_literal("null");
            return Value(nullptr);
         default:
            if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
               return parse_number();
            }
            error(std::string("unexpected character '") + c + "'");
      }
   }

   void consume_literal(char const* lit) {
      for (char const* p = lit; *p != '\0'; ++p) {
         if (eof() || advance() != *p) {
            error(std::string("expected literal '") + lit + "'");
         }
      }
   }

   uint32_t parse_hex4() {
      uint32_t code = 0;
      for (int i = 0; i < 4; ++i) {
         if (eof()) {
            error("incomplete \\uXXXX escape sequence");
         }
         char c = advance();
         code <<= 4;
         if (c >= '0' && c <= '9') {
            code |= static_cast<uint32_t>(c - '0');
         } else if (c >= 'a' && c <= 'f') {
            code |= static_cast<uint32_t>(10 + (c - 'a'));
         } else if (c >= 'A' && c <= 'F') {
            code |= static_cast<uint32_t>(10 + (c - 'A'));
         } else {
            error("invalid hex digit in \\uXXXX escape");
         }
      }
      return code;
   }

   static void append_utf8(uint32_t cp, std::string& out) {
      if (cp <= 0x7F) {
         out.push_back(static_cast<char>(cp));
      } else if (cp <= 0x7FF) {
         out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
         out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
      } else if (cp <= 0xFFFF) {
         out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
         out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
         out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
      } else {
         out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
         out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
         out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
         out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
      }
   }

   std::string parse_string() {
      if (advance() != '"') {
         error("expected '\"'");
      }
      std::string out;
      while (!eof()) {
         char c = advance();
         if (c == '"') {
            return out;
         }
         if (static_cast<unsigned char>(c) < 0x20) {
            error("unescaped control character in string");
         }
         if (c != '\\') {
            out.push_back(c);
            continue;
         }
         if (eof()) {
            error("unterminated escape sequence in string");
         }
         char esc = advance();
         switch (esc) {
            case '"':
               out.push_back('"');
               break;
            case '\\':
               out.push_back('\\');
               break;
            case '/':
               out.push_back('/');
               break;
            case 'b':
               out.push_back('\b');
               break;
            case 'f':
               out.push_back('\f');
               break;
            case 'n':
               out.push_back('\n');
               break;
            case 'r':
               out.push_back('\r');
               break;
            case 't':
               out.push_back('\t');
               break;
            case 'u': {
               uint32_t cp = parse_hex4();
               if (cp >= 0xD800 && cp <= 0xDBFF) {
                  if (pos_ + 2 <= text_.size() && text_[pos_] == '\\' && text_[pos_ + 1] == 'u') {
                     advance();
                     advance();
                     uint32_t low = parse_hex4();
                     if (low < 0xDC00 || low > 0xDFFF) {
                        error("invalid UTF-16 low surrogate");
                     }
                     cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
                  } else {
                     error("unpaired UTF-16 high surrogate");
                  }
               }
               append_utf8(cp, out);
               break;
            }
            default:
               error(std::string("invalid escape character '\\") + esc + "'");
         }
      }
      error("unterminated string literal");
   }

   Value parse_number() {
      size_t start = pos_;
      bool is_float = false;

      if (peek() == '-') {
         advance();
      }
      if (eof()) {
         error("incomplete number");
      }
      if (peek() == '0') {
         advance();
         if (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) {
            error("leading zeros are not permitted in JSON numbers");
         }
      } else if (std::isdigit(static_cast<unsigned char>(peek()))) {
         while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
         }
      } else {
         error("expected digit in number");
      }

      if (!eof() && peek() == '.') {
         is_float = true;
         advance();
         if (eof() || !std::isdigit(static_cast<unsigned char>(peek()))) {
            error("expected digit after decimal point");
         }
         while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
         }
      }

      if (!eof() && (peek() == 'e' || peek() == 'E')) {
         is_float = true;
         advance();
         if (!eof() && (peek() == '+' || peek() == '-')) {
            advance();
         }
         if (eof() || !std::isdigit(static_cast<unsigned char>(peek()))) {
            error("expected digit in exponent");
         }
         while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
         }
      }

      std::string num_str = text_.substr(start, pos_ - start);
      if (!is_float) {
         try {
            size_t idx = 0;
            long long iv = std::stoll(num_str, &idx, 10);
            if (idx == num_str.size()) {
               return Value(static_cast<int64_t>(iv));
            }
         } catch (...) {
            // Fall through to double if out of int64 range
         }
      }

      char* end_ptr = nullptr;
      double dv = std::strtod(num_str.c_str(), &end_ptr);
      if (end_ptr != num_str.c_str() + num_str.size()) {
         error("invalid number literal '" + num_str + "'");
      }
      return Value(dv);
   }

   Value parse_array(int depth) {
      advance(); // '['
      Value arr = Value::array();
      skip_ws();
      if (!eof() && peek() == ']') {
         advance();
         return arr;
      }
      while (true) {
         arr.push_back(parse_value(depth));
         skip_ws();
         if (eof()) {
            error("unterminated JSON array");
         }
         char c = advance();
         if (c == ']') {
            break;
         }
         if (c != ',') {
            error("expected ',' or ']' in array");
         }
      }
      return arr;
   }

   Value parse_object(int depth) {
      advance(); // '{'
      Value obj = Value::object();
      skip_ws();
      if (!eof() && peek() == '}') {
         advance();
         return obj;
      }
      while (true) {
         skip_ws();
         if (peek() != '"') {
            error("expected string key in JSON object");
         }
         std::string key = parse_string();
         skip_ws();
         if (eof() || advance() != ':') {
            error("expected ':' after object key '" + key + "'");
         }
         Value val = parse_value(depth);
         obj[key] = std::move(val);
         skip_ws();
         if (eof()) {
            error("unterminated JSON object");
         }
         char c = advance();
         if (c == '}') {
            break;
         }
         if (c != ',') {
            error("expected ',' or '}' in object");
         }
      }
      return obj;
   }
};

} // namespace

std::string Value::dump(int indent) const {
   std::string out;
   dump_impl(*this, indent, 0, out);
   return out;
}

Value Value::parse(std::string const& text) {
   Parser p(text);
   return p.parse_document();
}

Value Value::parse_file(std::string const& file_path) {
   std::ifstream ifs(file_path, std::ios::in | std::ios::binary);
   if (!ifs) {
      throw ConfigError("Cannot open JSON file: " + file_path);
   }
   std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
   return parse(content);
}

} // namespace json
} // namespace ezsnmp_data_logger
