#pragma once

#include "types.hpp"

#include <cstdint>
#include <span>
#include <variant>

namespace nfcv {

namespace command {
    struct Inventory {
        static constexpr std::byte cmd_id { 0x01 };
        struct Request {
        } request;
        struct Response {
            nfcv::UIDView uid;
        } response;
    };
    struct SystemInfo {
        static constexpr std::byte cmd_id { 0x2B };
        struct Request {
            nfcv::UIDConstView uid;
        } request;
        using Response = TagInfo &;
        Response response;
    };

    struct ReadSingleBlock {
        static constexpr std::byte cmd_id { 0x20 };
        struct Request {
            nfcv::UIDConstView uid;
            uint8_t block_address;
        } request;
        struct Response {
            std::span<std::byte> block_buffer;
        } response;
    };

    struct WriteSingleBlock {
        static constexpr std::byte cmd_id { 0x21 };
        struct Request {
            nfcv::UIDConstView uid;
            uint8_t block_address;
            std::span<const std::byte> block_buffer;
        } request;
        struct Response {
        } response;
    };

    struct StayQuiet {
        static constexpr std::byte cmd_id { 0x02 };
        struct Request {
            nfcv::UIDConstView uid;
        } request;
    };

} // namespace command

using Command = std::variant<command::Inventory, command::SystemInfo, command::ReadSingleBlock, command::WriteSingleBlock, command::StayQuiet>;

/// Utility function that checks if current command expects response
bool is_response_expected(const Command &command);
/// Utility function that checks if current command is "write-like" command
bool is_write_like_command(const Command &command);
} // namespace nfcv
