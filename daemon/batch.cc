#include "batch.h"
#include <msgpack.hpp>
#include <stdexcept>
#include <string>

namespace ezsnmp_logger {

Batch decode_batch(const void* data, size_t size) {
    msgpack::object_handle oh = msgpack::unpack(
        reinterpret_cast<const char*>(data), size);
    const msgpack::object& obj = oh.get();

    if (obj.type != msgpack::type::MAP) {
        throw std::runtime_error("decode_batch: top-level object must be a MAP");
    }

    Batch batch{};
    const auto& map = obj.via.map;
    for (uint32_t i = 0; i < map.size; ++i) {
        const std::string key = map.ptr[i].key.as<std::string>();
        const msgpack::object& val = map.ptr[i].val;

        if (key == "ts_unix_ns") {
            batch.ts_unix_ns = val.as<int64_t>();
        } else if (key == "dut_name") {
            batch.dut_name = val.as<std::string>();
        } else if (key == "dut_ip") {
            batch.dut_ip = val.as<std::string>();
        } else if (key == "samples") {
            if (val.type != msgpack::type::ARRAY) {
                throw std::runtime_error("decode_batch: 'samples' must be an ARRAY");
            }
            const auto& sarr = val.via.array;
            batch.samples.reserve(sarr.size);
            for (uint32_t si = 0; si < sarr.size; ++si) {
                const msgpack::object& sobj = sarr.ptr[si];
                if (sobj.type != msgpack::type::ARRAY || sobj.via.array.size < 7) {
                    throw std::runtime_error("decode_batch: each sample must be a 7-element ARRAY");
                }
                const auto& sa = sobj.via.array;
                Sample s{};
                s.metric_name = sa.ptr[0].as<std::string>();
                s.oid         = sa.ptr[1].as<std::string>();
                s.type_tag    = static_cast<TypeTag>(sa.ptr[2].as<int>());
                s.i64         = sa.ptr[3].as<int64_t>();
                s.f64         = sa.ptr[4].as<double>();
                s.str_val     = sa.ptr[5].as<std::string>();
                s.status      = sa.ptr[6].as<int>();
                batch.samples.push_back(std::move(s));
            }
        }
    }
    return batch;
}

} // namespace ezsnmp_logger
