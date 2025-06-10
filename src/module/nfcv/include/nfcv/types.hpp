#pragma once

#include <array>
#include <span>
#include <optional>
#include <cstdint>
#include <utility>

namespace nfcv {
constexpr size_t UID_SIZE = 8;
using UID = std::array<std::byte, UID_SIZE>;
using UIDView = std::span<std::byte, UID_SIZE>;
using UIDConstView = std::span<const std::byte, UID_SIZE>;
constexpr std::byte UID_MSB { 0xe0 };
constexpr size_t UID_MSB_INDEX = UID_SIZE - 1;

constexpr size_t MAX_BLOCK_SIZE_IN_BYTES = 32;
// constexpr size_t MAX_NUMBER_OF_BLOCKS = 256;

using BlockID = uint8_t;

struct TagInfo {
    using DSFID = uint8_t;
    using AFI = uint8_t;
    struct MemorySize {
        uint8_t block_size;
        uint8_t block_count;
    };
    using ICRef = uint8_t;

    std::optional<DSFID> dsfid;
    std::optional<AFI> afi;
    std::optional<MemorySize> mem_size;
    std::optional<ICRef> ic_ref;
};

enum class MessageFlag : uint8_t {
    two_subcarriers = 1 << 0,
    high_data_rate = 1 << 1,
    inventory_flag = 1 << 2,
    protocol_extension = 1 << 3,
};

enum class MessageFlagNoInv : uint8_t {
    select_flag = 1 << 4,
    address_flag = 1 << 5,
    custom_flag = 1 << 6,
};

enum class MessageFlagInv : uint8_t {
    afi_flag = 1 << 4,
    /// if set - 1 slot, otherwise 16 slots
    nb_slots_flag = 1 << 5,
    custom_flag = 1 << 6,
};

constexpr MessageFlag operator|(MessageFlag a, MessageFlag b) { return static_cast<MessageFlag>(std::to_underlying(a) | std::to_underlying(b)); }
constexpr MessageFlagNoInv operator|(MessageFlagNoInv a, MessageFlag b) { return static_cast<MessageFlagNoInv>(std::to_underlying(a) | std::to_underlying(b)); }
constexpr MessageFlagNoInv operator|(MessageFlag a, MessageFlagNoInv b) { return static_cast<MessageFlagNoInv>(std::to_underlying(a) | std::to_underlying(b)); }
constexpr MessageFlagNoInv operator|(MessageFlagNoInv a, MessageFlagNoInv b) { return static_cast<MessageFlagNoInv>(std::to_underlying(a) | std::to_underlying(b)); }
constexpr MessageFlagInv operator|(MessageFlagInv a, MessageFlag b) { return static_cast<MessageFlagInv>(std::to_underlying(a) | std::to_underlying(b)); }
constexpr MessageFlagInv operator|(MessageFlag a, MessageFlagInv b) { return static_cast<MessageFlagInv>(std::to_underlying(a) | std::to_underlying(b)); }
constexpr MessageFlagInv operator|(MessageFlagInv a, MessageFlagInv b) { return static_cast<MessageFlagInv>(std::to_underlying(a) | std::to_underlying(b)); }
} // namespace nfcv
