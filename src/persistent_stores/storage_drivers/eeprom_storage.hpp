#pragma once

#include <atomic>
#include <cstdint>

#include "storage.hpp"

class EEPROMStorage : public configuration_store::Storage {
    // TODO detect errors and return them
public:
    size_t read_bytes(size_t address, WritableBytes buffer) override;
    size_t write_bytes(size_t address, Bytes data) override;
    void flush() override;
};

inline configuration_store::Storage &EEPROMInstance() {
    static EEPROMStorage storage {};
    return storage;
}
