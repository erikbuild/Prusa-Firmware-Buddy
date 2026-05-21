#include <puppies/xbuddy_extension.hpp>

#include <bsod/bsod.h>
#include "buddy/digest.hpp"
#include "timing.h"
#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <logging/log.hpp>
#include <modbus/modbus.hpp>
#include <mutex>
#include <sys/fcntl.h>
#include <sys/unistd.h>
#include <utility>
#include <xbuddy_extension/shared_enums.hpp>

LOG_COMPONENT_REF(Buddy);

using Lock = std::unique_lock<freertos::Mutex>;

using FileId = xbuddy_extension::FileId;

static const char *get_file_path(FileId file_id) {
    switch (file_id) {
    case FileId::none:
        break;
    case FileId::firmware_ac_controller:
        return "/internal/res/puppies/fw-ac_controller.bin";
    case FileId::firmware_anfc:
        return "/internal/res/puppies/fw-anfc.bin";
    case FileId::firmware_tool_offset_sensor:
        return "/internal/res/puppies/fw-tool_offset_sensor.bin";
    }
    bsod_unreachable();
}

static int open(FileId file_id) {
    const char *path = get_file_path(file_id);
    const int fd = open(path, O_RDONLY);
    if (fd == -1) {
        log_error(Buddy, "open(%s) failed %d", path, errno);
    }
    return fd;
}

struct ClosingFileDescriptor {
    int fd;
    ~ClosingFileDescriptor() {
        if (fd != -1) {
            (void)close(fd);
        }
    }
};

static constexpr uint8_t unit = std::to_underlying(modbus::ServerAddress::xbuddy_extension);

namespace {

// The xbuddy takes action if not seen an update for 1 second (in particular,
// it signals its unhealthy status in 1-second heartbeats).
//
// Try to do at least three "activities" during that time, to have some error
// margin if something gets lost or delayed.
constexpr int32_t activity_update_every_ms = 300;

} // namespace

namespace xbuddy_extension::modbus {
namespace {

    void compute_digest_response(DigestRequest request, FileId file_id, Digest &modbus_digest) {
        const ClosingFileDescriptor fd { ::open(file_id) };

        const uint32_t salt = static_cast<uint32_t>(request.salt_hi << 16) | static_cast<uint32_t>(request.salt_lo);
        modbus_digest.request = request;

        DigestStatus digest_status;
        if (fd.fd == -1) {
            modbus_digest.data = {};
            digest_status = DigestStatus::unavailable;
        } else {
            // we defined Digest::data as little endian => no byte swapping needed
            // we also compute the digest in-place and save some stack space
            static_assert(std::endian::native == std::endian::little);
            const auto buddy_digest = std::as_writable_bytes(std::span { modbus_digest.data });
            if (buddy::compute_file_digest(fd.fd, salt, buddy_digest)) {
                digest_status = DigestStatus::ok;
            } else {
                log_error(Buddy, "buddy::compute_file_digest() failed");
                modbus_digest.data = {};
                digest_status = DigestStatus::retry;
            }
        }
        modbus_digest.status = serialize_digest_status(digest_status);
    }

} // namespace
} // namespace xbuddy_extension::modbus

