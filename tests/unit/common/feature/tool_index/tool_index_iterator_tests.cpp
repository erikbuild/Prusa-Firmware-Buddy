#include <catch2/catch_test_macros.hpp>

#include <vector>
#include <set>

#include <tool_index.hpp>

using Items = std::vector<uint8_t>;

using VirtualExtension = VirtualToolIndexExtension;
using PhysicalExtension = PhysicalToolIndexExtension;
using GcodeExtension = GcodeToolIndexExtension;

std::set<uint8_t> enabled_physical_tools;
std::set<uint8_t> enabled_virtual_tools;

template <>
bool PhysicalToolIndex::is_enabled() const {
    const auto &self = static_cast<const PhysicalToolIndex &>(*this);
    return enabled_physical_tools.contains(self.to_raw());
}

template <>
bool VirtualToolIndex::is_enabled() const {
    const auto &self = static_cast<const VirtualToolIndex &>(*this);
    return enabled_virtual_tools.contains(self.to_raw());
}

Items test_iterator(auto iterator) {
    Items result;
    for (auto tool : iterator) {
        result.push_back(tool.to_raw());
    }
    return result;
}

TEST_CASE("ToolIndexIterator::physical") {
    using Index = PhysicalToolIndex;
    using Iterator = Index::Iterator;

    enabled_physical_tools = { 1, 3 };

    CHECK(test_iterator(Iterator::make_empty()) == Items {});
    CHECK(test_iterator(Iterator::make_all()) == Items { 0, 1, 2, 3, 4 });
    CHECK(test_iterator(Iterator::make_all().skip_all_disabled()) == Items { 1, 3 });

    CHECK(test_iterator(Iterator::make_single(Index::from_raw(0))) == Items { 0 });
    CHECK(test_iterator(Iterator::make_single(Index::from_raw(0)).skip_all_disabled()) == Items {});

    CHECK(test_iterator(Iterator::make_single(Index::from_raw(Index::count - 1))) == Items { 4 });
    CHECK(test_iterator(Iterator::make_single(Index::from_raw(Index::count - 1)).skip_all_disabled()) == Items {});
    CHECK(test_iterator(Iterator::make_single(Index::from_raw(1)).skip_all_disabled()) == Items { 1 });
    CHECK(test_iterator(Iterator::make_single(Index::from_raw(3)).skip_all_disabled()) == Items { 3 });

    CHECK(test_iterator(Iterator::make_at(Index::from_raw(0))) == Items { 0, 1, 2, 3, 4 });
    CHECK(test_iterator(Iterator::make_at(Index::from_raw(0)).skip_all_disabled()) == Items { 1, 3 });
    CHECK(test_iterator(Iterator::make_at(Index::from_raw(2)).skip_all_disabled()) == Items { 3 });
    CHECK(test_iterator(Iterator::make_at(Index::from_raw(3)).skip_all_disabled()) == Items { 3 });
    CHECK(test_iterator(Iterator::make_at(Index::from_raw(Index::count - 1)).skip_all_disabled()) == Items {});

    CHECK_THROWS(Index::from_raw(5));
}

TEST_CASE("ToolIndexIterator::virtual") {
    using Index = VirtualToolIndex;
    using Iterator = Index::Iterator;

    enabled_virtual_tools = { 0, VirtualToolIndex::count - 1 };

    CHECK(test_iterator(Iterator::make_empty()) == Items {});
    CHECK(test_iterator(Iterator::make_all()) == Items { 0, 1, 2, 3 });
    CHECK(test_iterator(Iterator::make_all().skip_all_disabled()) == Items { 0, 3 });
    CHECK(test_iterator(Iterator::make_empty().skip_all_disabled()) == Items {});

    CHECK(test_iterator(Iterator::make_single(Index::from_raw(0))) == Items { 0 });
    CHECK(test_iterator(Iterator::make_single(Index::from_raw(0)).skip_all_disabled()) == Items { 0 });
    CHECK(test_iterator(Iterator::make_single(Index::from_raw(1)).skip_all_disabled()) == Items {});

    CHECK(test_iterator(Iterator::make_at(Index::from_raw(0))) == Items { 0, 1, 2, 3 });
    CHECK(test_iterator(Iterator::make_at(Index::from_raw(0)).skip_all_disabled()) == Items { 0, 3 });
    CHECK(test_iterator(Iterator::make_at(Index::from_raw(1)).skip_all_disabled()) == Items { 3 });
    CHECK(test_iterator(Iterator::make_at(Index::from_raw(Index::count - 1)).skip_all_disabled()) == Items { 3 });

    CHECK_THROWS(Index::from_raw(4));
}

TEST_CASE("ToolIndexIterator::variant") {
    using Index = VirtualToolIndex;

    enabled_virtual_tools = { 0 };

    std::variant<Index, NoTool, AllTools> variant = NoTool {};
    CHECK(test_iterator(tool_index_iterator(variant)) == Items {});

    variant = AllTools {};
    CHECK(test_iterator(tool_index_iterator(variant)) == Items { 0, 1, 2, 3 });
    CHECK(test_iterator(tool_index_iterator(variant).skip_all_disabled()) == Items { 0 });

    variant = VirtualToolIndex::from_raw(0);
    CHECK(test_iterator(tool_index_iterator(variant)) == Items { 0 });
    CHECK(test_iterator(tool_index_iterator(variant).skip_all_disabled()) == Items { 0 });

    variant = VirtualToolIndex::from_raw(3);
    CHECK(test_iterator(tool_index_iterator(variant)) == Items { 3 });
    CHECK(test_iterator(tool_index_iterator(variant).skip_all_disabled()) == Items {});
}
