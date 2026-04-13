#pragma once
#include "batch.h"
#include <string>

namespace ezsnmp_logger {

class ISink {
public:
    virtual ~ISink() = default;
    virtual void write(const Batch& batch) = 0;
    virtual void flush() = 0;
    virtual void close() = 0;
    virtual std::string name() const = 0;
};

} // namespace ezsnmp_logger
