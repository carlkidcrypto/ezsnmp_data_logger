#pragma once
#include "sink.h"
#include "config.h"
#include <string>
#include <unordered_map>
#include <fstream>
#include <vector>

namespace ezsnmp_logger {

class CsvSink : public ISink {
public:
    explicit CsvSink(const SinkConfig& cfg);
    ~CsvSink() override;

    void write(const Batch& batch) override;
    void flush() override;
    void close() override;
    std::string name() const override { return "csv"; }

private:
    std::string csv_dir_;
    static constexpr size_t kFlushSize = 256;

    struct FileState {
        std::ofstream file;
        bool header_written = false;
        std::vector<std::string> buffer;
    };

    std::unordered_map<std::string, FileState> files_;

    FileState& get_or_open(const std::string& dut_name);
};

} // namespace ezsnmp_logger
