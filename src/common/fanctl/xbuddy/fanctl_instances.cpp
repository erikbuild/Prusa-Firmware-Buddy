
#include <fanctl.hpp>
#include "hwio_pindef.h"
#include <fanctl/CFanCtl3Wire.hpp>
#include <CFanCtlCommonConsts.hpp>
#include "CFanCtl3WireDynamic.hpp"
#include <option/has_love_board.h>
#include <hw_configuration.hpp>
#include <option/has_indx.h>
#include <option/has_indx_head.h>

#if HAS_INDX()
    #include <fanctl/xlbuddy/CFanCtlPuppy.hpp>
#endif

#if !PRINTER_IS_PRUSA_MK4() && !PRINTER_IS_PRUSA_COREONE() && !PRINTER_IS_PRUSA_COREONEL()
    #error "Dynamic PWM is only for MK4/COREONE/COREONEL, fix your CMakeLists.txt!"
#endif

CFanCtlCommon &Fans::print([[maybe_unused]] size_t index) {
#if HAS_INDX()
    static auto instance = CFanCtlPuppy(0, 0, false, FANCTLPRINT_RPM_MAX);
#else
    static auto instance = CFanCtl3WireDynamic(
        buddy::hw::fanPrintPwm,
        buddy::hw::fanTach,
        FANCTLPRINT_PWM_MIN, FANCTLPRINT_PWM_MAX,
        FANCTLPRINT_RPM_MIN, FANCTLPRINT_RPM_MAX,
        FANCTLPRINT_PWM_THR,
        is_autofan_t::no,
        skip_tacho_t::yes,
        FANCTLPRINT_MIN_PWM_TO_MEASURE_RPM);

    if (index) {
        bsod("Print fan %u does not exist", index);
    }
#endif
    return instance;
};

#if !HAS_INDX_HEAD()
static auto write_heat_pwm = [](bool value) {
    buddy::hw::fanHeatBreakPwm.writeb(value);
};

static auto read_heat_tacho = []() {
    return buddy::hw::fanTach.readb();
};
#endif

CFanCtlCommon &Fans::heat_break([[maybe_unused]] size_t index) {
#if HAS_INDX()
    static auto instance = CFanCtlPuppy(0, 1, true, FANCTLHEATBREAK_RPM_MAX);
#else
    static CFanCtl3Wire instance = CFanCtl3Wire(
        write_heat_pwm,
        read_heat_tacho,
        {
            .min_pwm = FANCTLHEATBREAK_PWM_MIN,
            .max_pwm = FANCTLHEATBREAK_PWM_MAX,
            .min_rpm = FANCTLHEATBREAK_RPM_MIN,
            .max_rpm = FANCTLHEATBREAK_RPM_MAX,
            .thr_pwm = FANCTLHEATBREAK_PWM_THR,
            .autofan = is_autofan_t::yes,
            .skip_tacho = skip_tacho_t::no,
            .min_pwm_to_measure_rpm = FANCTLHEATBREAK_MIN_PWM_TO_MEASURE_RPM,
            .has_inverted_pwm = buddy::hw::Configuration::Instance().has_inverted_fans(),
        });

    if (index) {
        bsod("Heat break fan %u does not exist", index);
    }
#endif
    return instance;
};

#if HAS_INDX()
CFanCtlCommon &Fans::dock_fan() {
    // Same hardware as the C1 print fan (NEXTRUDER print-fan pin + shared tach).
    static auto instance = CFanCtl3WireDynamic(
        buddy::hw::fanPrintPwm,
        buddy::hw::fanTach,
        FANCTLPRINT_PWM_MIN, FANCTLPRINT_PWM_MAX,
        FANCTLPRINT_RPM_MIN, FANCTLPRINT_RPM_MAX,
        FANCTLPRINT_PWM_THR,
        is_autofan_t::no,
        skip_tacho_t::no,
        FANCTLPRINT_MIN_PWM_TO_MEASURE_RPM);
    return instance;
}
#endif

void Fans::tick() {
#if HAS_INDX()
    Fans::dock_fan().tick();
#else
    CFanCtl3Wire &heatbreak_fan = static_cast<CFanCtl3Wire &>(Fans::heat_break(PhysicalToolIndex::from_raw(0)));
    CFanCtl3Wire &print_fan = static_cast<CFanCtl3Wire &>(Fans::print(PhysicalToolIndex::from_raw(0)));

    if (heatbreak_fan.getSkipTacho() != skip_tacho_t::yes && heatbreak_fan.get_rpm_measured()) {
        buddy::hw::tachoSelectPrintFan.write(buddy::hw::Pin::State::high);
        print_fan.setSkipTacho(skip_tacho_t::no);
        heatbreak_fan.setSkipTacho(skip_tacho_t::yes);
    } else if (print_fan.getSkipTacho() != skip_tacho_t::yes && print_fan.get_rpm_measured()) {
        buddy::hw::tachoSelectPrintFan.write(buddy::hw::Pin::State::low);
        heatbreak_fan.setSkipTacho(skip_tacho_t::no);
        print_fan.setSkipTacho(skip_tacho_t::yes);
    }
    Fans::print(PhysicalToolIndex::from_raw(0)).tick();
    Fans::heat_break(PhysicalToolIndex::from_raw(0)).tick();
#endif
}
