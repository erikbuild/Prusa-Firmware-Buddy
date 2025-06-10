#pragma once

#include "error.hpp"
#include "types.hpp"

#include <cstddef>
#include <expected>

namespace nfcv {

class ReaderWriterInterface {
public:
    using AntennaData = uintptr_t;

    virtual Result<void> field_up(AntennaData antenna_data) = 0;
    virtual void field_down() = 0;

    virtual AntennaData switch_to_next_discovery_atenna() = 0;

    virtual Result<nfcv::UID> inventory() = 0;
    virtual Result<void> stay_quiet(const nfcv::UID &uid) = 0;
    virtual Result<nfcv::TagInfo> get_system_info(const nfcv::UID &uid) = 0;
    virtual Result<void> read_single_block(const nfcv::UID &uid, BlockID block_id, const std::span<std::byte> &buffer) = 0;
    virtual Result<void> write_single_block(const nfcv::UID &uid, BlockID block_id, const std::span<const std::byte> &buffer) = 0;
};

} // namespace nfcv