namespace buddy::puppies {

void XBuddyExtension::set_fan_pwm(size_t fan_idx, uint8_t pwm) {
    assert(fan_idx < FAN_CNT);
    fan_pwm_desired[fan_idx].store(pwm);
}

uint8_t XBuddyExtension::get_requested_fan_pwm(size_t fan_idx) {
    assert(fan_idx < FAN_CNT);
    return fan_pwm_desired[fan_idx].load();
}

uint8_t XBuddyExtension::get_flash_progress_percent() const {
    Lock lock(mutex);

    if (flash_file_size == 0) {
        return 0;
    }

    const uint32_t flash_file_offset = static_cast<uint32_t>(last_chunk_request.offset_hi << 16) | static_cast<uint32_t>(last_chunk_request.offset_lo);
    return 100 * flash_file_offset / flash_file_size;
}

bool XBuddyExtension::get_usb_power() const {
    return usb_power_desired.load();
}

void XBuddyExtension::set_white_led(uint8_t intensity) {
    w_led_pwm_desired.store(intensity);
}

void XBuddyExtension::set_white_strobe_frequency(std::optional<uint16_t> freq) {
    assert(freq != 0); // Explicit 0 makes no sense as frequency
    w_led_frequency_desired.store(freq.value_or(0));
}

void XBuddyExtension::set_rgbw_led(std::array<uint8_t, 4> color) {
    const uint32_t packed = static_cast<uint32_t>(color[0])
        | (static_cast<uint32_t>(color[1]) << 8)
        | (static_cast<uint32_t>(color[2]) << 16)
        | (static_cast<uint32_t>(color[3]) << 24);
    rgbw_led_desired.store(packed);
}

void XBuddyExtension::set_usb_power(bool enabled) {
    usb_power_desired.store(enabled);
}

void XBuddyExtension::set_mmu_power(bool enabled) {
    mmu_power_desired.store(enabled);
}

void XBuddyExtension::set_mmu_nreset(bool enabled) {
    mmu_nreset_desired.store(enabled);
}

std::optional<uint16_t> XBuddyExtension::get_fan_rpm(size_t fan_idx) const {
    assert(fan_idx < FAN_CNT);
    if (!valid.load()) {
        return std::nullopt;
    }
    return cached_fan_rpm[fan_idx].load();
}

std::array<uint16_t, XBuddyExtension::FAN_CNT> XBuddyExtension::get_fans_rpm() const {
    if (!valid.load()) {
        return std::array<uint16_t, FAN_CNT> { 0, 0, 0 };
    }
    std::array<uint16_t, FAN_CNT> out;
    for (size_t i = 0; i < FAN_CNT; ++i) {
        out[i] = cached_fan_rpm[i].load();
    }
    return out;
}

std::optional<float> XBuddyExtension::get_chamber_temp() const {
    if (!valid.load()) {
        return std::nullopt;
    }
    return static_cast<float>(cached_chamber_temperature_dc.load()) / 10.0f;
}

std::optional<XBuddyExtension::FilamentSensorState> XBuddyExtension::get_gpio_filament_sensor_state() const {
    if (!valid.load()) {
        return std::nullopt;
    }
    return static_cast<FilamentSensorState>(cached_gpio_filament_sensor.load());
}

std::optional<XBuddyExtension::FilamentSensorState> XBuddyExtension::get_ext_filament_sensor_state(uint8_t index) const {
    if (!valid.load()) {
        return std::nullopt;
    }
    using Register = decltype(status.value.ext_filament_sensors);
    assert(index < xbuddy_extension::ext_filament_sensor_count);
    const uint8_t shift = index * xbuddy_extension::bits_per_fs_state;
    constexpr Register mask = (Register(1) << xbuddy_extension::bits_per_fs_state) - 1;
    return static_cast<FilamentSensorState>((cached_ext_filament_sensors.load() >> shift) & mask);
}

CommunicationStatus XBuddyExtension::refresh_input(PuppyModbus &bus, uint32_t max_age) {
    // Already locked by caller.

    const auto result = bus.read(unit, status, max_age);

    switch (result) {
    case CommunicationStatus::OK:
        for (size_t i = 0; i < FAN_CNT; ++i) {
            cached_fan_rpm[i].store(status.value.fan_rpm[i]);
        }
        cached_chamber_temperature_dc.store(status.value.temperature);
        cached_gpio_filament_sensor.store(status.value.gpio_filament_sensor);
        cached_ext_filament_sensors.store(status.value.ext_filament_sensors);
        // Only after we have published the cached_..., to make sure to never
        // expose an invalid value.
        valid.store(true);
        break;
    case CommunicationStatus::ERROR:
        valid.store(false);
        break;
    default:
        // SKIPPED doesn't change the validity.
        break;
    }

    return result;
}

CommunicationStatus XBuddyExtension::refresh_holding(PuppyModbus &bus) {
    // Already locked by caller

    const auto write = [&](uint16_t &dst, const uint16_t val) {
        if (val != dst) {
            dst = val;
            config.dirty = true;
        }
    };
    for (size_t i = 0; i < FAN_CNT; ++i) {
        write(config.value.fan_pwm[i], static_cast<uint16_t>(fan_pwm_desired[i].load()));
    }
    write(config.value.w_led_pwm, static_cast<uint16_t>(w_led_pwm_desired.load()));
    write(config.value.w_led_frequency, w_led_frequency_desired.load());
    const uint32_t rgbw = rgbw_led_desired.load();
    write(config.value.rgbw_led_r_pwm, static_cast<uint16_t>(rgbw & 0xff));
    write(config.value.rgbw_led_g_pwm, static_cast<uint16_t>((rgbw >> 8) & 0xff));
    write(config.value.rgbw_led_b_pwm, static_cast<uint16_t>((rgbw >> 16) & 0xff));
    write(config.value.rgbw_led_w_pwm, static_cast<uint16_t>((rgbw >> 24) & 0xff));
    write(config.value.usb_power, static_cast<uint16_t>(usb_power_desired.load() ? 1 : 0));
    write(config.value.mmu_power, static_cast<uint16_t>(mmu_power_desired.load() ? 1 : 0));
    write(config.value.mmu_nreset, static_cast<uint16_t>(mmu_nreset_desired.load() ? 1 : 0));

    uint32_t now = ticks_ms();
    // We update it every time (so it's fresh), but force it written (set as
    // dirty) only once in a while. That way we can piggy-back on other
    // requests and save some round trips.
    config.value.activity = now;
    if (ticks_diff(now, last_activity_update) > activity_update_every_ms) {
        config.dirty = true;
    }

    const auto result = bus.write(unit, config);
    if (result == CommunicationStatus::OK) {
        last_activity_update = now;
    }

    return result;
}

CommunicationStatus XBuddyExtension::write_chunk(PuppyModbus &bus) {
    if (!valid.load()) {
        // We need up to date values of the request. Otherwise, we don't try anything.
        return CommunicationStatus::ERROR;
    }

    const xbuddy_extension::modbus::ChunkRequest &current_request = status.value.chunk_request;

    if (flash_fd != -1 && last_chunk_request.file_id != current_request.file_id) {
        // The current file there is outdated (or maybe no request any more).
        // Get rid of this one, maybe create a new one later.
        close_flash_file();
    }

    const FileId file_id = xbuddy_extension::modbus::parse_file_id(current_request.file_id);
    if (file_id == FileId::none) {
        // No request -> we are done.
        return CommunicationStatus::SKIPPED;
    }

    if (flash_fd != -1) {
        if (last_chunk_request == current_request) {
            // We didn't get a newer request yet, this one was already sent, wait for newer one.
            return CommunicationStatus::SKIPPED;
        }
    } else {
        flash_fd = open(file_id);
        if (flash_fd == -1) {
            return CommunicationStatus::SKIPPED;
        }
        // Cache the file size when opening
        const off_t lseek_result = lseek(flash_fd, 0, SEEK_END);
        if (lseek_result == -1) {
            log_error(Buddy, "lseek() failed %d", errno);
            close_flash_file();
            return CommunicationStatus::SKIPPED;
        }
        flash_file_size = lseek_result;
        last_chunk_request.file_id = current_request.file_id;
    }

    const uint32_t chunk_offset = static_cast<uint32_t>(current_request.offset_hi << 16) | static_cast<uint32_t>(current_request.offset_lo);
    if (lseek(flash_fd, chunk_offset, SEEK_SET) == -1) {
        log_error(Buddy, "lseek() failed %d", errno);
        close_flash_file();
        return CommunicationStatus::ERROR;
    }

    xbuddy_extension::modbus::Chunk modbus_chunk;
    modbus_chunk.request = current_request;

    // we defined Chunk::data as little endian => no byte swapping needed
    // we also read the chunk in-place and save some stack space
    static_assert(std::endian::native == std::endian::little);
    const auto chunk_buffer = std::as_writable_bytes(std::span { modbus_chunk.data });
    const size_t chunk_size = chunk_buffer.size();

    size_t cummulative_read = 0;
    // Deal with read being able to do short reads - we promise the other side
    // we'll give it full-sized chunks (unless it's the last one).
    while (cummulative_read < chunk_size) {
        const ssize_t nread = read(flash_fd, chunk_buffer.data() + cummulative_read, chunk_size - cummulative_read);
        if (nread == 0) {
            // EOF -> terminate, send whatever we have.
            break;
        } else if (nread == -1) {
            log_error(Buddy, "read() failed %d", errno);
            close_flash_file();
            return CommunicationStatus::ERROR;
        } else {
            cummulative_read += nread;
        }
    }

    modbus_chunk.size = cummulative_read;

    if (bus.write_holding_registers(modbus::ServerAddress::xbuddy_extension, modbus_chunk)) {
        log_debug(Buddy, "sent chunk offset %" PRIu32 " size %zu", chunk_offset, cummulative_read);
        last_chunk_request = current_request;
        return CommunicationStatus::OK;
    }
    return CommunicationStatus::ERROR;
}

CommunicationStatus XBuddyExtension::write_digest(PuppyModbus &bus, DigestComputeFn compute) {
    const xbuddy_extension::modbus::DigestRequest current_request = status.value.digest_request;

    if (current_request == last_digest_request) {
        return CommunicationStatus::SKIPPED;
    }

    const FileId file_id = xbuddy_extension::modbus::parse_file_id(current_request.file_id);
    if (file_id == FileId::none) {
        return CommunicationStatus::OK;
    }

    // Callback runs the slow work with the mutex released and reacquires
    // before returning.
    xbuddy_extension::modbus::Digest modbus_digest;
    compute(current_request, file_id, modbus_digest);

    if (bus.write_holding_registers(modbus::ServerAddress::xbuddy_extension, modbus_digest)) {
        last_digest_request = current_request;
        return CommunicationStatus::OK;
    } else {
        // Best effort — retry on next cycle (last_digest_request not updated,
        // so dedup won't suppress it).
        return CommunicationStatus::SKIPPED;
    }
}

void XBuddyExtension::close_flash_file() {
    if (flash_fd != -1) {
        close(flash_fd);
        flash_fd = -1;
        flash_file_size = 0;
    }
}

CommunicationStatus XBuddyExtension::refresh(PuppyModbus &bus) {
    Lock lock(mutex);

    const auto status = {
        // Refresh on every exchange in case we are flashing - we want to update
        // the request ASAP, it's changing after each sent chunk.
        refresh_input(bus, flash_fd != -1 ? 0 : 250),
        refresh_holding(bus),
        refresh_log_message(bus),
        write_chunk(bus),
        write_digest(bus, [&lock](xbuddy_extension::modbus::DigestRequest request, FileId file_id, xbuddy_extension::modbus::Digest &out) {
            // Release mutex for the slow digest computation (file read + SHA256).
            lock.unlock();
            xbuddy_extension::modbus::compute_digest_response(request, file_id, out);
            lock.lock();
        }),
    };

    constexpr auto equal_to = [](CommunicationStatus what) {
        return [what](CommunicationStatus status) { return status == what; };
    };

    if (std::ranges::any_of(status, equal_to(CommunicationStatus::ERROR))) {
        return CommunicationStatus::ERROR;
    }
    if (std::ranges::all_of(status, equal_to(CommunicationStatus::SKIPPED))) {
        return CommunicationStatus::SKIPPED;
    }

    // Drain the Cyphal bridge queue (up to 5 reads per cycle)
    if (stream_callback_) {
        for (int i = 0; i < 5; ++i) {
            if (pull_cyphal_bridge(bus) != CommunicationStatus::OK) {
                break;
            }
            if (cyphal_bridge.value.bytes_available == 0) {
                break;
            }
        }
    }

    return CommunicationStatus::OK;
}

CommunicationStatus XBuddyExtension::initial_scan(PuppyModbus &bus) {
    Lock lock(mutex);

    const auto input = refresh_input(bus, 0);
    config.dirty = true;
    return input;
}

CommunicationStatus XBuddyExtension::ping(PuppyModbus &bus) {
    Lock lock(mutex);

    return refresh_input(bus, 0);
}

CommunicationStatus XBuddyExtension::set_mmu_power(PuppyModbus &bus, bool mmu_power) {
    // set_mmu_power variand called from PuppyBootstrap. Needs a "write
    // immediatelly" semantics, therefore having the lock.
    // The other set_mmu_power is called from Marlin, that can live with a delayed write.
    Lock lock(mutex);
    mmu_power_desired.store(mmu_power);
    config.dirty = true;
    return refresh_holding(bus);
}

CommunicationStatus XBuddyExtension::refresh_log_message(PuppyModbus &bus) {
    // Already locked by caller

    // Check if log_message_sequence changed at all
    if (status.value.log_message_sequence == last_log_message_sequence) {
        return CommunicationStatus::SKIPPED;
    }

    xbuddy_extension::modbus::LogMessage log_message;
    if (!bus.read_input_registers(modbus::ServerAddress::xbuddy_extension, log_message)) {
        // Do not update last_log_message_sequence, it will be retried on next cycle
        log_warning(Buddy, "XBE: failed to read log message");
        return CommunicationStatus::ERROR;
    }

    // Check if we missed any messages, work in modular arithmetic
    const uint16_t expected_sequence = static_cast<uint16_t>(last_log_message_sequence + 1);
    if (log_message.sequence != expected_sequence) {
        // If we missed message, we at least log...
        log_info(Buddy, "XBE: missed log message(s)");
        // ...and continue with the newest message.
    }

    static_assert(std::endian::native == std::endian::little);
    const auto text_size = std::min((size_t)log_message.text_size, 2 * log_message.text_data.size());
    const auto text_data = (const char *)log_message.text_data.data();
    log_info(Buddy, "XBE: %.*s", text_size, text_data);

    last_log_message_sequence = log_message.sequence;
    return CommunicationStatus::OK;
}

void XBuddyExtension::set_otp(const OTP_v5 &otp_data) {
    Lock lock(mutex);
    otp = otp_data;
}

OTP_v5 XBuddyExtension::get_otp() const {
    Lock lock(mutex);
    return otp;
}

void XBuddyExtension::set_stream_callback(StreamCallback cb, void *ctx) {
    Lock lock(mutex);
    stream_callback_ = cb;
    stream_callback_ctx_ = ctx;
}

CommunicationStatus XBuddyExtension::pull_cyphal_bridge(PuppyModbus &bus) {
    // Always read fresh (no caching)
    if (!bus.read_input_registers(modbus::ServerAddress::xbuddy_extension, cyphal_bridge.value)) {
        return CommunicationStatus::ERROR;
    }
    dispatch_bridge_messages();
    return CommunicationStatus::OK;
}

void XBuddyExtension::dispatch_bridge_messages() {
    if (!stream_callback_) {
        return;
    }

    static_assert(std::endian::native == std::endian::little);
    const auto bytes = std::as_bytes(std::span { cyphal_bridge.value.data });
    const uint16_t size = std::min<uint16_t>(cyphal_bridge.value.size, static_cast<uint16_t>(bytes.size()));
    size_t offset = 0;

    while (offset + 3 <= size) {
        const uint8_t payload_len = static_cast<uint8_t>(bytes[offset]);
        const uint16_t port_id = static_cast<uint16_t>(bytes[offset + 1])
            | (static_cast<uint16_t>(bytes[offset + 2]) << 8);
        offset += 3;

        if (offset + payload_len > size) {
            log_warning(Buddy, "XBE: bridge msg truncated len=%u offset=%zu size=%u", payload_len, offset, size);
            break;
        }

        stream_callback_(port_id, bytes.subspan(offset, payload_len), stream_callback_ctx_);
        offset += payload_len;
    }
}

XBuddyExtension xbuddy_extension;

} // namespace buddy::puppies
