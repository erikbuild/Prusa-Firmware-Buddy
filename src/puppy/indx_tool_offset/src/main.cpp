#include "hal.hpp"

#include <FreeRTOS.h>
#include <task.h>
#include <freertos/timing.hpp>
#include <device/peripherals.h>
#include <can_driver_fdcan.hpp>

#include <device/hal.h>

// This magical incantation is required for fw_descriptor integration in cmake to work.
[[maybe_unused]] __attribute__((section(".fw_descriptor"), used)) const std::byte fw_descriptor[48] {};

static void main_task_code(void *) {
    while (true) {
        freertos::delay(1000);
        hal::set_status_led(true);
        freertos::delay(1000);
        hal::set_status_led(false);
    };
}

constexpr const size_t main_task_stack_size = 512;
alignas(32) StackType_t main_task_stack[main_task_stack_size];
StaticTask_t main_task_control_block;

extern "C" int main() {
    hal::init();

    [[maybe_unused]] TaskHandle_t main_task_handle = xTaskCreateStatic(
        main_task_code,
        "main_task",
        main_task_stack_size,
        nullptr,
        tskIDLE_PRIORITY + 1,
        main_task_stack,
        &main_task_control_block);

    // Start FreeRTOS scheduler and we are done.
    vTaskStartScheduler();
}

extern "C" void vApplicationStackOverflowHook([[maybe_unused]] TaskHandle_t xTask, [[maybe_unused]] char *pcTaskName) {
    std::abort();
}

extern "C" void vApplicationTickHook(void) {
    HAL_IncTick();
}

[[noreturn]] void __attribute__((noreturn, format(__printf__, 1, 4)))
_bsod(const char *fmt, const char *file_name, int line_number, ...) {
    (void)fmt, (void)file_name, (void)line_number;
    hal::panic();
}
