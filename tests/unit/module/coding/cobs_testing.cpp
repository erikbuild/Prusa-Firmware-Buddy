#include <catch2/catch.hpp>
#include <cobs/cobs.hpp>
#include <vector>
#include <algorithm>

// Helper to create a valid COBS message of specific decoded length
std::vector<uint8_t> create_cobs_message(size_t decoded_length) {
    std::vector<uint8_t> encoded;
    size_t remaining = decoded_length;

    while (remaining > 0) {
        size_t block_size = std::min(remaining, size_t { 254 });
        encoded.push_back(static_cast<uint8_t>(block_size + 1)); // Code byte
        for (size_t i = 0; i < block_size; i++) {
            encoded.push_back(0x42); // Non-zero data
        }
        remaining -= block_size;
    }
    encoded.push_back(0x00); // Delimiter
    return encoded;
}

namespace cobs {
TEST_CASE("COBS Decoder - invalid buffers", "[cobs][decoder]") {
    constexpr size_t BUFFER_SIZE = 300;

    auto expect_error = [&](std::span<const uint8_t> data, CobsError expected_error) {
        ArrayCobsStreamDecoder<BUFFER_SIZE> decoder;
        bool error_called = false;

        decoder.add_bytes(
            data,
            [](std::span<const uint8_t>) -> void {
                FAIL("Finish callback should not be called");
            },
            [&expected_error, &decoder](CobsError error) -> void {
                REQUIRE(error == expected_error);
                decoder.reset(CobsStreamDecoder::Mode::error);
            });

        REQUIRE(decoder.get_mode() == CobsStreamDecoder::Mode::error);
    };

    SECTION("Invalidly encoded message") {
        uint8_t invalid_data[] = { 0x05, 0x11, 0x22, 0x00 }; // Overhead byte says 5, but only 2 data bytes (invalid)
        expect_error(invalid_data, CobsError::invalid_input);
    }

    SECTION("Send only delimiter (not a valid message)") {
        uint8_t invalid_data[] = { 0x00 }; // Missing code byte (invalid)
        expect_error(invalid_data, CobsError::invalid_input);
    }

    SECTION("Message longer than buffer capacity") {
        std::vector<uint8_t> overflow_data = create_cobs_message(BUFFER_SIZE + 1);
        expect_error(overflow_data, CobsError::overflow);
    }

    SECTION("Invalid 0xFF block (terminated by zero)") {
        std::vector<uint8_t> invalid_data(256);
        invalid_data[0] = 0xFF;
        std::fill(invalid_data.begin() + 1, invalid_data.begin() + 255, 0x42);
        invalid_data[255] = 0x00; // 0xFF block must be followed by code, not 0x00

        // We only need to feed the part that fits
        std::span<uint8_t> span_to_feed(invalid_data);
        expect_error(span_to_feed, CobsError::invalid_input);
    }
}

TEST_CASE("COBS Decoder - valid buffers", "[cobs][decoder]") {
    constexpr size_t BUFFER_SIZE = 256;
    std::array<uint8_t, BUFFER_SIZE> input_buffer;
    CobsStreamDecoder decoder(input_buffer);

    auto expect_decoded = [&](std::vector<uint8_t> &input, std::vector<uint8_t> &expected) {
        CobsStreamDecoder decoder(input_buffer);
        bool finished = false;

        input.push_back(0x00);

        decoder.add_bytes(
            input,
            [&expected, &finished](std::span<const uint8_t> decoded) -> void {
                REQUIRE(std::equal(decoded.begin(), decoded.end(), expected.begin(), expected.end()));
                finished = true;
            },
            [](CobsError error) -> void {
                FAIL("Error should not be reachable");
            });
        REQUIRE(finished);
    };

    SECTION("Empty message") {
        std::vector<uint8_t> encoded = { 0x01 };
        std::vector<uint8_t> expected = {}; // Empty

        expect_decoded(encoded, expected);
    }

    SECTION("Delimeter (zero) in the middle") {
        std::vector<uint8_t> encoded = { 0x03, 0xDE, 0xAD, 0x03, 0xBE, 0xEF };
        std::vector<uint8_t> expected = { 0xDE, 0xAD, 0x00, 0xBE, 0xEF };

        expect_decoded(encoded, expected);
    }

    SECTION("Delimeter (zero) at start") {
        std::vector<uint8_t> encoded = { 0x01, 0x03, 0x01, 0x02 };
        std::vector<uint8_t> expected = { 0x00, 0x01, 0x02 };

        expect_decoded(encoded, expected);
    }

    SECTION("Delimeter (zero) at end") {
        std::vector<uint8_t> encoded = { 0x03, 0x01, 0x02, 0x01 };
        std::vector<uint8_t> expected = { 0x01, 0x02, 0x00 };

        expect_decoded(encoded, expected);
    }

    SECTION("Multiple consecutive zeros") {
        std::vector<uint8_t> encoded = { 0x01, 0x01, 0x01, 0x01 };
        std::vector<uint8_t> expected = { 0x00, 0x00, 0x00 };

        expect_decoded(encoded, expected);
    }

    SECTION("Alternating zeros and data") {
        std::vector<uint8_t> encoded = { 0x02, 0x01, 0x02, 0x02, 0x02, 0x03 };
        std::vector<uint8_t> expected = { 0x01, 0x00, 0x02, 0x00, 0x03 };

        expect_decoded(encoded, expected);
    }

    SECTION("Two zeros separated by data") {
        std::vector<uint8_t> encoded = { 0x01, 0x02, 0x11, 0x01 };
        std::vector<uint8_t> expected = { 0x00, 0x11, 0x00 };

        expect_decoded(encoded, expected);
    }

    SECTION("10 non-zero bytes") {
        std::vector<uint8_t> encoded = { 0x0B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        std::vector<uint8_t> expected(10, 0xFF);

        expect_decoded(encoded, expected);
    }

    SECTION("Min/max byte pattern with zeros") {
        std::vector<uint8_t> encoded = { 0x01, 0x02, 0xFF, 0x01 };
        std::vector<uint8_t> expected = { 0x00, 0xFF, 0x00 };

        expect_decoded(encoded, expected);
    }

    SECTION("Pattern: 0x00 0x00 0xFF") {
        std::vector<uint8_t> encoded = { 0x01, 0x01, 0x02, 0xFF };
        std::vector<uint8_t> expected = { 0x00, 0x00, 0xFF };

        expect_decoded(encoded, expected);
    }

    /*
        Following rule is commonly enforced in COBS decoders:
        * A 0x00 after 0xFF is invalid.
        * A 0x00 at the start is invalid unless it’s a delimiter.

        You shouldn’t see 0x00 where a code byte is expected (except as a delimiter between packets).

        Python "cobs" lib does not enforce this...
    */
    SECTION("254 non-zero bytes") {
        // Encoded: { 0xFF, [254 bytes], 0x01}
        std::vector<uint8_t> expected(254, 0x42);
        std::vector<uint8_t> encoded;
        encoded.push_back(0xFF);
        encoded.insert(encoded.end(), expected.begin(), expected.end());
        encoded.push_back(0x01);

        expect_decoded(encoded, expected);
    }

    SECTION("255 non-zero bytes") {
        // Encoded: { 0xFF, [254 bytes], 0x02, 0x42}
        std::vector<uint8_t> expected(255, 0x42);
        std::vector<uint8_t> encoded;
        encoded.push_back(0xFF);
        encoded.insert(encoded.end(), expected.begin(), expected.begin() + 254);
        encoded.push_back(0x02);
        encoded.push_back(0x42);

        expect_decoded(encoded, expected);
    }
}

TEST_CASE("COBS Decoder - Streaming (Partial Messages)", "[cobs][decoder]") {
    ArrayCobsStreamDecoder<64> decoder;
    bool finished = false;

    std::vector<uint8_t> expected = { 0xDE, 0xAD, 0x00, 0xBE, 0xEF };

    auto finish_cb = [&](std::span<const uint8_t> decoded) -> void {
        REQUIRE(std::equal(decoded.begin(), decoded.end(), expected.begin(), expected.end()));
        finished = true;
    };

    auto error_cb = [](CobsError error) -> void {
        FAIL("Error should not be reachable");
    };

    SECTION("Message split in two") {
        // Encoded: { 0x03, 0xDE, 0xAD, 0x03, 0xBE, 0xEF, 0x00 }
        std::vector<uint8_t> chunk1 = { 0x03, 0xDE, 0xAD };
        std::vector<uint8_t> chunk2 = { 0x03, 0xBE, 0xEF, 0x00 };

        decoder.add_bytes(chunk1, finish_cb, error_cb);
        REQUIRE_FALSE(finished); // Message is not complete yet

        decoder.add_bytes(chunk2, finish_cb, error_cb);
        REQUIRE(finished); // Now it's complete
    }

    SECTION("Message fed byte-by-byte") {
        std::vector<uint8_t> encoded_message = { 0x03, 0xDE, 0xAD, 0x03, 0xBE, 0xEF, 0x00 };

        for (size_t i = 0; i < encoded_message.size() - 1; ++i) {
            std::array<uint8_t, 1> byte_chunk = { encoded_message[i] };
            decoder.add_bytes(byte_chunk, finish_cb, error_cb);
            REQUIRE_FALSE(finished); // Not complete yet
        }

        // Feed the final delimiter
        std::array<uint8_t, 1> final_byte = { encoded_message.back() };
        decoder.add_bytes(final_byte, finish_cb, error_cb);
        REQUIRE(finished); // Complete
    }
}

TEST_CASE("COBS Decoder - Reset", "[cobs][decoder]") {
    ArrayCobsStreamDecoder<64> decoder;
    bool finished = false;

    std::vector<uint8_t> expected = { 0xAA, 0xBB }; // The second message

    auto finish_cb = [&](std::span<const uint8_t> decoded) -> void {
        REQUIRE(std::equal(decoded.begin(), decoded.end(), expected.begin(), expected.end()));
        finished = true;
    };
    auto error_cb = [](CobsError error) -> void {
        FAIL("Error should not be reachable");
    };

    // 1. Feed a partial (invalid) message
    std::vector<uint8_t> partial_msg = { 0x03, 0xDE, 0xAD };
    decoder.add_bytes(partial_msg, finish_cb, error_cb);
    REQUIRE_FALSE(finished);

    // 2. Reset the decoder
    decoder.reset();
    REQUIRE_FALSE(finished);

    // 3. Feed a new, complete message
    std::vector<uint8_t> new_msg = { 0x03, 0xAA, 0xBB, 0x00 };
    decoder.add_bytes(new_msg, finish_cb, error_cb);

    // 4. Check that only the *second* message was decoded
    REQUIRE(finished);
}

TEST_CASE("COBS Decoder -Two messages in a buffer", "[cobs][decoder]") {
    constexpr size_t BUFFER_SIZE = 20;
    std::array<uint8_t, BUFFER_SIZE> input_buffer;

    auto expect_multiple_same_decoded = [&](std::vector<uint8_t> &input, std::vector<uint8_t> &expected, uint8_t cnt) {
        CobsStreamDecoder decoder(input_buffer);
        uint8_t finished_cnt = 0;

        decoder.add_bytes(
            input,
            [&finished_cnt, &expected](std::span<const uint8_t> decoded) -> void {
                REQUIRE(std::equal(decoded.begin(), decoded.end(), expected.begin(), expected.end()));
                finished_cnt += 1;
            },
            [](CobsError error) -> void {
                FAIL("Error should not be reachable");
            });
        REQUIRE(finished_cnt == cnt);
    };

    SECTION("Multiple same messages in one buffer") {
        std::vector<uint8_t> multiple_messages_encoded = { 0x01, 0x01, 0x02, 0xFF, 0x00, 0x01, 0x01, 0x02, 0xFF, 0x00, 0x01, 0x01, 0x02, 0xFF, 0x00, 0x01, 0x01, 0x02, 0xFF, 0x00 };
        std::vector<uint8_t> expected = { 0x00, 0x00, 0xFF };

        expect_multiple_same_decoded(multiple_messages_encoded, expected, 4);
    }
}
} // namespace cobs
