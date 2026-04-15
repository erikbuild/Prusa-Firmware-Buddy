#pragma once

#include <cstdint>

#include <cmsis_gcc.h>

class CriticalSection {
    uint32_t primask;

public:
    [[gnu::always_inline]] inline CriticalSection()
        : primask(__get_PRIMASK()) {
        __disable_irq();
    }

    [[gnu::always_inline]] inline ~CriticalSection() {
        __set_PRIMASK(primask);
    }

    CriticalSection(const CriticalSection &) = delete;
    CriticalSection &operator=(const CriticalSection &) = delete;
};
