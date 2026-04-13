#include "csv_sink.h"
#include <filesystem>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace ezsnmp_logger {

CsvSink::CsvSink(const SinkConfig& cfg) : csv_dir_(cfg.csv_dir) {
    std::filesystem::create_directories(csv_dir_);
}

CsvSink::~CsvSink() {
    close();
}

CsvSink::FileState& CsvSink::get_or_open(const std::string& dut_name) {
    auto it = files_.find(dut_name);
    if (it != files_.end()) return it->second;

    FileState& fs = files_[dut_name];
    std::string path = csv_dir_ + "/" + dut_name + ".csv";
    // Check if file already exists to avoid re-writing header
    bool exists = std::filesystem::exists(path);
    fs.file.open(path, std::ios::app | std::ios::out);
    if (!fs.file.is_open()) {
        throw std::runtime_error("CsvSink: cannot open " + path);
    }
    if (!exists) {
        fs.file << "ts_unix_ns,metric_name,oid,type_tag,i64,f64,str_val,status\n";
    }
    fs.header_written = true;
    return fs;
}

static std::string escape_csv(const std::string& s) {
    // Wrap in quotes if it contains comma, quote, or newline
    if (s.find_first_of(",\"\n\r") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += '"';
    return out;
}

void CsvSink::write(const Batch& batch) {
    FileState& fs = get_or_open(batch.dut_name);
    for (const auto& s : batch.samples) {
        std::ostringstream row;
        row << batch.ts_unix_ns << ','
            << escape_csv(s.metric_name) << ','
            << escape_csv(s.oid) << ','
            << static_cast<int>(s.type_tag) << ','
            << s.i64 << ','
            << std::setprecision(17) << s.f64 << ','
            << escape_csv(s.str_val) << ','
            << s.status << '\n';
        fs.buffer.push_back(row.str());
    }
    if (fs.buffer.size() >= kFlushSize) {
        for (auto& line : fs.buffer) fs.file << line;
        fs.buffer.clear();
        fs.file.flush();
    }
}

void CsvSink::flush() {
    for (auto& [name, fs] : files_) {
        if (!fs.buffer.empty()) {
            for (auto& line : fs.buffer) fs.file << line;
            fs.buffer.clear();
        }
        fs.file.flush();
    }
}

void CsvSink::close() {
    flush();
    for (auto& [name, fs] : files_) {
        fs.file.close();
    }
    files_.clear();
}

} // namespace ezsnmp_logger
