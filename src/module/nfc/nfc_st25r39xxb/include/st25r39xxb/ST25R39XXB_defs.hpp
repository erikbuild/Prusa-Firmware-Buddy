#pragma once

#include <utility>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <variant>
#include <optional>

namespace st25r39xxb {
// TODO: move all the registers into separate headers and copy paste documentation
enum class RegisterA : uint8_t {
    io_configuration_1 = 0x00,
    io_configuration_2 = 0x01,
    operation_control = 0x02,
    mode_definition = 0x03,
    bit_rate_definition = 0x04,
    ISO14443A = 0x05,
    ISO14443B_settings_1 = 0x06,
    ISO14443B_and_felica_definition = 0x07,
    passive_target = 0x08,
    stream_mode_definition = 0x09,
    auxilary_definition = 0x0A,
    receiver_configuration_1 = 0x0B,
    receiver_configuration_2 = 0x0C,
    receiver_configuration_3 = 0x0D,
    receiver_configuration_4 = 0x0E,
    receiver_timer_mask = 0x0F,
    no_response_timer_1 = 0x10,
    no_response_timer_2 = 0x11,
    timer_and_emv_control = 0x12,
    general_purpose_timer_1 = 0x13,
    general_purpose_timer_2 = 0x14,
    ppon2_field_wait = 0x15,
    mask_main_interrupt = 0x16,
    mask_timer_and_nfc_interrupt = 0x17,
    mask_error_and_wake_up_interrupt = 0x18,
    mask_passive_target_interrupt = 0x19,
    main_interrupt = 0x1A,
    timer_and_nfc_interrupt = 0x1B,
    error_and_wake_up_interrupt = 0x1C,
    passive_target_interrupt = 0x1D,
    fifo_status_1 = 0x1E,
    fifo_status_2 = 0x1F,
    collision_display = 0x20,
    passive_target_display = 0x21,
    number_of_transmitted_bytes_1 = 0x22,
    number_of_transmitted_bytes_2 = 0x23,
    bit_rate_detection_display = 0x24,
    a_d_converter_output = 0x25,
    antenna_tuning_control_1 = 0x26,
    antenna_tuning_control_2 = 0x27,
    tx_driver = 0x28,
    passive_target_modulation = 0x29,
    external_field_detector_activation_threshold = 0x2A,
    external_field_detector_deactivation_threshold = 0x2B,
    regulator_voltage_control = 0x2C,
    rssi_display = 0x2D,
    gain_reduction_state = 0x2E,
    auxilary_display = 0x31,
    wake_up_timer_control = 0x32,
    amplitude_measurement_configuration = 0x33,
    amplitude_measurement_reference = 0x34,
    amplitude_measurement_auto_averaging_display = 0x35,
    amplitude_measurement_display = 0x36,
    phase_measurement_configuration = 0x37,
    phase_measurement_reference = 0x38,
    phase_measurement_auto_averaging_display = 0x39,
    phase_measurement_display = 0x3A,
    ic_identity = 0x3F,
};

enum class RegisterB : uint8_t {
    emd_suppression_configuration = 0x05,
    subcarrier_start_timer = 0x06,
    p2p_receiver_configuration_1 = 0x0B,
    correlator_configuration_1 = 0x0C,
    correlator_configuration_2 = 0x0D,
    squelch_timer = 0x0F,
    nfc_field_on_guard_timer = 0x15,
    auxiliary_modulation_setting = 0x28,
    tx_driver_timing = 0x29,
    resistive_am_modulation = 0x2A,
    tx_driver_timing_display = 0x2B,
    regulator_display = 0x2C,
    overshoot_protection_configuration_1 = 0x30,
    overshoot_protection_configuration_2 = 0x31,
    undershoot_protection_configuration_1 = 0x32,
    undershoot_protection_configuration_2 = 0x33,
    aws_config_1 = 0x2E,
    aws_config_2 = 0x2F,
    aws_time_1 = 0x34,
    aws_time_2 = 0x35,
    aws_time_3 = 0x36,
    aws_time_4 = 0x37,
    aws_time_5 = 0x38,
    aws_time_6 = 0x39,
};

enum class Command : uint8_t {
    /// Puts the ST25R3916B into power-up state
    set_default_1 = 0xC0,
    /// Puts the ST25R3916B into power-up state
    set_default_2 = 0xC1,
    /// Stops all activities: transmission, reception, direct command execution, timers
    stop_all_1 = 0xC2,
    /// Stops all activities: transmission, reception, direct command execution, timers
    stop_all_2 = 0xC3,
    /// Starts a transmit sequence with automatic CRC generation
    transmit_with_crc = 0xC4,
    /// Starts a transmit sequence without automatic CRC generation
    transmit_without_crc = 0xC5,
    /// Transmits REQA command (ISO14443A mode only)
    transmit_reqa = 0xC6,
    /// Transmits WUPA command (ISO14443A mode only)
    transmit_wupa = 0xC7,
    /// Performs Initial RF Collision avoidance and switches on the field
    nfc_init_field_on = 0xC8,
    /// Performs Response RF Collision avoidance and switches on the field
    nfc_resp_field_on = 0xC9,
    /// Puts the passive target logic into Sense (Idle) state
    go_to_sense = 0xCD,
    /// Puts the passive target logic into Sleep (Halt) state
    go_to_sleep = 0xCE,
    /// Mask receive data Stops receivers and RX decoders
    mask_receive_data = 0xD0,
    /// Starts receivers and RX decoders
    unmask_receive_data = 0xD1,
    /// Changes AM modulation state
    change_am_modulation_state = 0xD2,
    /// Measures the amplitude of the signal present on RFI inputs and stores the result in the A/D converter output register
    measure_amplitude = 0xD3,
    /// Resets receiver gain to the value in the Receiver configuration register 4
    reset_rx_gain = 0xD5,
    /// Adjusts supply regulators according to the current supply voltage level
    adjust_regulators = 0xD6,
    /// Starts the driver timing calibration according to the setting in the TX driver timing display register
    calibrate_driver_timing = 0xD8,
    /// Measures the phase difference between the signal on RFO and RFI
    measure_phase = 0xD9,
    /// Clears the RSSI bits in the RSSI display register and restarts the measurement
    clear_rssi = 0xDA,
    /// Clears FIFO
    clear_fifo = 0xDB,
    /// Enters in Transparent mode
    enter_transparent_mode = 0xDC,
    /// Measures power supply
    measure_power_supply = 0xDF,
    start_general_purpose_timer = 0xE0,
    start_wake_up_timer = 0xE1,
    /// Starts the mask-receive timer and squelch operation
    start_mask_receive_timer = 0xE2,
    start_no_response_timer = 0xE3,
    start_ppon2_timer = 0xE4,
    stop_no_response_timer = 0xE8,
    trigger_rc_calibration = 0xEA,
    /// Enables R/W access to register Space-B
    register_space_b_access = 0xFB,
    /// Enable R/W access to Test register
    test_access = 0xFC,
};

enum class IRQType : uint32_t {
    none = 0,
    unused_0 = 1 << 0,
    /// IRQ due to automatic reception restart
    /// Set when a frame is suppressed as EMD
    rx_restart = 1 << 1,
    /// IRQ due to bit collision
    bit_collision = 1 << 2,
    /// IRQ due to end of transmission
    tx_end = 1 << 3,
    /// IRQ due to end of receive
    rx_end = 1 << 4,
    /// IRQ due to start of receive
    rx_start = 1 << 5,
    /// IRQ due to FIFO water level
    /// Set during receive, if more than 300 bytes are in the FIFO. Set during transmit, if less than 200 bytes are in the FIFO.
    fifo_water_level = 1 << 6,
    /// IRQ when oscillator frequency is stable
    /// Set after oscillator is started by setting Operation control register bit en.
    oscilator_freq_stable = 1 << 7,
    /// IRQ when in target mode the initiator bitrate was recognized
    initiator_bitrate_recognized = 1 << 8,
    /// IRQ after minimum guard time expire
    /// An external field was not detected during RF collision avoidance, field was switched on, IRQ sent after minimum guard time according to NFCIP-1.
    minimum_guard_time_expires = 1 << 9,
    /// IRQ due to detection of collision during RF Collision Avoidance
    /// Must be cleared before collision avoidance is performed.
    collision_detected = 1 << 10,
    /// IRQ due to detection of external field drop below Target activation level
    ex_field_under_target_activation_level = 1 << 11,
    /// IRQ due to detection of external field higher than Target activation level
    ex_field_over_target_activation_level = 1 << 12,
    /// IRQ due to general purpose timer expire
    general_purpose_timer_expire = 1 << 13,
    /// IRQ due to No-response timer expire
    no_response_timer_expire = 1 << 14,
    /// IRQ due to termination of direct command
    direct_command_finished = 1 << 15,
    unused_1 = 1 << 16,
    /// Wake-up interrupt due to phase measurement.
    /// Result of phase measurement ∆pm larger than reference.
    phase_measurement_wake_up = 1 << 17,
    /// Wake-up interrupt due to amplitude measurement
    /// Result of amplitude measurement ∆am larger than reference.
    amplitude_measurement_wake_up = 1 << 18,
    /// Wake-up timer interrupt
    /// Timeout after execution of Start Wake-Up Timer command in case option with IRQ at every timeout is selected.
    wake_up_timer = 1 << 19,
    /// Hard framing error
    /// Framing error that results in corrupted Rx data.
    hard_framing_error = 1 << 20,
    /// Soft framing error
    /// Framing error that does not result in corrupted Rx data.
    soft_framing_error = 1 << 21,
    /// Parity error
    parity_error = 1 << 22,
    /// CRC error
    /// Avoid delayed reading of interrupt status register to not fall into potential signaling of CRCError.
    crc_error = 1 << 23,
    /// Passive target Active interrupt
    /// Sent when Active state is reached.
    passive_target_active = 1 << 24,
    /// Passive target Active* interrupt
    /// Sent when Active* state is reached.
    passive_target_active_star = 1 << 25,
    unused_2 = 1 << 26,
    /// NFC 212/424kb/s Passive target ‘Active’ interrupt
    /// Sent after NFC 212/424 kb/s automatic response to SENSF_REQ was sent.
    nfc_212_424_passive_target_active = 1 << 27,
    /// IRQ due to end of receive, 3916 is handling the response Sent in passive target mode when
    /// NFC-A anti-collision or NFC-F SENSF_RES is automatically sent (MCU action required).
    rx_end_in_passive_target_mode = 1 << 28,
    /// IRQ due to active P2P field on event
    /// Sent after RF collision avoidance, if there was no collision and field was turned on.
    pwp_field_active = 1 << 29,
    /// IRQ for passive target slot number water level
    /// Sent if four unused slot numbers (TSN) remain in PT_memory.
    passive_target_slot_water_level = 1 << 30,
    /// PPON2 field on waiting timer interrupt
    ppon2_wait_timer = 1U << 31,
    errors = hard_framing_error | soft_framing_error | parity_error | crc_error,
    all = std::numeric_limits<uint32_t>::max(),
};

constexpr IRQType operator&(IRQType lhs, IRQType rhs) { return IRQType { std::to_underlying(lhs) & std::to_underlying(rhs) }; }
constexpr IRQType operator|(IRQType lhs, IRQType rhs) { return IRQType { std::to_underlying(lhs) | std::to_underlying(rhs) }; }
constexpr IRQType operator^(IRQType lhs, IRQType rhs) { return IRQType { std::to_underlying(lhs) ^ std::to_underlying(rhs) }; }
constexpr IRQType operator~(IRQType val) { return IRQType { ~std::to_underlying(val) }; }

enum class Impedance : uint8_t {
    ohm1_7 = 0x00,
    ohm2 = 0x01,
    ohm2_38 = 0x02,
    ohm2_74 = 0x03,
    ohm3 = 0x04,
    ohm3_43 = 0x05,
    ohm4_23 = 0x06,
    ohm5 = 0x07,
    ohm5_8 = 0x08,
    ohm6_9 = 0x09,
    ohm10_12 = 0x0A,
    ohm14 = 0x0B,
    ohm29 = 0x0C,
    ohm62_22 = 0x0D,
    ohm87_04 = 0x0E,
    high = 0x0F,
};

enum class Amplitude : uint8_t {
    percent_0 = 0x00,
    percent_8 = 0x10,
    percent_10 = 0x20,
    percent_11 = 0x30,
    percent_12 = 0x40,
    percent_13 = 0x50,
    percent_14 = 0x60,
    percent_15 = 0x70,
    percent_20 = 0x80,
    percent_25 = 0x90,
    percent_30 = 0xA0,
    percent_40 = 0xB0,
    percent_50 = 0xC0,
    percent_60 = 0xD0,
    percent_70 = 0xE0,
    percent_82 = 0xF0,
};

namespace config {
    enum class AWSTransient : uint8_t {
        slow,
        medium,
        fast,
    };

    using AWS = std::optional<AWSTransient>;

    struct ModulationBase {
        AWS aws = std::nullopt;
    };

    struct AMModulation : public ModulationBase {
        Amplitude target_amplitude = Amplitude::percent_82;
    };

    struct OOKModulation : public ModulationBase {
    };

} // namespace config
using ModulationConfiguration = std::variant<config::AMModulation, config::OOKModulation>;

namespace constant {
    constexpr size_t FIFO_SIZE = 512;
}

} // namespace st25r39xxb
