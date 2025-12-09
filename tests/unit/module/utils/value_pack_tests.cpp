#include <catch2/catch_test_macros.hpp>
#include <utils/meta/value_pack.hpp>

TEST_CASE("ValuePack", "[buddy_utils]") {
    CHECK((ValuePack<1, 2> {}.append<1>() != ValuePack<1, 2> {}));
    CHECK((ValuePack<1, 2> {}.append<1>() == ValuePack<1, 2, 1> {}));

    CHECK((ValuePack<1, 2> {}.concatenate(ValuePack<4, 3> {}).append<9, 10>() == ValuePack<1, 2, 4, 3, 9, 10> {}));
    CHECK(ValuePack<1, 2>::Concatenate<ValuePack<4, 3>>::Append<1, 2>::equals<ValuePack<1, 2, 4, 3, 1, 2>>());

    CHECK((ValuePack<1, 2, ValuePack<1, 2, ValuePack<5, 6> {}> {}, 3, 4> {}.flatten() == ValuePack<1, 2, 1, 2, 5, 6, 3, 4> {}));

    CHECK((ValuePack<1, 2, 1, 2, 5, 6, ValuePack<1, 2> {}, 3, 4, ValuePack<1, 2> {}> {}.unique() == ValuePack<1, 2, 5, 6, ValuePack<1, 2> {}, 3, 4> {}));

    CHECK((ValuePack<ValuePack<1> {}> {}.flatten() == ValuePack<1> {}));
}
