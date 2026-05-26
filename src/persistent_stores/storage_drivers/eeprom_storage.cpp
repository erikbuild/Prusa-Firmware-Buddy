#include "eeprom_storage.hpp"

#include <common/st25dv64k.h>

size_t EEPROMStorage::read_bytes(size_t address, WritableBytes buffer) {
    st25dv64k_user_read_bytes(address, buffer.data(), buffer.size());
    return buffer.size();
}

size_t EEPROMStorage::write_bytes(size_t address, Bytes data) {
    st25dv64k_user_write_bytes(address, data.data(), data.size());
    bytes_written_.fetch_add(data.size(), std::memory_order_relaxed);
    return data.size();
}
