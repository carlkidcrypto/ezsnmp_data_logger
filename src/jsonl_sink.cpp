#include "ezsnmp_data_logger/jsonl_sink.h"

#include <filesystem>

#include "ezsnmp_data_logger/exceptions.h"
#include "ezsnmp_data_logger/json_utils.h"

namespace ezsnmp_data_logger {

namespace {

std::string sanitize_jsonl_name(std::string const& name) {
   std::string safe;
   safe.reserve(name.size());
   for (char c : name) {
      if (c == '/' || c == '\\' || c == ':' || c == '\0') {
         safe.push_back('_');
      } else {
         safe.push_back(c);
      }
   }
   return safe.empty() ? "unknown_dut" : safe;
}

} // namespace

JsonlSink::JsonlSink(SinkConfig const& cfg)
    : dir_(cfg.dir),
      flush_every_n_(cfg.flush_every_n > 0 ? static_cast<size_t>(cfg.flush_every_n) : 64) {
   std::error_code ec;
   std::filesystem::create_directories(dir_, ec);
   if (ec) {
      throw SinkError("JsonlSink: failed to create directory '" + dir_ + "': " + ec.message());
   }
}

JsonlSink::~JsonlSink() { close(); }

JsonlSink::FileState& JsonlSink::get_or_open(std::string const& dut_name) {
   auto it = files_.find(dut_name);
   if (it != files_.end()) {
      return it->second;
   }

   FileState& fs = files_[dut_name];
   std::filesystem::path file_path =
       std::filesystem::path(dir_) / (sanitize_jsonl_name(dut_name) + ".jsonl");
   fs.file.open(file_path, std::ios::app | std::ios::out);
   if (!fs.file.is_open()) {
      files_.erase(dut_name);
      throw SinkError("JsonlSink: cannot open file '" + file_path.string() + "'");
   }
   return fs;
}

void JsonlSink::write(Batch const& batch) {
   FileState& fs = get_or_open(batch.dut_name);
   for (auto const& s : batch.samples) {
      json::Value row = json::Value::object();
      row["ts_unix_ns"] = batch.ts_unix_ns;
      row["dut_name"] = batch.dut_name;
      row["dut_ip"] = batch.dut_ip;
      row["metric_name"] = s.metric_name;
      row["oid"] = s.oid;
      row["oid_index"] = s.oid_index;
      row["snmp_type"] = s.snmp_type;
      row["raw_value"] = s.raw_value;
      row["type_tag"] = static_cast<int>(s.type_tag);
      row["i64"] = s.i64;
      row["f64"] = s.f64;
      row["str_val"] = s.str_val;
      row["status"] = s.status;
      fs.buffer.push_back(row.dump(-1) + "\n");
   }
   if (fs.buffer.size() >= flush_every_n_) {
      for (auto const& line : fs.buffer) {
         fs.file << line;
      }
      fs.buffer.clear();
      fs.file.flush();
   }
}

void JsonlSink::flush() {
   for (auto& kv : files_) {
      auto& fs = kv.second;
      if (!fs.buffer.empty()) {
         for (auto const& line : fs.buffer) {
            fs.file << line;
         }
         fs.buffer.clear();
      }
      fs.file.flush();
   }
}

void JsonlSink::close() noexcept {
   flush();
   for (auto& kv : files_) {
      kv.second.file.close();
   }
   files_.clear();
}

} // namespace ezsnmp_data_logger
