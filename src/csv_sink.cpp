#include "ezsnmp_data_logger/csv_sink.h"

#include <filesystem>
#include <iomanip>
#include <sstream>

#include "ezsnmp_data_logger/exceptions.h"

namespace ezsnmp_data_logger {

namespace {

std::string escape_csv_field(std::string const& s) {
   if (s.find_first_of(",\"\n\r") == std::string::npos) {
      return s;
   }
   std::string out = "\"";
   for (char c : s) {
      if (c == '"') {
         out += "\"\"";
      } else {
         out.push_back(c);
      }
   }
   out.push_back('"');
   return out;
}

std::string sanitize_filename(std::string const& name) {
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

CsvSink::CsvSink(SinkConfig const& cfg)
    : csv_dir_(cfg.dir),
      flush_every_n_(cfg.flush_every_n > 0 ? static_cast<size_t>(cfg.flush_every_n) : 64) {
   std::error_code ec;
   std::filesystem::create_directories(csv_dir_, ec);
   if (ec) {
      throw SinkError("CsvSink: failed to create directory '" + csv_dir_ + "': " + ec.message());
   }
}

CsvSink::~CsvSink() { close(); }

CsvSink::FileState& CsvSink::get_or_open(std::string const& dut_name) {
   auto it = files_.find(dut_name);
   if (it != files_.end()) {
      return it->second;
   }

   FileState& fs = files_[dut_name];
   std::filesystem::path file_path =
       std::filesystem::path(csv_dir_) / (sanitize_filename(dut_name) + ".csv");
   bool exists = std::filesystem::exists(file_path) &&
                 std::filesystem::is_regular_file(file_path) &&
                 std::filesystem::file_size(file_path) > 0;
   fs.file.open(file_path, std::ios::app | std::ios::out);
   if (!fs.file.is_open()) {
      files_.erase(dut_name);
      throw SinkError("CsvSink: cannot open file '" + file_path.string() + "'");
   }
   if (!exists) {
      fs.file << "ts_unix_ns,dut_name,dut_ip,metric_name,oid,oid_index,snmp_type,raw_value,"
                 "type_tag,i64,f64,str_val,status\n";
      fs.file.flush();
   }
   return fs;
}

void CsvSink::write(Batch const& batch) {
   FileState& fs = get_or_open(batch.dut_name);
   for (auto const& s : batch.samples) {
      std::ostringstream row;
      row << batch.ts_unix_ns << ',' << escape_csv_field(batch.dut_name) << ','
          << escape_csv_field(batch.dut_ip) << ',' << escape_csv_field(s.metric_name) << ','
          << escape_csv_field(s.oid) << ',' << escape_csv_field(s.oid_index) << ','
          << escape_csv_field(s.snmp_type) << ',' << escape_csv_field(s.raw_value) << ','
          << static_cast<int>(s.type_tag) << ',' << s.i64 << ',' << std::setprecision(15) << s.f64
          << ',' << escape_csv_field(s.str_val) << ',' << s.status << '\n';
      fs.buffer.push_back(row.str());
   }
   if (fs.buffer.size() >= flush_every_n_) {
      for (auto const& line : fs.buffer) {
         fs.file << line;
      }
      fs.buffer.clear();
      fs.file.flush();
   }
}

void CsvSink::flush() {
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

void CsvSink::close() noexcept {
   flush();
   for (auto& kv : files_) {
      kv.second.file.close();
   }
   files_.clear();
}

} // namespace ezsnmp_data_logger
