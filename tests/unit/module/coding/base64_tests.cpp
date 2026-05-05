#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include <base64/base64.hpp>

std::string encode(std::string_view data) {
    base64::Base64Encoder encoder;
    std::string result;

    for (auto byte : data) {
        result += encoder.encode(static_cast<std::byte>(byte));
    }

    result += encoder.finalize();
    return result;
}

std::optional<std::string> decode(std::string_view data) {
    base64::Base64Decoder decoder;
    std::string result;

    for (const auto byte : data) {
        std::byte out;
        using Result = base64::Base64Decoder::DecodeResult;
        switch (decoder.decode(byte, out)) {

        case Result::error:
            return std::nullopt;

        case Result::new_output:
            result += static_cast<char>(out);
            break;

        case Result::no_output:
            break;
        }
    }

    if (!decoder.finalize()) {
        return std::nullopt;
    }

    return result;
}

TEST_CASE("encoded_length", "[base64]") {
    using base64::encoded_length;
    REQUIRE(encoded_length(0) == 0);
    REQUIRE(encoded_length(1) == 4);
    REQUIRE(encoded_length(2) == 4);
    REQUIRE(encoded_length(3) == 4);
    REQUIRE(encoded_length(4) == 8);
    REQUIRE(encoded_length(5) == 8);
    REQUIRE(encoded_length(6) == 8);
    REQUIRE(encoded_length(7) == 12);
    REQUIRE(encoded_length(100) == 136);
}

TEST_CASE("Base64 - RFC 4648 test vectors (encode/decode)", "[base64]") {
    auto check = [](std::string_view binary, std::string_view encoded) {
        CAPTURE(binary, encoded);
        CHECK(encode(binary) == encoded);
        CHECK(decode(encoded) == binary);
        CHECK(base64::encoded_length(binary.size()) == encoded.size());
    };

    check("", "");
    check("f", "Zg==");
    check("fo", "Zm8=");
    check("foo", "Zm9v");
    check("foob", "Zm9vYg==");
    check("fooba", "Zm9vYmE=");
    check("foobar", "Zm9vYmFy");
}

TEST_CASE("Base64Encoder - reset clears partial state", "[base64][encoder]") {
    base64::Base64Encoder encoder;
    (void)encoder.encode(std::byte { 0x42 });
    encoder.reset();

    std::string out;
    for (char c : std::string_view { "foobar" }) {
        out += encoder.encode(static_cast<std::byte>(c));
    }
    out += encoder.finalize();
    REQUIRE(out == "Zm9vYmFy");
}

TEST_CASE("Base64Encoder - reusable after finalize", "[base64][encoder]") {
    base64::Base64Encoder encoder;
    std::string out;

    out += encoder.encode(std::byte { 'f' });
    out += encoder.finalize();

    for (char c : std::string_view { "fo" }) {
        out += encoder.encode(static_cast<std::byte>(c));
    }
    out += encoder.finalize();

    REQUIRE(out == "Zg==Zm8=");
}

TEST_CASE("Base64Decoder - concatenated streams", "[base64][decoder]") {
    REQUIRE(decode("Zg==Zg==") == "ff");
    REQUIRE(decode("Zm8=Zm8=") == "fofo");
    REQUIRE(decode("Zm9vYmFyZm9vYmFy") == "foobarfoobar");
}

TEST_CASE("Base64Decoder - invalid characters return Error", "[base64][decoder]") {
    REQUIRE_FALSE(decode(" ").has_value()); // below '+'
    REQUIRE_FALSE(decode("{").has_value()); // above 'z'
    REQUIRE_FALSE(decode(",").has_value()); // inside range, not a Base64 char
}

TEST_CASE("Base64Decoder - finalize detects truncated input", "[base64][decoder]") {
    REQUIRE_FALSE(decode("Z").has_value());
    REQUIRE_FALSE(decode("Zm").has_value());
    REQUIRE_FALSE(decode("Zm9").has_value());
}

TEST_CASE("Base64Decoder - reset clears state", "[base64][decoder]") {
    base64::Base64Decoder decoder;
    std::byte out;
    (void)decoder.decode('Z', out);
    decoder.reset();
    REQUIRE(decoder.finalize());
}

TEST_CASE("Round-trip all 256 byte values", "[base64]") {
    std::string input(256, '\0');
    for (size_t i = 0; i < input.size(); ++i) {
        input[i] = char(i);
    }

    auto encoded = encode(input);
    REQUIRE(encoded.size() == base64::encoded_length(input.size()));

    auto decoded = decode(encoded);
    REQUIRE(decoded.has_value());
    REQUIRE(decoded == input);
}
