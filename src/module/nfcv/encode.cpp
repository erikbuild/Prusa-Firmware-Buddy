#include <nfcv/encode.hpp>

nfcv::Encoder1Of4::Encoder1Of4(MsgBuilder &msg_builder)
    : builder(msg_builder)
    , crc() {
    builder.push_back(std::byte { 0x21 }); // 1 of 4 SOF
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
    auto crc_res = crc.get_result();
    static_assert(std::endian::native == std::endian::little);
    const std::span<std::byte> crc_res_span { reinterpret_cast<std::byte *>(&crc_res), sizeof(crc_res) };
    append_bytes_impl(crc_res_span, false);
    builder.push_back(std::byte { 0x04 }); // EOF
}

namespace nfcv::impl {
Result<void> construct_command(MsgBuilder &builder, [[maybe_unused]] const command::Inventory &command) {
    if ((builder.capacity() - builder.size()) < nfcv::Encoder1Of4::calculate_message_size(3)) {
        return std::unexpected(Error::buffer_overflow);
    }
    nfcv::Encoder1Of4 encoder(builder);
    encoder.append_byte(std::byte { 0x26 }); // Flags: 1 subcarier, high datarate, inventory flag, 1 slot
    encoder.append_byte(std::byte { 0x01 }); // Specify Inventory command
    encoder.append_byte(std::byte { 0x00 }); // No optional data
    encoder.append_crc_and_finalize();
    return {};
}

Result<void> construct_command(MsgBuilder &builder, const command::SystemInfo &command) {
    if ((builder.capacity() - builder.size()) < nfcv::Encoder1Of4::calculate_message_size(10)) {
        return std::unexpected(Error::buffer_overflow);
    }

    nfcv::Encoder1Of4 encoder(builder);
    encoder.append_byte(std::byte { 0x22 });
    encoder.append_byte(std::byte { 0x2B }); // SysInfo command
    for (const auto &byte : command.request.uid) {
        encoder.append_byte(byte);
    }
    encoder.append_crc_and_finalize();
    return {};
}

Result<void> construct_command(MsgBuilder &builder, const command::ReadSingleBlock &command) {
    if ((builder.capacity() - builder.size()) < nfcv::Encoder1Of4::calculate_message_size(11)) {
        return std::unexpected(Error::buffer_overflow);
    }

    nfcv::Encoder1Of4 encoder(builder);
    encoder.append_byte(std::byte { 0x22 });
    encoder.append_byte(std::byte { 0x20 });
    for (const auto &byte : command.request.uid) {
        encoder.append_byte(byte);
    }
    encoder.append_byte(std::byte { command.request.block_address });

    encoder.append_crc_and_finalize();
    return {};
}

Result<void> construct_command(MsgBuilder &builder, const command::WriteSingleBlock &command) {
    if ((builder.capacity() - builder.size()) < nfcv::Encoder1Of4::calculate_message_size(11 + command.request.block_buffer.size())) {
        return std::unexpected(Error::buffer_overflow);
    }

    nfcv::Encoder1Of4 encoder(builder);
    encoder.append_byte(std::byte { 0x22 });
    encoder.append_byte(std::byte { 0x21 });
    for (const auto &byte : command.request.uid) {
        encoder.append_byte(byte);
    }
    encoder.append_byte(std::byte { command.request.block_address });
    for (const auto &byte : command.request.block_buffer) {
        encoder.append_byte(byte);
    }

    encoder.append_crc_and_finalize();
    return {};
}

Result<void> construct_command(MsgBuilder &builder, const command::StayQuiet &command) {
    if ((builder.capacity() - builder.size()) < nfcv::Encoder1Of4::calculate_message_size(10)) {
        return std::unexpected(Error::buffer_overflow);
    }

    nfcv::Encoder1Of4 encoder(builder);
    encoder.append_byte(std::byte { 0x22 });
    encoder.append_byte(std::byte { 0x02 });
    for (const auto &byte : command.request.uid) {
        encoder.append_byte(byte);
    }

    encoder.append_crc_and_finalize();
    return {};
}

} // namespace nfcv::impl

nfcv::Result<void> nfcv::construct_command(MsgBuilder &builder, const Command &command) {
    return std::visit([&](const auto &cmd) { return impl::construct_command(builder, cmd); }, command);
}
