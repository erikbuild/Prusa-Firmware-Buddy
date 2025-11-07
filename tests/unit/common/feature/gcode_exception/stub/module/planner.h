#pragma once

#include <cstdint>

class Planner {
public:
    // Irrelevant
    uint32_t delay_before_delivering = 0;

    void synchronize() {}
};

inline Planner planner;
