#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace ezsnmp_logger {

enum class TypeTag : int { Null = 0, Int64 = 1, Float64 = 2, Str = 3 };

struct Sample {
    std::string metric_name;
    std::string oid;
    TypeTag type_tag;
    int64_t i64;
    double f64;
    std::string str_val;
    int status;
};

struct Batch {
    int64_t ts_unix_ns;
    std::string dut_name;
    std::string dut_ip;
    std::vector<Sample> samples;
};

// Decode a MessagePack blob into a Batch. Throws on error.
Batch decode_batch(const void* data, size_t size);

} // namespace ezsnmp_logger
