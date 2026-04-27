#pragma once

#include <limits>
#include <array>
#include <atomic>
#include <memory>
#include <optional>

#include <indx_head/modbus.hpp>

#include <otp/types.hpp>
#include "puppies/PuppyModbus.hpp"
#include <fifo_coder/fifo_decoder.hpp>
#include "puppies/time_sync.hpp"
#include <include/dwarf_registers.hpp>
#include <utils/utility_extensions.hpp>
#include <puppies/dwarf_status_led.hpp>
#include <indx_head/errors.hpp>
#include <indx_head/leds.hpp>
#include <utils/color.hpp>
#include <timing.h>

namespace freertos {
class Mutex;
}

using namespace fifo_coder;

namespace buddy::puppies {

class Indx final : public ModbusDevice, public Decoder::Callbacks {
public:
    using SystemCoil = dwarf_shared::registers::SystemCoil;
    using SystemFIFO = dwarf_shared::registers::SystemFIFO;

    static constexpr uint16_t ENCODED_FIFO_ADDRESS { std::to_underlying(SystemFIFO::encoded_stream) };

    static constexpr uint_fast8_t NUM_FANS = 2;
    static constexpr uint8_t FIFO_RETRIES = 3;

    /// when this is set as PWM, fan is switched to automatic mode
    static constexpr uint16_t FAN_MODE_AUTO_PWM = std::numeric_limits<uint16_t>::max();

public:
    Indx(uint8_t modbus_address);
    Indx(const Indx &) = delete;

    CommunicationStatus ping(PuppyModbus &);
    CommunicationStatus initial_scan(PuppyModbus &);

    /**
     * @brief Refreshes all registers from dwarf.
     * @return CommunicationStatus::OK on successful refresh and
     *   CommunicationStatus::SKIPPED on successful skip.
     */
    CommunicationStatus refresh(PuppyModbus &);

    /**
     * @brief Pulls data from dwarf fifo.
     * Originally fast_refresh().
     * @param[out] more true if there is more data to pull, false if fifo is empty
     * @return CommunicationStatus::OK on success
     */
    CommunicationStatus pull_fifo(PuppyModbus &, bool &more);

    /**
     * @brief Enable/disable loadcell data in FIFO.
     *
     * @param active Enable loadcell FIFO output
     * @return True when successful, false on communication error
     */
    bool set_loadcell(PuppyModbus &, bool active);

    /**
     * @brief Get loadcell active state
     *
     * @return True if loadcell output is enabled, false otherwise
     */
    bool get_loadcell_active();

    /**
     * @brief Enable/disable accelerometer data in FIFO.
     *
     * @param active Enable accelerometer FIFO output
     * @return True when successful, false on communication error
     */
    bool set_accelerometer(PuppyModbus &, bool active);

    /**
     * @brief Gets accelerometer active state
     *
     * @return True if accelerometer output is enabled, false otherwise
     */
    bool get_accelerometer_active();

    CommunicationStatus set_hotend_target_temp(float target);
    CommunicationStatus set_hotend_temp_compensation(float offset);
    [[nodiscard]] float get_hotend_temp_compensated() const;
    [[nodiscard]] float get_hotend_temp_uncompensated() const;

    /// In °C/s
    [[nodiscard]] float get_hotend_temp_raw_c_dt_s() const;

    [[nodiscard]] int16_t get_mcu_temperature(); ///< Get MCU temperature [°C]
    [[nodiscard]] int16_t get_board_temperature(); ///< Get board temperature [°C]
    [[nodiscard]] float get_tpis_ambient_temperature(); ///< Get TPiS sensor ambient temperature [°C]
    [[nodiscard]] float get_24V(); ///< Get 24V power supply voltage [V]
    /** Get nozzle presence (debounced on the INDX_HEAD side).
     *  @returns nullopt until the head reports a definitive value, true if nozzle is present, false otherwise
     */
    [[nodiscard]] std::optional<bool> get_nozzle_present();
    void invalidate_nozzle_data(); ///< Invalidate after pickup/park

    // Buddy-side communication error counters (since boot)
    std::atomic<uint16_t> fifo_error_count = 0;
    std::atomic<uint16_t> refresh_error_count = 0;

    void set_fan(uint8_t fan, uint16_t target);
    void set_fan_auto(uint8_t fan);
    void set_selftest_mode(bool enabled);

    /**
     * @brief Set INDX_HEAD LEDs' color.
     * @param color
     * @param mode set up led pwm mode
     */
    void set_leds_color(Color color, indx_head::leds::Mode mode);

