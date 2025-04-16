#include <catch2/catch.hpp>
#include <generators/random_bytes.hpp>

#include <nfcv/decode.hpp>
#include <nfcv/encode.hpp>

void test_decode(std::span<const std::byte> input, std::span<const std::byte> expected_output) {
    std::vector<std::byte> output;
    output.resize(input.size());
    std::span output_span { output.data(), output.size() };
    auto res = nfcv::decode(input, output_span);
    REQUIRE(res.has_value());
    REQUIRE(res.value().size() == expected_output.size());

    std::ranges::for_each(std::views::zip(*res, expected_output), [](const auto &val) {
        const auto &[lhs, rhs] = val;
        CAPTURE(lhs, rhs);
        CHECK(lhs == rhs);
    });
}

TEST_CASE("Test NFC-V decode Inventory response", "[nfcv][decode]") {
    static constexpr std::array<uint8_t, 12> expected_output = { 0, 0, 19, 123, 90, 4, 9, 1, 4, 224, 176, 178 };
    std::array<uint8_t, 26> input = { 0xb7, 0xaa, 0xaa, 0xaa, 0x4a, 0xcb, 0x4a, 0x53, 0x2d, 0xd3, 0xac, 0xac, 0xca, 0xb2, 0xca, 0xaa, 0xaa, 0xac, 0xaa, 0x2a, 0xb5, 0x4a, 0x33, 0x4b, 0xb3, 0x03 };
    test_decode(std::as_bytes(std::span { input }), std::as_bytes(std::span { expected_output }));
}

TEST_CASE("Test NFC-V decode SysInfo response - SLIX 2", "[nfcv][decode]") {
    static constexpr std::array<uint8_t, 17> expected_output = { 0, 15, 19, 123, 90, 4, 9, 1, 4, 224, 0, 0, 79, 3, 1, 173, 33 };
    std::array<uint8_t, 36> input = { 0xb7, 0xaa, 0x4a, 0xb5, 0x4a, 0xcb, 0x4a, 0x53, 0x2d, 0xd3, 0xac, 0xac, 0xca, 0xb2, 0xca, 0xaa, 0xaa, 0xac, 0xaa, 0x2a, 0xb5, 0xaa, 0xaa, 0xaa, 0x4a, 0xb5, 0x4c, 0xab, 0xca, 0xaa, 0xca, 0x34, 0xd3, 0x2a, 0xab, 0x03 };
    test_decode(std::as_bytes(std::span { input }), std::as_bytes(std::span { expected_output }));
}

TEST_CASE("Test NFC-V decode SysInfo response - SLIX", "[nfcv][decode]") {
    static constexpr std::array<uint8_t, 17> expected_output = { 0, 15, 110, 118, 241, 108, 80, 1, 4, 224, 0, 0, 27, 3, 1, 31, 116 };
    std::array<uint8_t, 36> input = { 0xb7, 0xaa, 0x4a, 0xb5, 0x2a, 0x35, 0x2d, 0x4d, 0xcd, 0x4a, 0xb5, 0x34, 0xad, 0xca, 0xcc, 0xaa, 0xaa, 0xac, 0xaa, 0x2a, 0xb5, 0xaa, 0xaa, 0xaa, 0x4a, 0xd3, 0x4a, 0xab, 0xca, 0xaa, 0x4a, 0xd5, 0xaa, 0x4c, 0xad, 0x03 };
    test_decode(std::as_bytes(std::span { input }), std::as_bytes(std::span { expected_output }));
}

TEST_CASE("Test NFC-V decode ReadSingleBlock response", "[nfcv][decode]") {
    static constexpr std::array<uint8_t, 7> expected_output = { 0, 16, 17, 18, 19, 164, 87 };
    std::array<uint8_t, 16> input = { 0xb7, 0xaa, 0xaa, 0xca, 0xca, 0xca, 0x2a, 0xcb, 0x4a, 0xcb, 0xaa, 0x2c, 0x53, 0xcd, 0xac, 0x03 };
    test_decode(std::as_bytes(std::span { input }), std::as_bytes(std::span { expected_output }));
}

TEST_CASE("Test NFC-V decode WriteSingleBlock response", "[nfcv][decode]") {
    static constexpr std::array<uint8_t, 3> expected_output = { 0, 120, 240 };
    std::array<uint8_t, 8> input = { 0xb7, 0xaa, 0xaa, 0x52, 0xad, 0x4a, 0xb5, 0x03 };
    test_decode(std::as_bytes(std::span { input }), std::as_bytes(std::span { expected_output }));
}

TEST_CASE("Test NFC-V decode - single buffer decoding", "[nfcv][decode]") {
    static constexpr std::array<uint8_t, 7> expected_output = { 0, 16, 17, 18, 19, 164, 87 };
    std::array<uint8_t, 16> input = { 0xb7, 0xaa, 0xaa, 0xca, 0xca, 0xca, 0x2a, 0xcb, 0x4a, 0xcb, 0xaa, 0x2c, 0x53, 0xcd, 0xac, 0x03 };

    std::span input_span { reinterpret_cast<std::byte *>(input.data()), input.size() };
    auto res = nfcv::decode(std::as_bytes(std::span { input }), input_span);
    if (res.has_value()) {
        CAPTURE(res.value());
    } else {
        CAPTURE(res.error());
    }
    REQUIRE(res.has_value());
    REQUIRE(res.value().size() == expected_output.size());

    std::ranges::for_each(std::views::zip(*res, expected_output), [](const auto &val) {
        const auto &[lhs, rhs] = val;
        CAPTURE(lhs, rhs);
        CHECK(lhs == std::byte { rhs });
    });
}
