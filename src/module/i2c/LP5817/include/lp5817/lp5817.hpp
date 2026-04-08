#pragma once

#include <i2c/base.hpp>

#include <expected>

namespace lp5817 {
template <i2c::Device HWImpl>
class LP5817 : public HWImpl {
public:
    static constexpr i2c::Address ADDRESS = 0x2D;

    enum class Error {
        hal_error, // HWImpl function returned false
        brownout, // POR reported in flags
        overheat, // TSD reported in flags
        not_inited,
    };

    template <typename T>
    using Result = std::expected<T, Error>;

    // TODO: More configuration arguments (aka power and initial color)
    Result<void> init() {
        // --- STEP 0: Software Reset ---
        // Ensure chip is in a clean state (Datasheet 7.6.7) [cite: 982]
        if (const auto res = write_register(Register::RESET, 0xCC); !res.has_value()) {
            return res;
        }
        this->delay_ms(5); // Wait for internal reboot
        // --- STEP 1: Enable Chip ---
        // (Write 01h to register 00h)
        if (const auto res = write_register(Register::CHIP_EN, 0x01); !res.has_value()) {
            return res;
        }

        // Datasheet requirement: Wait >1ms after enable before analog config [cite: 1138]
        this->delay_ms(2);

        state = State::initializing;

        // Extra non documented step. After enable we should clear the POWER ON flag
        if (const auto res = write_register(Register::FLAG_CLR, 0x01); !res.has_value()) {
            return res;
        }

        // --- STEP 2: Configure Current (51mA range) ---
        // (Write 01h to register 01h)
        if (const auto res = write_register(Register::CONF0, 0x01); !res.has_value()) {
            return res;
        }

        // --- STEP 3: Set Dot Currents (Analog Gain) ---
        // Setting Max Current for all channels
        static constexpr std::array<std::byte, 3> target_pwr = { std::byte { 0xff }, std::byte { 0xff }, std::byte { 0xff } };
        if (!this->write_memory(ADDRESS, std::to_underlying(Register::OUT0_DC), target_pwr)) {
            state = State::invalid;
            return std::unexpected(Error::hal_error);
        }

        // --- STEP 4: Enable Outputs ---
        // Enable OUT0, OUT1, OUT2 (Binary 0000 0111)
        if (const auto res = write_register(Register::CONF1, 0x07); !res.has_value()) {
            return res;
        }

        // --- STEP 5: Apply Configuration (UPDATE 1) ---
        // Essential to latch analog configurations [cite: 887]
        if (const auto res = write_register(Register::UPDATE, 0x55); !res.has_value()) {
            return res;
        }

        // --- STEP 6: Set Initial PWM (Brightness) ---
        if (const auto res = set_color(0xff, 0xff, 0xff); !res.has_value()) {
            return res;
        }

        // Wait for analog circuit to stabilize before checking faults
        this->delay_ms(5);

        // --- STEP 7: Fault Verification ---
        if (const auto res = check_for_faults(); !res.has_value()) {
            return res;
        }

        state = State::running;

        return {};
    }

    Result<void> check_for_faults() {
        Result<uint8_t> faults = 0;
        if (faults = read_register(Register::FLAG); !faults.has_value()) {
            return std::unexpected(faults.error());
        }

        static constexpr uint8_t POR_FLAG = 0b0000'0001;
        if (*faults & POR_FLAG) {
            return std::unexpected(Error::brownout);
        }

        static constexpr uint8_t TSD_FLAG = 0b0000'0010;
        if (*faults & TSD_FLAG) {
            return std::unexpected(Error::overheat);
        }

        return {};
    }

    Result<void> set_color(uint8_t r, uint8_t g, uint8_t b) {
        if (state != State::running && state != State::initializing) {
            state = State::invalid;
            return std::unexpected(Error::not_inited);
        }

        const std::array<std::byte, 3> desired_color = { std::byte { r }, std::byte { g }, std::byte { b } };

        if (!this->write_memory(ADDRESS, std::to_underlying(Register::OUT0_PWM), desired_color)) {
            state = State::invalid;
            return std::unexpected(Error::hal_error);
        }

        return {};
    }

protected:
    enum class Register : uint8_t {
        CHIP_EN = 0x00,

        CONF0 = 0x01,
        CONF1 = 0x02,
        CONF2 = 0x03,
        CONF3 = 0x04,

        SHUTDOWN = 0x0D,
        RESET = 0x0E,
        UPDATE = 0x0F,

        FLAG_CLR = 0x13,

        OUT0_DC = 0x14,
        OUT1_DC = 0x15,
        OUT2_DC = 0x16,

        OUT0_PWM = 0x18,
        OUT1_PWM = 0x19,
        OUT2_PWM = 0x1A,

        FLAG = 0x40,
    };

    Result<void> write_register(Register reg, uint8_t value) {
        if (!this->write_memory(ADDRESS, std::to_underlying(reg), std::span { reinterpret_cast<const std::byte *>(&value), 1 })) {
            state = State::invalid;
            return std::unexpected(Error::hal_error);
        }
        return {};
    }

    Result<uint8_t> read_register(Register reg) {
        uint8_t value = 0;
        if (!this->read_memory(ADDRESS, std::to_underlying(reg), std::span { reinterpret_cast<std::byte *>(&value), 1 })) {
            state = State::invalid;
            return std::unexpected(Error::hal_error);
        }
        return { value };
    }

    enum class State : uint8_t {
        invalid,
        initializing,
        running,
    } state
        = State::invalid;
};
} // namespace lp5817
