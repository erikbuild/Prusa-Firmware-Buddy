#include <catch2/catch.hpp>
#include <iso13239/crc.hpp>
#include <generators/random_bytes.hpp>

class ReferenceCRC {
public:
    constexpr ReferenceCRC()
        : curr_value(0xffff) {}
    constexpr void add_byte(uint8_t byte) {
        curr_value ^= static_cast<uint16_t>(byte);
        for (size_t i = 0; i < 8; ++i) {
            if (curr_value & 1) {
                curr_value = (curr_value >> 1) ^ 0x8408;
            } else {
                curr_value >>= 1;
            }
        }
    }
    constexpr void add_bytes(std::span<const uint8_t> bytes) {
        for (const auto byte : bytes) {
            add_byte(byte);
        }
    }
    constexpr uint16_t get_result() {
        return ~curr_value;
    }

private:
    uint16_t curr_value;
};

TEST_CASE("ISO13239 CRC CCTTI property test", "[crc][property]") {
// For proper testing change this #if 0 to #if 1, but only test on your local machine.
// This will cause the test to run forever and should really test ours crc implementation.
#if 0
    auto data = GENERATE(random_bytes(1, 512));
#else
    auto data = GENERATE(take(1'000, random_bytes(1, 512)));
#endif

    iso13239::CRC crc;
    crc.add_bytes(data);

    ReferenceCRC ref_crc;
    ref_crc.add_bytes(data);

    REQUIRE(crc.get_result() == ref_crc.get_result());
}
