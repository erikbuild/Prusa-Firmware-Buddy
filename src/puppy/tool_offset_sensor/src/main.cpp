#define DO_NOT_CHECK_ATOMIC_LOCK_FREE
#include "hal.hpp"

#include <cyphal_tool_offset_sensor_node.hpp>

#include <FreeRTOS.h>
#include <task.h>
#include <freertos/timing.hpp>
#include <device/peripherals.h>
#include <can_driver_fdcan.hpp>
#include <device/hal.h>
#include <o1heap/o1heap.hpp>
#include <cstring>
#include <ldc1612.hpp>

LOG_COMPONENT_REF(LDC1612)

// This magical incantation is required for fw_descriptor integration in cmake to work.
[[maybe_unused]] __attribute__((section(".fw_descriptor"), used)) const std::byte fw_descriptor[48] {};

static void main_task_code(void *) {

    while (true) { // repeat initialization until successful
        if (hal::ldc1612.is_device_present()) {
            log_debug(LDC1612, "Device detected successfully");
        } else {
            log_error(LDC1612, "Device not detected");
            freertos::delay(1000);
            continue;
        }

        if (hal::ldc1612.reset()) {
            log_debug(LDC1612, "Device reset successfully");
        } else {
            log_error(LDC1612, "Failed to reset device");
            freertos::delay(1000);
            continue;
        }

        LDC1612::ChannelConfig ch_config {
            .rcount = 4096,
            .settlecount = 64,
            .fin_divider = 1,
            .fref_divider = 1,
            .drive_current = 30,
            .offset = 0
        };

        LDC1612::DeviceConfig device_config {
            .sleep_mode = false,
            .use_external_clock = true,
            .rp_override_en = false,
            .auto_amp_dis = false,
            .mux_config = {
                .deglitch = LDC1612::DeglitchFilter::MHz_10 },
            .error_config = {
                .report_underrange = true,
                .report_overrange = true,
                .report_watchdog = true,
                .report_amplitude_high = false,
                .report_amplitude_low = false,
                .int_on_underrange = false,
                .int_on_overrange = false,
                .int_on_watchdog = false,
                .int_on_amplitude_high = false,
                .int_on_amplitude_low = false,
                .int_on_zero_count = false,
                .int_on_data_ready = true,
            },
            .ch0 = ch_config,
            .ch1 = ch_config
        };

        if (hal::ldc1612.initialize(device_config)) {
            log_debug(LDC1612, "Device initialized successfully");
        } else {
            log_error(LDC1612, "Failed to initialize device");
            freertos::delay(1000);
            continue;
        }

        if (hal::ldc1612.set_dual_channel_mode()) {
            log_debug(LDC1612, "Dual channel mode set successfully");
        } else {
            log_error(LDC1612, "Failed to set dual channel mode");
            freertos::delay(1000);
            continue;
        }
        break;
    }

    while (true) {
        static uint8_t consecutive_failures = 0;

        if (consecutive_failures >= 5) {
            log_error(LDC1612, "Too many consecutive failures, resetting board");
            freertos::delay(100); // delay to propagate message before reset
            hal::reset();
        }
        freertos::delay(100);

        auto status = hal::ldc1612.read_status();
        if (!status.has_value()) {
            log_warning(LDC1612, "Failed to read status");
            consecutive_failures++;
            continue;
        } else if (status->data_ready) {
            auto ch0_data = hal::ldc1612.read_channel(LDC1612::Channel::CH0);
            auto ch1_data = hal::ldc1612.read_channel(LDC1612::Channel::CH1);
            if (ch0_data.has_value() && ch1_data.has_value()) {
                log_warning(LDC1612, "CH0: %u, CH1: %u", static_cast<unsigned int>(ch0_data.value()), static_cast<unsigned int>(ch1_data.value()));
                consecutive_failures = 0;
            } else {
                log_warning(LDC1612, "Failed to read channel data");
                consecutive_failures++;
            }
        }
    };
}

namespace {
constexpr size_t main_task_stack_size = 1024;
alignas(32) StackType_t main_task_stack[main_task_stack_size];
StaticTask_t main_task_control_block;

// CAN node app task, processes requests from the CAN. Implemented in can_node
constexpr size_t node_task_stack_size = 1024 / sizeof(StackType_t);
alignas(32) StackType_t node_task_stack[node_task_stack_size];
StaticTask_t node_task_control_block;

// High-priority task that is woken up when we need to receive/send over CAN and handles the request
constexpr size_t can_task_stack_size = 2048 / sizeof(StackType_t);
alignas(32) StackType_t can_task_stack[can_task_stack_size];
StaticTask_t can_task_control_block;

auto get_uid() {
    tool_offset_sensor::cyphal::ToolOffsetSensorNode::UID uid;
    static constexpr size_t copy_bytes = std::min<size_t>(MCU_UID_SIZE, uid.size());
    std::memcpy(uid.data(), reinterpret_cast<const void *>(UID_BASE), copy_bytes);
    std::memset(uid.data() + MCU_UID_SIZE, 0, uid.size() - copy_bytes);
    return uid;
}

can::FdcanDriver can_driver(hal::peripherals::hfdcan1, hal::enable_bit_rate_switch);

// Heap allocated for canard
O1Heap<8192> canard_heap;

void *canard_heap_allocate(CanardInstance *, size_t bytes) {
    return canard_heap.alloc(bytes);
}
void canard_heap_free(CanardInstance *, void *ptr) {
    canard_heap.free(ptr);
}
} // namespace

// Cannot be in private namespace - linked with src/can
// Also, has to be initialized before can_node
can::cyphal::Task::RxQueue<1> cyphal_task_rx_queue;
can::cyphal::Task can::cyphal::cyphal_task(can_driver, cyphal_task_rx_queue, 32, &canard_heap_allocate, &canard_heap_free);

tool_offset_sensor::cyphal::ToolOffsetSensorNode can_node(get_uid());

extern "C" int main() {
    hal::init();

    // Set up Cyphal node and communication basics
    can_node.init();

    [[maybe_unused]] TaskHandle_t can_task_handle = xTaskCreateStatic(
        can::cyphal::Task::task,
        "can_task",
        can_task_stack_size,
        &can::cyphal::cyphal_task,
        tskIDLE_PRIORITY + 2,
        can_task_stack,
        &can_task_control_block);

    [[maybe_unused]] TaskHandle_t node_task_handle = xTaskCreateStatic(
        [](void *) { can_node.task(); },
        "node_task",
        node_task_stack_size,
        nullptr,
        tskIDLE_PRIORITY + 1,
        node_task_stack,
        &node_task_control_block);

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

namespace puppy::fault {

bool trigger_fault(tool_offset_sensor::cyphal::Fault fault) {
    return can_node.set_fault(fault);
}

bool trigger_fault(puppy::fault::SharedFault fault) {
    return trigger_fault(tool_offset_sensor::cyphal::from_shared(fault));
}

} // namespace puppy::fault
