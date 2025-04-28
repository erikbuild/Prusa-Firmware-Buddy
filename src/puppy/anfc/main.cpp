#include "hal.hpp"
#include "nfc.hpp"

#include <FreeRTOS.h>
#include <task.h>
#include <freertos/timing.hpp>
#include <device/peripherals.h>
#include <cyphal_anfc_node.hpp>
#include <can_driver_fdcan.hpp>
#include <device/hal.h>
#include <nfc_task.hpp>

// This magical incantation is required for fw_descriptor integration in cmake to work.
[[maybe_unused]] __attribute__((section(".fw_descriptor"), used)) const std::byte fw_descriptor[48] {};

LOG_COMPONENT_DEF(nfc);

namespace {

// CAN node app task, processes requests from the CAN. Implemented in can_node
constexpr const size_t node_task_stack_size = 200;
alignas(32) StackType_t node_task_stack[node_task_stack_size];
StaticTask_t node_task_control_block;

// High-priority task that is woken up when we need to receive/send over CAN and handles the request
constexpr const size_t can_task_stack_size = 384;
alignas(32) StackType_t can_task_stack[can_task_stack_size];
StaticTask_t can_task_control_block;

// NFC reader task, handles lenghty blocking interactions with the NFC tags. Implemented in nfc_task
constexpr const size_t nfc_task_stack_size = 400;
alignas(32) StackType_t nfc_task_stack[nfc_task_stack_size];
StaticTask_t nfc_task_control_block;

auto get_uid() {
    anfc::cyphal::ANFCNode::UID uid;
    static constexpr size_t copy_bytes = std::min<size_t>(MCU_UID_SIZE, uid.size());
    memcpy(uid.data(), reinterpret_cast<const void *>(UID_BASE), copy_bytes);
    memset(uid.data() + MCU_UID_SIZE, 0, uid.size() - copy_bytes);
    return uid;
}

can::FdcanDriver can_driver(hal::peripherals::hfdcan1);
} // namespace

// Cannot be in private namespace - linked with src/can
// Also, has to be initialized before can_node
can::cyphal::Task can::cyphal::cyphal_task(can_driver);

anfc::cyphal::ANFCNode can_node(get_uid());

NFCTask nfc_task;

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

    [[maybe_unused]] TaskHandle_t nfc_task_handle = xTaskCreateStatic(
        [](void *) { nfc_task.task(); },
        "nfc_task",
        nfc_task_stack_size,
        nullptr,
        tskIDLE_PRIORITY + 1,
        nfc_task_stack,
        &nfc_task_control_block);

    // Start FreeRTOS scheduler and we are done.
    vTaskStartScheduler();
}
