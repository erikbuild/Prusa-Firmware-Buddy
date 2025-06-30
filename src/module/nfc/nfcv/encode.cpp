#include <nfcv/encode.hpp>

#include <cassert>

nfcv::Encoder1Of4::Encoder1Of4(MsgBuilder &msg_builder)
    : builder(msg_builder)
    , crc()
    , did_finalize(false) {
    builder.push_back(std::byte { 0x21 }); // 1 of 4 SOF
}

nfcv::Encoder1Of4::~Encoder1Of4() {
    if (!did_finalize) {
        std::abort();
    }
}

void nfcv::Encoder1Of4::append_byte(std::byte byte) { append_byte_impl(byte); }

void nfcv::Encoder1Of4::append_byte_impl(std::byte byte, bool calculate_crc) {
    static constexpr std::array bit_pattern_1_of_4 {
        std::byte { 0x02 },
        std::byte { 0x08 },
        std::byte { 0x20 },
        std::byte { 0x80 },
    };
    for (std::size_t i = 0; i < 8; i += 2) {
        const auto bit_pair = std::to_integer<uint8_t>(byte >> i) & 0x03;
        builder.push_back(bit_pattern_1_of_4.at(bit_pair));
    }

    if (calculate_crc) {
        crc.add_byte(std::to_integer<uint8_t>(byte));
    }
}

void nfcv::Encoder1Of4::append_bytes(const std::span<const std::byte> &bytes) { append_bytes_impl(bytes); }

void nfcv::Encoder1Of4::append_bytes_impl(const std::span<const std::byte> &buffer, bool calculate_crc) {
    for (const auto byte : buffer) {
        append_byte_impl(byte, calculate_crc);
    }
}

void nfcv::Encoder1Of4::append_crc_and_finalize() {
    if (did_finalize) {
        std::abort();
    }
    did_finalize = true;
    append_raw_impl(crc.get_result(), false);
    builder.push_back(std::byte { 0x04 }); // EOF
}

namespace nfcv::impl {

template <typename Command>
constexpr std::byte command_flags() {
    return static_cast<std::byte>(MessageFlag::high_data_rate | MessageFlagNoInv::address_flag);
}

template <>
constexpr std::byte command_flags<command::Inventory>() {
    return static_cast<std::byte>(MessageFlag::high_data_rate | MessageFlag::inventory_flag | MessageFlagInv::nb_slots_flag);
}

constexpr std::size_t expected_message_size([[maybe_unused]] const command::Inventory &command) {
    return 3;
}

Result<void> construct_rest(Encoder1Of4 &encoder, [[maybe_unused]] const command::Inventory &command) {
    encoder.append_byte(std::byte { 0x00 }); // No optional data
    return {};
}

constexpr std::size_t expected_message_size([[maybe_unused]] const command::SystemInfo &command) {
    return 10;
}

Result<void> construct_rest(Encoder1Of4 &encoder, const command::SystemInfo &command) {
    encoder.append_bytes(command.request.uid);
    return {};
}

constexpr std::size_t expected_message_size([[maybe_unused]] const command::ReadSingleBlock &command) {
    return 11;
}

Result<void> construct_rest(Encoder1Of4 &encoder, const command::ReadSingleBlock &command) {
    encoder.append_bytes(command.request.uid);
    encoder.append_raw(command.request.block_address);
    return {};
}

constexpr std::size_t expected_message_size(const command::WriteSingleBlock &command) {
    return 11 + command.request.block_buffer.size();
}

Result<void> construct_rest(Encoder1Of4 &encoder, const command::WriteSingleBlock &command) {
    encoder.append_bytes(command.request.uid);
    encoder.append_raw(command.request.block_address);
    encoder.append_bytes(command.request.block_buffer);
    return {};
}

constexpr std::size_t expected_message_size([[maybe_unused]] const command::StayQuiet &command) {
    return 10;
}

Result<void> construct_rest(Encoder1Of4 &encoder, const command::StayQuiet &command) {
    for (const auto &byte : command.request.uid) {
        encoder.append_byte(byte);
    }
    return {};
}

} // namespace nfcv::impl

nfcv::Result<void> nfcv::construct_command(MsgBuilder &builder, const Command &command) {
    nfcv::Encoder1Of4 encoder(builder);
    return std::visit([&]<typename T>(const T &cmd) -> nfcv::Result<void> {
        const auto expected_size = nfcv::Encoder1Of4::calculate_message_size(impl::expected_message_size(cmd));
        if (builder.capacity() < expected_size) {
            return std::unexpected(Error::buffer_overflow);
        }

        encoder.append_byte(impl::command_flags<T>());
        encoder.append_byte(T::cmd_id);
        const auto res = impl::construct_rest(encoder, cmd);
        encoder.append_crc_and_finalize();

        assert(builder.size() == expected_size);
        return res;
    },
        command);
}
