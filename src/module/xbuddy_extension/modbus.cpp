/// @file
#include <xbuddy_extension/modbus.hpp>

#include <type_traits>

static_assert(std::is_standard_layout_v<xbuddy_extension::modbus::Status>);
static_assert(std::is_standard_layout_v<xbuddy_extension::modbus::Config>);
static_assert(std::is_standard_layout_v<xbuddy_extension::modbus::Chunk>);
static_assert(std::is_standard_layout_v<xbuddy_extension::modbus::Digest>);
static_assert(std::is_standard_layout_v<xbuddy_extension::modbus::LogMessage>);

// Chunk structure is optimized to transfer as much data as possible
// in a single MODBUS transaction to improve throughput:
//  * MODBUS PDU is 253 bytes
//  * of those, 6 are used for function header
//  * this leaves 247 bytes
//  * which means 123 16-bit registers
static_assert(6 /*header*/ + sizeof(xbuddy_extension::modbus::Chunk) + 1 /*unused*/ == 253 /*pdu*/);

// TODO invent some better trait for this
static_assert(6 /*header*/ + sizeof(xbuddy_extension::modbus::Digest) < 253 /*pdu*/);
static_assert(6 /*header*/ + sizeof(xbuddy_extension::modbus::LogMessage) < 253 /*pdu*/);

xbuddy_extension::FileId xbuddy_extension::modbus::parse_file_id(uint16_t file_id) {
    switch (const auto value = static_cast<FileId>(file_id)) {
    case FileId::none:
    case FileId::firmware_ac_controller:
        return value;
    }
    return FileId::none;
}

uint16_t xbuddy_extension::modbus::serialize_file_id(xbuddy_extension::FileId file_id) {
    return static_cast<uint16_t>(file_id);
}
