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
            inline bool operator==(const Request &o) const = default;
        } request;
        using Response = UID;
        Response &response;
    };

    struct SystemInfo {
        static constexpr std::byte cmd_id { 0x2B };
        struct Request {
            UID uid;

            inline bool operator==(const Request &o) const = default;
        } request;
        using Response = TagInfo;
        Response &response;
    };

    struct ReadSingleBlock {
        static constexpr std::byte cmd_id { 0x20 };
        struct Request {
            UID uid;
            uint8_t block_address;

            inline bool operator==(const Request &o) const = default;
        } request;
        using Response = const std::span<std::byte>;
        Response response;
    };

    struct WriteSingleBlock {
        static constexpr std::byte cmd_id { 0x21 };
        static constexpr bool is_write_alike = true;
        struct Request {
            UID uid;
            uint8_t block_address;
            std::span<const std::byte> block_buffer;
        } request;
        struct Response {
        } response;
    };

    struct StayQuiet {
        static constexpr std::byte cmd_id { 0x02 };
        struct Request {
            UID uid;

            inline bool operator==(const Request &o) const = default;
        } request = {};
        // No repsonse expected
    };

    struct WriteAFI {
        static constexpr std::byte cmd_id { 0x27 };
        static constexpr bool is_write_alike = true;
        struct Request {
            UID uid;
            AFI afi;

            inline bool operator==(const Request &o) const = default;
        } request;
        struct Response {
        } response;
    };

    struct WriteDSFID {
        static constexpr std::byte cmd_id { 0x29 };
        static constexpr bool is_write_alike = true;
        struct Request {
            UID uid;
            DSFID dsfid;

            inline bool operator==(const Request &o) const = default;
        } request;
        struct Response {
        } response;
    };

    ///* SLIX2 extension
    struct SetEAS {
        static constexpr std::byte cmd_id { 0xA2 };
        static constexpr bool is_write_alike = true;
        struct Request {
            UID uid;

            inline bool operator==(const Request &o) const = default;
        } request;
        struct Response {
        } response;
    };

    ///* SLIX2 extension
    struct ResetEAS {
        static constexpr std::byte cmd_id { 0xA3 };
        static constexpr bool is_write_alike = true;
        struct Request {
            UID uid;

            inline bool operator==(const Request &o) const = default;
        } request;
        struct Response {
        } response;
    };

} // namespace command

using Command = std::variant<
    command::Inventory, command::SystemInfo, command::StayQuiet,
    command::ReadSingleBlock, command::WriteSingleBlock,
    command::WriteAFI, command::WriteDSFID,
    command::SetEAS, command::ResetEAS>;

/// Utility function that checks if current command expects response
bool is_response_expected(const Command &command);
/// Utility function that checks if current command is "write-like" command
bool is_write_like_command(const Command &command);
} // namespace nfcv
