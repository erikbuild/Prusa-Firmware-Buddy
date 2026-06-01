/// @file
#pragma once

#include <atomic>
#include <cstdint>
#include <utils/byte_utils.hpp>

namespace configuration_store {

class Storage {

public:
    virtual size_t read_bytes(size_t address, WritableBytes buffer) = 0;
    virtual size_t write_bytes(size_t address, Bytes data) = 0;
    virtual void flush() = 0;
    Storage() = default;
    Storage(const Storage &other) = delete;
    Storage(Storage &&other) = delete;
    Storage &operator=(const Storage &other) = delete;
    Storage &operator=(Storage &&other) = delete;

public:
    auto bytes_written() const {
        return bytes_written_.load(std::memory_order_relaxed);
    }

protected:
    /// Number of bytes written to the EEPROM - metric
    std::atomic<uint32_t> bytes_written_ = 0;
};
} // namespace configuration_store
