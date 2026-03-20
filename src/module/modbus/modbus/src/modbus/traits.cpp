/// @file
#include <modbus/traits.hpp>

#include <array>

namespace {

struct ValidBasic {
    static constexpr uint16_t address = 0x1000;
    uint16_t value;
};
static_assert(modbus::RegisterFile<ValidBasic>);

struct ValidMultipleRegisters {
    static constexpr uint16_t address = 0x2000;
    uint16_t reg1;
    uint16_t reg2;
    uint16_t reg3;
};
static_assert(modbus::RegisterFile<ValidMultipleRegisters>);

struct ValidWithArray {
    static constexpr uint16_t address = 0x3000;
    uint16_t data[10];
};
static_assert(modbus::RegisterFile<ValidWithArray>);

struct ValidMaxSize {
    static constexpr uint16_t address = 0x4000;
    uint16_t data[modbus::max_register_file_size_bytes / sizeof(uint16_t)];
};
static_assert(modbus::RegisterFile<ValidMaxSize>);
static_assert(sizeof(ValidMaxSize) == modbus::max_register_file_size_bytes);

struct InvalidNoAddress {
    uint16_t value;
};
static_assert(!modbus::RegisterFile<InvalidNoAddress>);

struct InvalidWrongAddressType {
    static constexpr const char *address = "wrong";
    uint16_t value;
};
static_assert(!modbus::RegisterFile<InvalidWrongAddressType>);

struct InvalidOddSize {
    static constexpr uint16_t address = 0x5000;
    uint8_t data[3];
};
static_assert(!modbus::RegisterFile<InvalidOddSize>);

struct InvalidTooLarge {
    static constexpr uint16_t address = 0x6000;
    uint16_t data[(modbus::max_register_file_size_bytes / sizeof(uint16_t)) + 1];
};
static_assert(!modbus::RegisterFile<InvalidTooLarge>);

struct InvalidNonStandardLayout {
    static constexpr uint16_t address = 0x7000;
    uint16_t value;
    virtual ~InvalidNonStandardLayout() = default;
};
static_assert(!modbus::RegisterFile<InvalidNonStandardLayout>);

struct InvalidWrongAlignment {
    static constexpr uint16_t address = 0x8000;
    uint32_t value;
};
static_assert(!modbus::RegisterFile<InvalidWrongAlignment>);

struct InvalidEmpty {
    static constexpr uint16_t address = 0x9000;
};
static_assert(!modbus::RegisterFile<InvalidEmpty>);

// RegisterFileWithPayload tests

struct ValidPayload {
    static constexpr uint16_t address = 0xA000;
    uint16_t size;
    std::array<uint16_t, 10> data;
};
static_assert(modbus::RegisterFileWithPayload<ValidPayload>);

struct PayloadMissingSize {
    static constexpr uint16_t address = 0xA100;
    std::array<uint16_t, 10> data;
};
static_assert(!modbus::RegisterFileWithPayload<PayloadMissingSize>);

struct PayloadMissingData {
    static constexpr uint16_t address = 0xA200;
    uint16_t size;
};
static_assert(!modbus::RegisterFileWithPayload<PayloadMissingData>);

struct PayloadNonSpannableData {
    static constexpr uint16_t address = 0xA300;
    uint16_t size;
    uint16_t data; // not an array
};
static_assert(!modbus::RegisterFileWithPayload<PayloadNonSpannableData>);

} // namespace
