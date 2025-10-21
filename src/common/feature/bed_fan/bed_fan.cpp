/// @file
#include <feature/bed_fan/bed_fan.hpp>

#include <option/has_ac_controller.h>

#if HAS_AC_CONTROLLER()
    #include <puppies/ac_controller.hpp>
#endif

namespace bed_fan {

BedFan &bed_fan() {
    static BedFan instance;
    return instance;
}

#if HAS_AC_CONTROLLER()

std::optional<uint8_t> BedFan::get_pwm() const {
    return buddy::puppies::ac_controller.get_bed_fan_pwm();
}

std::optional<std::array<uint16_t, 2>> BedFan::get_rpm() const {
    return buddy::puppies::ac_controller.get_bed_fan_rpm();
}

void BedFan::set_pwm(uint8_t pwm) {
    if (pwm == 0) {
        start_timestamp_ms = 0;
    } else {
        start_timestamp_ms = start_timestamp_ms != 0 ? start_timestamp_ms : ticks_ms();
    }
    buddy::puppies::ac_controller.set_bed_fan_pwm(pwm);
}

bool BedFan::is_rpm_ok() {
    static constexpr uint32_t rpm_start_delay_ms = 5000;
    const auto bed_fan_rpms = get_rpm();
    if (!bed_fan_rpms.has_value() || start_timestamp_ms == 0 || ticks_diff(start_timestamp_ms + rpm_start_delay_ms, ticks_ms()) >= 0) {
        return true;
    }

    return bed_fan_rpms.value()[0] > 0 && bed_fan_rpms.value()[1] > 0;
}

#else

    #error

#endif

} // namespace bed_fan
