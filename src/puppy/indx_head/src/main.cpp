#pragma GCC poison printf
#include "hal.hpp"

#include <FreeRTOS.h>
#include <task.h>

#include <cstddef>

// This magical incantation is required for fw_descriptor integration in cmake to work.
[[maybe_unused]] __attribute__((section(".fw_descriptor"), used)) const std::byte fw_descriptor[48] {};

extern "C" int main() {
    hal::init();
}

extern "C" void vApplicationStackOverflowHook([[maybe_unused]] TaskHandle_t xTask, [[maybe_unused]] char *pcTaskName) {
    hal::panic(indx_head::errors::FaultStatusMask::stack_overflow);
}

extern "C"
    [[noreturn, gnu::format(__printf__, 1, 4)]] void
    _bsod([[maybe_unused]] const char *fmt, [[maybe_unused]] const char *file_name, [[maybe_unused]] int line_number, ...) {
    hal::panic(indx_head::errors::FaultStatusMask::assert_failed);
}
