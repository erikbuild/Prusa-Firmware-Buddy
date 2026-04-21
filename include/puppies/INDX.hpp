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

    static constexpr uint32_t DWARF_READ_PERIOD = 200; ///< Read registers this often [ms]
    static constexpr uint32_t DWARF_FIFO_PULL_PERIOD = 200; ///< Pull fifo of unselected dwarf this often [ms]
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
     * @brief Pulls data from dwarf fifo, but is timed for non-selected dwarf.
     * @param cycle_ticks_ms ticks_ms() valid through current poll cycle [ms]
     * @param[out] worked true if fifo was pulled, false if not yet
     * @return CommunicationStatus::OK on success and
     *   CommunicationStatus::SKIPPED on successful skip.
     */
    CommunicationStatus fifo_refresh(PuppyModbus &, uint32_t cycle_ticks_ms);

    /**
     * @brief Pulls data from dwarf fifo.
     * Originally fast_refresh().
     * @param[out] more true if there is more data to pull, false if fifo is empty
     * @return CommunicationStatus::OK on success
     */
    CommunicationStatus pull_fifo(PuppyModbus &, bool &more);

    /**
     * @brief Set loadcell
     *
     * Can be enabled only on selected dwarf.
     * Automatically disable accelerometer on loadcell activation.
     *
     * @param active Loadcell state bool
     * @return True when successful, false otherwise (either communication error or Dwarf not selected)
     */
    bool set_loadcell(PuppyModbus &, bool active);

    /**
     * @brief Enable/disable accelerometer data in FIFO.
     *
     * The head keeps sampling the accelerometer continuously,
     * but only puts data into the modbus FIFO when enabled.
     *
     * @param active Enable accelerometer FIFO output
     * @return True when successful, false on communication error
     */
    bool set_accelerometer(PuppyModbus &, bool active);

    CommunicationStatus set_hotend_target_temp(float target);
    float get_hotend_temp();

    [[nodiscard]] int16_t get_mcu_temperature(); ///< Get MCU temperature [°C]
    [[nodiscard]] int16_t get_board_temperature(); ///< Get board temperature [°C]
    [[nodiscard]] float get_24V(); ///< Get 24V power supply voltage [V]
    /** Get nozzle presence (debounced on the INDX_HEAD side).
     *  @returns nullopt until the head reports a definitive value, true if nozzle is present, false otherwise
     */
    [[nodiscard]] std::optional<bool> get_nozzle_present();
    void invalidate_nozzle_data(); ///< Invalidate after pickup/park

    // Diagnostics counters (for testing)
    [[nodiscard]] uint16_t get_diag_uart_errors();

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

    uint16_t nozzle_invalidation_token = 0; ///< Token sent to head; data is valid only after head echoes it back nozzle_invalidation_ack from INDX_HEAD

    MODBUS_REGISTER TimeSync_t {
        uint32_t dwarf_time_us {};
    };
    ModbusInputRegisterBlock<indx_head::modbus::Status::time_sync_address(), TimeSync_t> TimeSync {};

    ModbusHoldingRegisterBlock<indx_head::modbus::Config::address, indx_head::modbus::Config> general_write;
    // Because they can be set from an interrupt.
    std::array<std::atomic<uint16_t>, NUM_FANS> fan_pwm_desired { 0, 0 };
    std::atomic<bool> selftest_mode_ { false };

    MODBUS_REGISTER LoadcellEnabled {
        uint16_t loadcell_enabled = 0;
    };
    ModbusHoldingRegisterBlock<indx_head::modbus::Config::loadcell_enabled_address(), LoadcellEnabled> loadcell_enabled {};

    MODBUS_REGISTER AccelerometerEnabled {
        uint16_t accelerometer_enabled = 0;
    };
    ModbusHoldingRegisterBlock<indx_head::modbus::Config::accelerometer_enabled_address(), AccelerometerEnabled> accelerometer_enabled {};

private:
    // FIXME: Need to be forward-declared, because this header file is included
    // from marlin and it seems virtually impossible to persuade the **** build
    // system to set the include paths to the place where we hide the
    // freertos/mutex.hpp.
    std::unique_ptr<freertos::Mutex> mutex;

    // Log transfer buffer and position
    std::array<char, 256> log_line_buffer;
    size_t log_line_pos = 0;
    buddy::puppies::TimeSync time_sync;

    struct LoadcellSamplerate {
        static constexpr float loadcell_sample_rate = 366.f; ///< Sample rate from INDX_HEAD
        static constexpr float expected = 1000.f / loadcell_sample_rate; ///< Expected sampling interval [ms]
        uint32_t count; ///< Number of samples processed in one fifo pull
        uint32_t last_timestamp; ///< Timestamp of last sample
        uint32_t last_processed_timestamp; ///< Timestamp of last update of sampling rate
    } loadcell_samplerate;

    CommunicationStatus write_general(PuppyModbus &);
    CommunicationStatus pull_fifo_nolock(PuppyModbus &, bool &more);
    bool dispatch_log_event();
    CommunicationStatus run_time_sync(PuppyModbus &);
    CommunicationStatus read_general_status(PuppyModbus &);
    void handle_nozzle_presence(); ///< Update cached_nozzle_state from latest modbus data
    bool set_loadcell_nolock(PuppyModbus &, bool active);
    bool raw_set_loadcell(PuppyModbus &, bool active); // Low level loadcell enable/disable, no dependencies

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
