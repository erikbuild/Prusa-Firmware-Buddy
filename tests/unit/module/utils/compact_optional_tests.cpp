#include <catch2/catch_test_macros.hpp>
#include <utils/compact_optional.hpp>

#include <climits>

TEST_CASE("CompactOptional<int>") {
    using Optional = CompactOptional<int, INT_MAX>;
    Optional opt;
    CHECK(!opt.has_value());
    CHECK_THROWS(*opt);

    CHECK_THROWS(opt = INT_MAX);
    CHECK(!opt.has_value());
    CHECK(opt != 3);

    opt = 3;
    CHECK(opt.has_value());
    CHECK(opt == 3);
    CHECK(opt == Optional(3));
    CHECK(opt != Optional(std::nullopt));
    CHECK(opt == opt);

    opt = std::nullopt;
    CHECK(!opt.has_value());
    CHECK(opt == std::nullopt);
    CHECK(opt != 3);

    Optional opt2 { INT_MIN };
    CHECK(opt != opt2);
    CHECK(opt2.has_value());
    CHECK(*opt2 == INT_MIN);
}

TEST_CASE("CompactOptional<float>") {
    using Optional = CompactOptional<float, NAN>;
    Optional opt;
    CHECK(!opt.has_value());
    CHECK_THROWS(*opt);

    CHECK_THROWS(opt = NAN);
    CHECK(!opt.has_value());
    CHECK(opt != 3);

    opt = 3;
    CHECK(opt.has_value());
    CHECK(opt == 3);
    CHECK(opt == Optional(3));
    CHECK(opt != Optional(std::nullopt));
    CHECK(opt == opt);

    opt = std::nullopt;
    CHECK(!opt.has_value());
    CHECK_THROWS(*opt);
    CHECK(opt == std::nullopt);
    CHECK(opt != 3);

    Optional opt2 { INFINITY };
    CHECK(opt != opt2);
    CHECK(opt2.has_value());
    CHECK(*opt2 == INFINITY);
    CHECK(opt2 != NAN);
}
