///@file
#pragma once

#include "PuppyModbus.hpp"
#include "PuppyBus.hpp"
#include <cstddef>
#include <atomic>
#include <freertos/mutex.hpp>
#include <otp/types.hpp>
#include <span>
#include <xbuddy_extension/modbus.hpp>
#include <xbuddy_extension/shared_enums.hpp>

namespace buddy::puppies {

class XBuddyExtension final {
public:
    static constexpr size_t FAN_CNT = xbuddy_extension::fan_count;
    using FilamentSensorState = xbuddy_extension::FilamentSensorState;

    // These are called from whatever task that needs them.
    void set_fan_pwm(size_t fan_idx, uint8_t pwm);
    void set_white_led(uint8_t intensity);
    /**
     * Set the strobe frequency of the white led.
     *
     * This overrides the PWM cycle to be "slow" at the given frequency, creating a strobe effect.
     *
     * nullopt leaves it back to the extension board.
     *
     * As the PWM timer is used for some fans too, but it seems their
     * regulation works fine even in that case.
     */
    void set_white_strobe_frequency(std::optional<uint16_t> frequency);
    void set_rgbw_led(std::array<uint8_t, 4> rgbw);
    void set_usb_power(bool enabled);
    void set_mmu_power(bool enabled);
    void set_mmu_nreset(bool enabled);
    std::optional<uint16_t> get_fan_rpm(size_t fan_idx) const;

    /// A convenience function returning a snapshot of all fans' RPMs at once.
    /// Primarily used in feeding the Connect interface with a set of telemetry readings
    /// @returns known measured RPM of all fans at once (access mutex locked only once)
    /// If an data is not valid, returned readings are zeroed - that's what the Connect interface expects
    /// -> no need to play with std::optional which only makes usage much harded.
    std::array<uint16_t, FAN_CNT> get_fans_rpm() const;

    std::optional<float> get_chamber_temp() const;

    /// Single GPIO sensor (PA5 on standard, PA9 on iX)
    std::optional<FilamentSensorState> get_gpio_filament_sensor_state() const;

    /// TMP1826 multi-tool sensor (PC14/EXT connector)
    std::optional<FilamentSensorState> get_ext_filament_sensor_state(uint8_t index) const;

    uint8_t get_requested_fan_pwm(size_t fan_idx);

    /// Get current flash progress (0-100 percent, 0 if not flashing)
    uint8_t get_flash_progress_percent() const;

    bool get_usb_power() const;

    // Buddy-side communication error counter (since boot)
    std::atomic<uint16_t> refresh_error_count = 0;

    // Cyphal bridge stream callback -- called from puppy task for each
    // message drained from the XBE CyphalBridgeQueue.
    using StreamCallback = void (*)(uint16_t port_id, std::span<const std::byte> payload, void *ctx);
    void set_stream_callback(StreamCallback cb, void *ctx);

    // These are called from the puppy task.
    CommunicationStatus refresh(PuppyModbus &);
    CommunicationStatus initial_scan(PuppyModbus &);
    CommunicationStatus ping(PuppyModbus &);
    CommunicationStatus set_mmu_power(PuppyModbus &, bool mmu_power);
    CommunicationStatus set_mmu_nreset(PuppyModbus &, bool mmu_nreset);

    void set_otp(const OTP_v5 &);
    OTP_v5 get_otp() const;

private:
    // The registers cached here are accessed from different tasks.
    mutable freertos::Mutex mutex;

    // If reading/refresh failed, this'll be in invalid state and we'll return
    // nullopt for queries.
    bool valid = false;

    OTP_v5 otp = {};

    using Config = xbuddy_extension::modbus::Config;
    ModbusHoldingRegisterBlock<Config::address, Config> config;

    using Status = xbuddy_extension::modbus::Status;
    ModbusInputRegisterBlock<Status::address, Status> status;

    // Track last log sequence to detect new log messages
    uint16_t last_log_message_sequence = 0;

    // To not send activity updates too often.
    uint32_t last_activity_update = 0;

    // Just don't resend another request unless a new request comes.
    xbuddy_extension::modbus::ChunkRequest last_chunk_request = {};

    // The file we are reading from during flashing (-1 when not flashing).
    int flash_fd = -1;

    // The size of the flash file (cached when opening, 0 when not flashing).
    size_t flash_file_size = 0;

    void close_flash_file();

    CommunicationStatus refresh_holding(PuppyModbus &);
    CommunicationStatus refresh_input(PuppyModbus &, uint32_t max_age);
    CommunicationStatus write_chunk(PuppyModbus &);
    CommunicationStatus write_digest(PuppyModbus &);
    CommunicationStatus refresh_log_message(PuppyModbus &);

    // Cyphal bridge
    using CyphalBridge = xbuddy_extension::modbus::CyphalBridge;
    ModbusInputRegisterBlock<CyphalBridge::address, CyphalBridge> cyphal_bridge;
    StreamCallback stream_callback_ = nullptr;
    void *stream_callback_ctx_ = nullptr;
    CommunicationStatus pull_cyphal_bridge(PuppyModbus &);
    void dispatch_bridge_messages();
};

extern XBuddyExtension xbuddy_extension;

} // namespace buddy::puppies
