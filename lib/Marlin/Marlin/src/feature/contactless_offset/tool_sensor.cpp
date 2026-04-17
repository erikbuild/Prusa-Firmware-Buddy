#include "tool_sensor.hpp"
#include <hwio_pindef.h>
#include <utils/atomic_circular_queue.hpp>
#include <timing.h>
#include <atomic>
#include <cassert>

namespace tool_offset {

class LocalLdc1612 : public Sensor {
public:
    static constexpr size_t QUEUE_SIZE = 256;
    static constexpr uint16_t CH1_RCOUNT = 2000;
    static constexpr buddy::hw::LDC1612::Channel CHANNEL = buddy::hw::LDC1612::Channel::CH1;

    LocalLdc1612() = default;

    void start() override {
        last_error = Error::NONE;
        sample_queue.clear();
        running = false;
        async_in_flight = false;

        if (!buddy::hw::ldc1612.is_device_present()) {
            last_error = Error::HW_FAILURE;
            return;
        }

        buddy::hw::LDC1612::DeviceConfig config = {
            .sleep_mode = false,
            .use_external_clock = true,
            .rp_override_en = true,
            .auto_amp_dis = false,
            .mux_config = {
                .deglitch = buddy::hw::LDC1612::DeglitchFilter::MHz_10,
            },
            .error_config = {},
            .ch0 = { .rcount = 2000, .settlecount = 32, .fin_divider = 1, .fref_divider = 1, .drive_current = 31, .offset = 0x0000 },
            .ch1 = { .rcount = CH1_RCOUNT, .settlecount = 256, .fin_divider = 1, .fref_divider = 1, .drive_current = 30, .offset = 0x0000 }
        };

        if (!buddy::hw::ldc1612.initialize(config)) {
            last_error = Error::HW_FAILURE;
            return;
        }

        if (!buddy::hw::ldc1612.set_single_channel_mode(CHANNEL)) {
            last_error = Error::HW_FAILURE;
            return;
        }

        // Just make sure the first sample is valid, read a sample synchronously
        uint32_t timeout_start = ticks_us();
        static constexpr uint32_t timeout_us = 100'000;
        static constexpr int discard_initial_samples = 30;
        for (int i = 0; i < discard_initial_samples; ++i) {
            while (!buddy::hw::ldc1612.is_data_ready(CHANNEL)) {
                if (ticks_us() - timeout_start > timeout_us) {
                    last_error = Error::HW_FAILURE;
                    return;
                }
            }
            buddy::hw::ldc1612.read_channel(CHANNEL);
        }

        sample_count = 0;
        start_time = ticks_us();
        running = true;
        start_async_read();
    }

    void stop() override {
        running = false;
        buddy::hw::ldc1612.set_sleep_mode(true);
        sample_queue.clear();
    }

    std::optional<float> get_sample() override {
        uint32_t raw_value;
        if (sample_queue.dequeue(raw_value)) {
            return static_cast<float>(raw_value);
        }

        if (running && !async_in_flight) {
            start_async_read();
        }

        return std::nullopt;
    }

    float sampling_freq() const override {
        uint32_t count = sample_count;
        if (count == 0) {
            return 0.0f;
        }
        uint32_t elapsed_us = ticks_us() - start_time;
        if (elapsed_us == 0) {
            return 0.0f;
        }
        return static_cast<float>(count) * 1'000'000.0f / static_cast<float>(elapsed_us);
    }

    Error get_last_error() const override {
        return last_error;
    }

private:
    AtomicCircularQueue<uint32_t, uint32_t, QUEUE_SIZE> sample_queue;
    std::atomic<bool> running { false };
    std::atomic<bool> async_in_flight { false };
    std::atomic<uint32_t> sample_count { 0 };
    uint32_t start_time { 0 };
    Error last_error { Error::NONE };

    void start_async_read() {
        if (!running) {
            return;
        }

        async_in_flight = true;

        bool started = buddy::hw::ldc1612.read_channel_async(
            CHANNEL,
            [this](std::optional<uint32_t> data) {
                handle_async_complete(data);
            });

        if (!started) {
            async_in_flight = false;
        }
    }

    void handle_async_complete(std::optional<uint32_t> data) {
        async_in_flight = false;

        if (!running) {
            return;
        }

        if (data.has_value()) {
            ++sample_count;
            bool enqueued = sample_queue.enqueue(data.value());
            assert(enqueued && "Sensor sample queue overrun - polling too slow");
            if (!enqueued) {
                last_error = Error::OVERFLOW;
            }
        }

        start_async_read();
    }
};

std::unique_ptr<Sensor> get_default_sensor() {
    return std::make_unique<LocalLdc1612>();
}

} // namespace tool_offset
