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