    /**
     * @brief Power INDX_HEAD LED on/off.
     */
    void set_leds_enabled(bool set);

    /**
     * @brief Set dwarf status LED to pulse.
     * @param mode select solid, flashing or pulsing
     */
    void set_leds_mode(indx_head::leds::Mode mode);

    uint16_t get_heatbreak_fan_pwr();

    uint16_t get_fan_pwm(uint8_t fan_nr) const;
    uint16_t get_fan_rpm(uint8_t fan_nr) const;
    bool get_fan_rpm_ok(uint8_t fan_nr) const;
    uint16_t get_fan_state(uint8_t fan_nr) const;

    void set_otp(const OTP_v5 &);
    OTP_v5 get_otp() const;

private:
    OTP_v5 otp = {};

    ModbusInputRegisterBlock<indx_head::modbus::Status::address, indx_head::modbus::Status> register_general_status {};

    // Cached from RegisterGeneralStatus.ToolFilamentSensor, for use from an interrupt (where we can't lock).
    std::atomic<uint16_t> tool_filament_sensor = 0;
    /// Cached nozzle presence for use by Marlin.
    ///
    /// (encodes validity too).
    std::atomic<indx_head::NozzlePresence> cached_nozzle_state { indx_head::NozzlePresence::unknown };
    static_assert(std::atomic<indx_head::NozzlePresence>::is_always_lock_free);

    std::atomic<uint16_t> nozzle_invalidation_token { 0 }; ///< Token sent to head; data is valid only after head echoes it back nozzle_invalidation_ack from INDX_HEAD

    ModbusHoldingRegisterBlock<indx_head::modbus::Config::address, indx_head::modbus::Config> general_write;
    // Cached hotend temperature fields — populated from read_general_status(), read lock-free by Marlin.
    std::atomic<int16_t> cached_hotend_temp_compensated_c100 { indx_head::modbus::default_hotend_temperature_c100 };
    std::atomic<int16_t> cached_hotend_temp_uncompensated_c100 { indx_head::modbus::default_hotend_temperature_c100 };
    std::atomic<int16_t> cached_hotend_temp_raw_c100_dt_s { 0 };
    static_assert(std::atomic<int16_t>::is_always_lock_free);
    static_assert(std::atomic<uint16_t>::is_always_lock_free);

    // Desired values for temperature control — written lock-free by Marlin, applied in write_general().
    std::atomic<uint16_t> nozzle_target_temperature_desired { 0 };
    std::atomic<int16_t> hotend_temperature_compensation_c100_desired { 0 };

    // Because they can be set from an interrupt.
    std::array<std::atomic<uint16_t>, NUM_FANS> fan_pwm_desired { 0, 0 };
    std::atomic<bool> selftest_mode_ { false };

private:
    // FIXME: Need to be forward-declared, because this header file is included
    // from marlin and it seems virtually impossible to persuade the **** build
    // system to set the include paths to the place where we hide the
    // freertos/mutex.hpp.
    std::unique_ptr<freertos::Mutex> mutex;

    buddy::puppies::TimeSync time_sync;

    struct LoadcellSamplerate {
        static constexpr float loadcell_sample_rate = 366.f; ///< Sample rate from INDX_HEAD
        static constexpr float expected = 1000.f / loadcell_sample_rate; ///< Expected sampling interval [ms]
        uint32_t count; ///< Number of samples processed in one fifo pull
        uint32_t last_timestamp; ///< Timestamp of last sample
        uint32_t last_processed_timestamp; ///< Timestamp of last update of sampling rate
    } loadcell_samplerate;

    CommunicationStatus write_general(PuppyModbus &);
    bool dispatch_log_event();
    CommunicationStatus read_general_status(PuppyModbus &);
    void handle_fault_status();
    void handle_nozzle_presence(); ///< Update cached_nozzle_state from latest modbus data
    void handle_time_sync(const RequestTiming &);

    // Register refresh control
    uint32_t last_update_ms = 0; ///< Last time we updated registers
    uint32_t refresh_nr = 0; ///< Switch of different refresh cases
    uint32_t last_pull_ms = 0; ///< Last time we pulled data from fifo

protected:
    void decode_log(const LogData &data) final;
    void decode_loadcell(const LoadcellRecord &data) final;
    void decode_accelerometer_fast(const AccelerometerFastData &data) final;
    void decode_accelerometer_freq(const AccelerometerSamplingRate &data) final;
};

extern Indx indx;

} // namespace buddy::puppies
