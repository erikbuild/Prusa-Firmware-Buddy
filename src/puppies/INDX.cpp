#include <puppies/INDX.hpp>

#include <cassert>
#include <utility>

#include <bsod/bsod.h>
#include <fifo_coder/fifo_decoder.hpp>
#include <freertos/mutex.hpp>
#include <indx_head/errors.hpp>
#include <indx_head/nozzle_presence.hpp>

#include <logging/log.hpp>
#include "loadcell.hpp"
#include "timing.h"
#include <logging/log_dest_bufflog.hpp>
#include <assert.h>
#include "metric.h"
#include <puppies/PuppyBootstrap.hpp>
#include <i18n.h>
#include <config_store/store_instance.hpp>
#include "Marlin/src/module/prusa/accelerometer.h"
#include <utils/string_builder.hpp>
#include <option/has_indx_head.h>
#include <otp/types.hpp>

using namespace fifo_coder;

namespace buddy::puppies {

using Lock = std::unique_lock<freertos::Mutex>;

LOG_COMPONENT_DEF(INDX, logging::Severity::info);

METRIC_DEF(metric_fast_refresh_delay, "dwarf_fast_refresh_delay", METRIC_VALUE_INTEGER, 0, METRIC_DISABLED);

Indx::Indx(uint8_t modbus_address)
    : ModbusDevice(modbus_address)
    , mutex(new freertos::Mutex)
    , time_sync(1) // Just magic number for metric (unnecessary with single head)
    , loadcell_samplerate {} {

    if (config_store().tool_leds_enabled.get()) {
        set_leds_color(COLOR_ORANGE, indx_head::leds::Mode::solid);
    } else {
        set_leds_color(COLOR_BLACK, indx_head::leds::Mode::off);
    }
}

CommunicationStatus Indx::refresh(PuppyModbus &bus) {
    Lock guard(*mutex);
    typedef CommunicationStatus (Indx::*MethodType)(PuppyModbus &);
    static constexpr MethodType funcs[] = {
        &Indx::read_general_status,
        &Indx::write_general,
        &Indx::run_time_sync,
    };
    if (++refresh_nr >= std::size(funcs)) {
        refresh_nr = 0;
    }
    return (this->*funcs[refresh_nr])(bus);
}

void Indx::handle_fault_status() {
    const auto fault = register_general_status.value.fault_status;
    if (fault == indx_head::errors::FaultStatusMask::no_fault) {
        // nothing to do
        return;
    }

    // handle the fault
    log_error(INDX, "Fault status: %d", std::to_underlying(fault));
    auto has_fault = [=](indx_head::errors::FaultStatusMask tested) {
        return std::to_underlying(fault) & std::to_underlying(tested);
    };
    if (has_fault(indx_head::errors::FaultStatusMask::board_min_temp)) {
        fatal_error(ErrCode::ERR_TEMPERATURE_INDX_HEAD_BOARD_MINTEMP_ERR);
    }
    if (has_fault(indx_head::errors::FaultStatusMask::board_max_temp)) {
        fatal_error(ErrCode::ERR_TEMPERATURE_INDX_HEAD_BOARD_MAXTEMP_ERR);
    }

    // acknowledge the fault
    general_write.value.clear_fault_status = std::to_underlying(fault);
    general_write.dirty = true;
}

void Indx::handle_nozzle_presence() {
    // Trust nozzle data only when both are true:
    //  1. Head echoed back our invalidation token (debouncer was reset)
    //  2. Head reports a definitive result (not unknown — debouncer has settled with fresh samples)
    using indx_head::NozzlePresence;
    const auto nozzle_state = static_cast<NozzlePresence>(register_general_status.value.nozzle_present);
    const bool has_definitive_result = nozzle_state != NozzlePresence::unknown;
    const bool head_acknowledged = register_general_status.value.nozzle_invalidation_ack == nozzle_invalidation_token;
    const bool valid = has_definitive_result && head_acknowledged;

    cached_nozzle_state.store(valid ? nozzle_state : NozzlePresence::unknown);
}

CommunicationStatus Indx::read_general_status(PuppyModbus &bus) {
    // read general status registers
    CommunicationStatus status = bus.read(unit, register_general_status, 250);
    if (status == CommunicationStatus::OK) {
        handle_fault_status();
        handle_nozzle_presence();
    }
    return status;
}

CommunicationStatus Indx::ping(PuppyModbus &bus) {
    Lock guard(*mutex);
    // Need to check if puppy is responding on modbus, might as well read general status.
    return bus.read(unit, register_general_status);
}

CommunicationStatus Indx::initial_scan(PuppyModbus &bus) {
    Lock guard(*mutex);
    time_sync.init();
    run_time_sync(bus);

    general_write.value.loadcell_enabled = true;
    general_write.dirty = true;

    return bus.write(unit, general_write);
}

void Indx::set_leds_color(Color color, indx_head::leds::Mode mode) {
    // FIXME: Calculate this on the head so we set correct colors from inside of the head, but needs to be rewritten into fixed point arythmetic
    static constexpr float gamma = 2.2f; // Use 2.6 or 2.8 for richer colors
    static constexpr float rgb_max = 255.f;
    static constexpr float pwm_max = 255.f;
    color.r = static_cast<uint8_t>(pwm_max * std::pow(float(color.r) / rgb_max, gamma));
    color.g = static_cast<uint8_t>(pwm_max * std::pow(float(color.g) / rgb_max, gamma));
    color.b = static_cast<uint8_t>(pwm_max * std::pow(float(color.b) / rgb_max, gamma));
    Lock guard(*mutex);
    general_write.value.leds.r = color.r;
    general_write.value.leds.g = color.g;
    general_write.value.leds.b = color.b;
    general_write.value.leds.mode = mode;
    general_write.dirty = true;
}

void Indx::set_leds_enabled(bool set) {
    Lock guard(*mutex);
    general_write.value.leds.mode = set ? indx_head::leds::Mode::solid : indx_head::leds::Mode::off;
    general_write.dirty = true;
}

CommunicationStatus Indx::pull_fifo(PuppyModbus &bus, bool &more) {
    Lock guard(*mutex);

    // Read coded FIFO
    std::array<uint16_t, MODBUS_FIFO_LEN> fifo;
    size_t read = 0;
    CommunicationStatus status = bus.ReadFIFO(unit, ENCODED_FIFO_ADDRESS, fifo, read, FIFO_RETRIES);
    if (status == CommunicationStatus::ERROR) {
        more = true; // Request failed, most probably there is more data waiting
        PrusaAccelerometer::set_possible_overflow();
        return status;
    }

    // calculate metric of read latency
    static uint32_t time_last_read = 0;
    auto now = ticks_ms();
    metric_record_integer(&metric_fast_refresh_delay, now - time_last_read);
    time_last_read = now;

    if (!read) {
        more = false;
        return CommunicationStatus::OK;
    }

    Decoder decoder(fifo, read);
    decoder.decode(*this);

    // Update sampling rate of the loadcell.ProcessSample()
    if (loadcell_samplerate.count > 30) {
        float interval = static_cast<float>(loadcell_samplerate.last_timestamp - loadcell_samplerate.last_processed_timestamp) / static_cast<float>(1000 * loadcell_samplerate.count);
        // Ignore invalid values, values outside of 25% of expected value may be caused by glitch in modbus communication
        if (interval >= loadcell_samplerate.expected * 0.75f && interval <= loadcell_samplerate.expected * 1.25f) {
            loadcell.analysis.SetSamplingIntervalMs(interval); // Update sampling interval
        }
        loadcell_samplerate.count = 0;
        loadcell_samplerate.last_processed_timestamp = loadcell_samplerate.last_timestamp;
    }

    more = decoder.more();
    return status;
}

namespace fans {
    constexpr size_t PRINTFAN_INDEX = 0;
    constexpr size_t HEATBREAKFAN_INDEX = 1;
} // namespace fans

CommunicationStatus Indx::write_general(PuppyModbus &bus) {
    // Handle delayed writes from possibly an interrupt.
    const auto write = [&](auto &dst, const auto val) {
        if (val != dst) {
            dst = val;
            general_write.dirty = true;
        }
    };
    write(general_write.value.print_fan_pwm.value, fan_pwm_desired[fans::PRINTFAN_INDEX].load());
    write(general_write.value.selftest_mode, selftest_mode_.load() ? 1 : 0);

    CommunicationStatus status = bus.write(unit, general_write);
    if (status == CommunicationStatus::ERROR) {
        return status;
    }

    log_debug(INDX, "Written GeneralWrite");
    general_write.value.clear_fault_status = 0;
    return status;
}

float Indx::get_hotend_temp_compensated() const {
    Lock guard(*mutex);

    // Sent as int16 in uint16 modbus register - sent in centiDeg (deg * 100) for precision on 2 decimal places
    return static_cast<float>(static_cast<int16_t>(register_general_status.value.hotend_measured_temperature_compensated_c100)) / 100.f;
}

float Indx::get_hotend_temp_uncompensated() const {
    Lock guard(*mutex);

    // Sent as int16 in uint16 modbus register - sent in centiDeg (deg * 100) for precision on 2 decimal places
    return static_cast<float>(static_cast<int16_t>(register_general_status.value.hotend_measured_temperature_uncompensated_c100)) / 100.f;
}

float Indx::get_hotend_temp_raw_c_dt_s() const {
    Lock guard(*mutex);

    // Sent in centiDeg (deg * 100) for precision on 2 decimal places
    return static_cast<float>(register_general_status.value.hotend_temp_raw_c100_dt_s) / 100.f;
}

CommunicationStatus Indx::set_hotend_target_temp(float target) {
    Lock guard(*mutex);

    general_write.value.nozzle_target_temperature = (uint16_t)target;
    general_write.dirty = true;
    return CommunicationStatus::OK;
}

CommunicationStatus Indx::set_hotend_temp_compensation(float offset) {
    Lock guard(*mutex);

    general_write.value.hotend_temperature_compensation_c100 = static_cast<int16_t>(offset * 100.0f);
    general_write.dirty = true;
    return CommunicationStatus::OK;
}

CommunicationStatus Indx::run_time_sync(PuppyModbus &bus) {
    RequestTiming timing;
    CommunicationStatus status = bus.read(unit, TimeSync, 1000, &timing);
    if (status == CommunicationStatus::ERROR) {
        log_error(INDX, "Failed to read fault status register");
        return status;
    }

    if (status != CommunicationStatus::SKIPPED) {
        time_sync.sync(TimeSync.value.dwarf_time_us, timing);
    }

    return status;
}

bool Indx::set_accelerometer(PuppyModbus &bus, bool active) {
    Lock guard(*mutex);
    general_write.value.accelerometer_enabled = active;
    general_write.dirty = true;
    return bus.write(unit, general_write) == CommunicationStatus::OK;
}

bool Indx::get_accelerometer_active() {
    Lock guard(*mutex);
    return general_write.value.accelerometer_enabled;
}

bool Indx::set_loadcell(PuppyModbus &bus, bool active) {
    Lock guard(*mutex);
    general_write.value.loadcell_enabled = active;
    general_write.dirty = true;
    return bus.write(unit, general_write) == CommunicationStatus::OK;
}

bool Indx::get_loadcell_active() {
    Lock guard(*mutex);
    return general_write.value.loadcell_enabled;
}

int16_t Indx::get_mcu_temperature() {
    // FIXME:
    // Called from interrupts, can't lock :-(
    // BFW-6219.
    // Lock guard(*mutex);

    // Sent as int16 in uint16 modbus register
    return static_cast<int16_t>(register_general_status.value.mcu_temperature);
}

int16_t Indx::get_board_temperature() {
    // FIXME:
    // Called from interrupts, can't lock :-(
    // BFW-6219.
    // Lock guard(*mutex);

    // Sent as int16 in uint16 modbus register
    return static_cast<int16_t>(register_general_status.value.board_temperature);
}

float Indx::get_24V() {
    // FIXME:
    // Called from interrupts, can't lock :-(
    // BFW-6219.
    // Lock guard(*mutex);

    return register_general_status.value.system_24V_mV / 1000.0f;
}

std::optional<bool> Indx::get_nozzle_present() {
    // Called from Marlin - keep lockfree.
    const auto state = cached_nozzle_state.load();
    if (state == indx_head::NozzlePresence::unknown) {
        return std::nullopt;
    }

    return state == indx_head::NozzlePresence::present;
}

void Indx::invalidate_nozzle_data() {
    Lock guard(*mutex);
    cached_nozzle_state.store(indx_head::NozzlePresence::unknown);

    // Generate a new token and send it to the head. The head will reset its
    // debouncer and echo the token back in nozzle_invalidation_ack.
    // handle_nozzle_presence() won't re-validate until the ack matches.
    nozzle_invalidation_token = static_cast<uint16_t>(ticks_ms());
    if (nozzle_invalidation_token == 0) {
        nozzle_invalidation_token = 1; // Avoid 0 — it's the head's initial ack value
    }
    general_write.value.invalidate_nozzle_presence = nozzle_invalidation_token;
    general_write.dirty = true;
}

void Indx::set_fan(uint8_t fan, uint16_t target) {
    assert(fan < NUM_FANS);
    // FIXME:
    // Because this sometimes gets called from an interrupt, we need to just
    // store the value and handle it properly under a lock somewhere else.
    // BFW-6219.
    fan_pwm_desired[fan].store(target);
}

void Indx::set_fan_auto(uint8_t fan) {
    assert(fan < NUM_FANS);
    fan_pwm_desired[fan].store(FAN_MODE_AUTO_PWM);
}

void Indx::set_selftest_mode(bool enabled) {
    selftest_mode_.store(enabled);
}

uint16_t Indx::get_heatbreak_fan_pwr() {
    // FIXME:
    // Called from interrupts, can't lock :-(
    // BFW-6219.
    // Lock guard(*mutex);
    return 0;
    // return RegisterGeneralStatus.value.heatbreak_fan_pwm;
}

void Indx::decode_log(const LogData &data) {
    // This function is mandated by the API, but head doesn't actually produce
    // any logs and we intend to keep it that way.
    (void)data;
}

void Indx::decode_loadcell(const LoadcellRecord &data) {
    // throw away samples if time is not synced
    if (!this->time_sync.is_time_sync_valid()) {
        return;
    }

    // Store sample timestamp and count sample
    loadcell_samplerate.last_timestamp = this->time_sync.buddy_time_us(data.timestamp);
    loadcell_samplerate.count++;

    loadcell.ProcessSample(data.loadcell_raw_value, loadcell_samplerate.last_timestamp);
}

void Indx::decode_accelerometer_fast(const AccelerometerFastData &data) {
    for (AccelerometerXyzSample sample : data) {
        PrusaAccelerometer::put_sample(sample);
    }
}

void Indx::decode_accelerometer_freq(const AccelerometerSamplingRate &data) {
    PrusaAccelerometer::set_rate(data.frequency);
}

uint16_t Indx::get_fan_pwm(uint8_t fan_nr) const {
    // Lock guard(*mutex);
    // FIXME:
    // Called from interrupts, can't lock :-(
    // BFW-6219.
    switch (fan_nr) {
    case fans::PRINTFAN_INDEX:
        return register_general_status.value.print_fan_pwm;
    case fans::HEATBREAKFAN_INDEX:
        return register_general_status.value.heatbreak_fan_pwm;
    }
    bsod_unreachable();
}

uint16_t Indx::get_fan_rpm(uint8_t fan_nr) const {
    // FIXME:
    // Called from interrupts, can't lock :-(
    // BFW-6219.
    // Lock guard(*mutex);
    switch (fan_nr) {
    case fans::PRINTFAN_INDEX:
        return register_general_status.value.print_fan_rpm;
    case fans::HEATBREAKFAN_INDEX:
        return register_general_status.value.heatbreak_fan_rpm;
    }
    bsod_unreachable();
}

bool Indx::get_fan_rpm_ok(uint8_t fan_nr) const {
    // FIXME:
    // Called from interrupts, can't lock :-(
    // BFW-6219.
    // Lock guard(*mutex);
    switch (fan_nr) {
    case fans::PRINTFAN_INDEX:
        return register_general_status.value.print_fan_is_rpm_ok;
    case fans::HEATBREAKFAN_INDEX:
        return register_general_status.value.heatbreak_fan_is_rpm_ok;
    }
    bsod_unreachable();
}

uint16_t Indx::get_fan_state(uint8_t fan_nr) const {
    // FIXME:
    // Called from interrupts, can't lock :-(
    // BFW-6219.
    // Lock guard(*mutex);
    switch (fan_nr) {
    case fans::PRINTFAN_INDEX:
        return register_general_status.value.print_fan_state;
    case fans::HEATBREAKFAN_INDEX:
        return register_general_status.value.heatbreak_fan_state;
    }
    bsod_unreachable();
}

void Indx::set_otp(const OTP_v5 &otp_data) {
    Lock guard(*mutex);
    otp = otp_data;
}

OTP_v5 Indx::get_otp() const {
    Lock guard(*mutex);
    return otp;
}

#if HAS_INDX_HEAD()
Indx indx { PuppyBootstrap::get_modbus_address_for_dock(Dock::INDX_HEAD) };
#endif

} // namespace buddy::puppies
