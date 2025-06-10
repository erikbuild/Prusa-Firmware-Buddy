#pragma once

#include "commands.hpp"
#include "error.hpp"

#include <inplace_vector.hpp>
#include <iso13239/crc.hpp>

#include <expected>

namespace nfcv {
using MsgBuilder = stdext::inplace_vector<std::byte, 512>;

Result<void> construct_command(MsgBuilder &builder, const Command &cmd);

/// One of two encoding methods for NFC-V VCD messages.
/// Encodes every bit pair as 1 of 4 valid values.
class Encoder1Of4 {
public:
    static constexpr size_t calculate_message_size(size_t msg_size_in_bytes) {
        return 2 /*SOF + EOF bytes*/ + (msg_size_in_bytes + 2 /*crc*/) * 4 /*stored as bit pairs so 4 per byte*/;
    }

    Encoder1Of4(MsgBuilder &builder);

    void append_byte(std::byte byte);
    void append_bytes(const std::span<const std::byte> &bytes);
    void append_crc_and_finalize();

private:
    void append_byte_impl(std::byte byte, bool calculate_crc = true);
    void append_bytes_impl(const std::span<const std::byte> &bytes, bool calculate_crc = true);
    MsgBuilder &builder;
    iso13239::CRC crc;
};
} // namespace nfcv
