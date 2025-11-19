#include <cyphal_node.hpp>

void puppy::fault::trigger_fault(SharedFault) {}

int64_t get_timestamp_us() {
    return 0;
}

#include <otp.hpp>
std::optional<board_revision_t> otp_get_board_revision() {
    return {};
}
std::optional<uint8_t> otp_get_bom_id() {
    return {};
}
uint32_t otp_get_timestamp() {
    return 0;
}
uint8_t otp_get_serial_nr(serial_nr_t &) {
    return 0;
}

#include <bsod.h>
void _bsod(const char *, const char *, int, ...) {
    std::unreachable();
}

#include <device/multi_watchdog.hpp>
device::MultiWatchdog::MultiWatchdog() {}
device::MultiWatchdog::~MultiWatchdog() {}
