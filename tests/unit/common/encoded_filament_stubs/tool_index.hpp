#pragma once

#include <cstdint>

struct PhysicalToolIndex {
    static constexpr int count = 6;
    uint8_t val;
    auto to_raw() const { return val; }
};

struct VirtualToolIndex {
    static constexpr int count = 6;
    uint8_t val;
    auto to_raw() const { return val; }
    PhysicalToolIndex to_physical() const { return PhysicalToolIndex { val }; }
};
