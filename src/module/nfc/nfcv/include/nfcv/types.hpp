#pragma once

#include <array>
#include <span>
#include <optional>
#include <cstdint>
#include <utility>

namespace nfcv {
constexpr size_t UID_SIZE = 8;
constexpr std::byte UID_MSB { 0xe0 };
constexpr size_t UID_MSB_INDEX = UID_SIZE - 1;

constexpr size_t MAX_BLOCK_SIZE_IN_BYTES = 32;
// constexpr size_t MAX_NUMBER_OF_BLOCKS = 256;

/// Magic constant you're supposed to put before the UID for SLIX2 extension commands
/// The spec file is not very clear what the shorthand means,
// it's likely something to do with "integrated circuit manufacturer code"
constexpr std::byte SLIX_IC_MFG { 0x04 };

using UID = std::array<std::byte, UID_SIZE>;
using BlockID = uint8_t;
using AFI = uint8_t;
using DSFID = uint8_t;

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

/// Four-byte variant of the capability container
struct CapabilityContainer4 {
    static constexpr uint8_t expected_magic_number = 0xE1;
    uint8_t magic_number;
    uint8_t version : 4;
    uint8_t access_conditions : 4;

    /// Size of the chip in bytes divided by 8
    uint8_t memory_length_8;
    uint8_t capabilities;
};
static_assert(sizeof(CapabilityContainer4) == 4);

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

using SLIX2Password = uint32_t;

enum class SLIX2PasswordID : uint8_t {
    read = 0x01,
    write = 0x02,
    privacy = 0x04,
    destroy = 0x08,
    eas_afi = 0x10,

    ///! Enum is not linear!
    _password_count = 5,
};

enum class SLIX2PageProtection : uint8_t {
    none = 0b00,

    /// Reading and writing is protected by the read password
    rw_read_password = 0b01,

    /// Reading is unprotected, writing is protected by the write password
    write = 0b10,

    /// Reading is protected by the read password, writing is protected by the write password
    rw_separate_passwords = 0b11,
};

constexpr MessageFlag operator|(MessageFlag a, MessageFlag b) { return static_cast<MessageFlag>(std::to_underlying(a) | std::to_underlying(b)); }
constexpr MessageFlagNoInv operator|(MessageFlagNoInv a, MessageFlag b) { return static_cast<MessageFlagNoInv>(std::to_underlying(a) | std::to_underlying(b)); }
constexpr MessageFlagNoInv operator|(MessageFlag a, MessageFlagNoInv b) { return static_cast<MessageFlagNoInv>(std::to_underlying(a) | std::to_underlying(b)); }
constexpr MessageFlagNoInv operator|(MessageFlagNoInv a, MessageFlagNoInv b) { return static_cast<MessageFlagNoInv>(std::to_underlying(a) | std::to_underlying(b)); }
constexpr MessageFlagInv operator|(MessageFlagInv a, MessageFlag b) { return static_cast<MessageFlagInv>(std::to_underlying(a) | std::to_underlying(b)); }
constexpr MessageFlagInv operator|(MessageFlag a, MessageFlagInv b) { return static_cast<MessageFlagInv>(std::to_underlying(a) | std::to_underlying(b)); }
constexpr MessageFlagInv operator|(MessageFlagInv a, MessageFlagInv b) { return static_cast<MessageFlagInv>(std::to_underlying(a) | std::to_underlying(b)); }

} // namespace nfcv
