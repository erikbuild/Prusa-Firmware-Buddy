#pragma once

#include <cstdint>
#include <expected>
#include <inplace_vector.hpp>
#include <nfcv/commands.hpp>

#include "ST25R39XXB_defs.hpp"
#include "hw_interface.hpp"
#include "system_interface.hpp"

namespace st25r39xxb {

enum class Error : uint8_t {
    timeout,
    invalid_chip,
    bad_oscilator,
    buffer_overflow,
    invalid_crc,
    device_hard_framing_error,
    device_soft_framing_error,
    device_parity_error,
    device_crc_error,
    no_response,
    response_invalid_size,
    response_format_invalid,
    response_is_error,
    unknown,
};

template <typename T>
using Result = std::expected<T, Error>;

class ST25R39XXB {
public:
    enum class Antenna : uint8_t {
        antenna_1,
        antenna_2,
    };

    ST25R39XXB(HWInterface &hw_int, SystemInterface &sys_int)
        : hw_int(hw_int)
        , sys_int(sys_int)
        , current_antenna(ST25R39XXB::Antenna::antenna_2)
        , buffer() {}

    /// Generic initialization function. Should be always called
    [[nodiscard]] Result<void> init();
    /// NFC-V Poller initialization
    /// Demo function to test that everything works.
    void nfcv_init_poller();
    // TODO: this function is currently too exhasutive, make something more lightweight/async
    [[nodiscard]] Result<void> nfcv_tick_poller();
    /// Sets one of the antennas as output (the other will still be able to receive data)
    void select_antenna(Antenna target_antena);

private:
    HWInterface &hw_int;
    SystemInterface &sys_int;
    Antenna current_antenna = ST25R39XXB::Antenna::antenna_2;
    stdext::inplace_vector<std::byte, constant::FIFO_SIZE> buffer;

    [[nodiscard]] uint16_t get_fifo_len();
    /**
     * @brief Masks which interupt should ST25R39XXb send
     *
     * @attention The @p mask denotes which interrupts should be masked.
     * So if you want to enable an interrupt, then you will need to set the bit 0.
     * (So all 0xffffffff will disable all the interrupts).
     *
     * @param mask Mask of interrupts to "mask" (don't use)
     */
    void set_interrupt_mask(st25r39xxb::IRQType mask);
    [[nodiscard]] st25r39xxb::IRQType read_interrupt();
    [[nodiscard]] Result<void> await_interrupt(st25r39xxb::IRQType wait_for, uint32_t timeout);

    [[nodiscard]] Result<void> turn_on_oscilator();
    void set_output_impedance(st25r39xxb::Impedance target_impedance);
    void set_output_amplitude(st25r39xxb::Amplitude target_amplitude);
    void switch_antennas();

    [[nodiscard]] Result<void> nfcv_field_up();
    void nfcv_field_down();

    /**
     * Sends NFC-V command over ST25R39XXB.
     *
     * It returns true if command was successfull, otherwise returns false
     */
    [[nodiscard]] Result<void> nfcv_command(nfcv::Command &command);
};

} // namespace st25r39xxb
