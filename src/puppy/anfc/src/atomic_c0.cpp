#include <cstdint>
#include <cmsis_gcc.h>

class CriticalSection {
    uint32_t primask;

public:
    CriticalSection()
        : primask(__get_PRIMASK()) {
        __disable_irq();
    }

    ~CriticalSection() {
        __set_PRIMASK(primask);
    }

    CriticalSection(const CriticalSection &) = delete;
    CriticalSection &operator=(const CriticalSection &) = delete;
};

namespace {

template <auto op, typename T>
inline __attribute__((always_inline)) T atomic_op(volatile void *memv, T val, [[maybe_unused]] int model) {
    auto &mem = *static_cast<volatile T *>(memv);
    CriticalSection cs;
    const T r = mem;
    op(mem, val);
    return r;
}

constexpr auto op_add = [](auto &mem, auto val) { mem += val; };
constexpr auto op_sub = [](auto &mem, auto val) { mem -= val; };
constexpr auto op_xchg = [](auto &mem, auto val) { mem = val; };

} // namespace

extern "C" __attribute__((__used__)) uint8_t __atomic_fetch_add_1(volatile void *memv, uint8_t val, int model) {
    return atomic_op<op_add>(memv, val, model);
}

extern "C" __attribute__((__used__)) uint16_t __atomic_fetch_add_2(volatile void *memv, uint16_t val, int model) {
    return atomic_op<op_add>(memv, val, model);
}

extern "C" __attribute__((__used__)) unsigned __atomic_fetch_add_4(volatile void *memv, unsigned val, [[maybe_unused]] int model) {
    return atomic_op<op_add>(memv, val, model);
}

extern "C" __attribute__((__used__)) unsigned __atomic_fetch_sub_4(volatile void *memv, unsigned val, [[maybe_unused]] int model) {
    return atomic_op<op_sub>(memv, val, model);
}

extern "C" __attribute__((__used__)) uint8_t __atomic_exchange_1(volatile void *memv, uint8_t val, [[maybe_unused]] int model) {
    return atomic_op<op_xchg>(memv, val, model);
}
