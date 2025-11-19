#pragma once

#include <cstdint>

namespace st25r39xxb {

class SystemInterface {
public:
    virtual void delay(uint32_t delay_ms) = 0;
    virtual uint32_t await_interrupt(uint32_t timeout_ms) = 0;
    virtual void trigger_interrupt() = 0;
};

} // namespace st25r39xxb
