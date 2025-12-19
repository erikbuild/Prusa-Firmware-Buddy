#pragma once

#include <cstdint>

struct PhysicalToolIndex {

    static constexpr int count = 1;

    uint8_t val;

    static auto to_raw_static(PhysicalToolIndex i) {
        return i.val;
    }

    auto to_raw() const {
        return val;
    }
};

struct VirtualToolIndex {

    static constexpr int count = 1;

    uint8_t val;

    static auto to_raw_static(VirtualToolIndex i) {
        return i.val;
    }

    PhysicalToolIndex to_physical() const {
        return PhysicalToolIndex { val };
    }

    auto to_raw() const {
        return val;
    }
};
