#pragma once

#include <printers.h>
#include <option/xl_enclosure_support.h>

// FANCTLPRINT - printing fan
inline constexpr uint8_t FANCTLPRINT_PWM_MIN = 10; // min duty cycle length 10 / 50 = 0.2 = 20%
inline constexpr uint8_t FANCTLPRINT_PWM_MAX = 50; // 1000Hz / 50 = 20Hz PWM cycle
#if PRINTER_IS_PRUSA_MK4()
inline constexpr uint16_t FANCTLPRINT_RPM_MIN = 90; // Dynamic PWM enables lower RPM
#else
inline constexpr uint16_t FANCTLPRINT_RPM_MIN = 150;
#endif
inline constexpr uint16_t FANCTLPRINT_RPM_MAX =
#if HAS_INDX()
    10000
#elif (PRINTER_IS_PRUSA_MK4() || PRINTER_IS_PRUSA_MK3_5() || PRINTER_IS_PRUSA_iX() || PRINTER_IS_PRUSA_XL() || PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL())
    6850
#elif PRINTER_IS_PRUSA_MINI()
    5000
#else
    #error "You need to specify printfans max RPM"
#endif
    ;
inline constexpr uint8_t FANCTLPRINT_PWM_THR = 20;

// On Mk3 the printer would ignore rpm measurements if the pwm was under 30%.
// Because some of the printers have a really weak print fan, it would cause
// MK3.5 users to get print fan errors on low pwm, that wouldn't happend on MK3.
// Sadly since we doing pwm differently we are not able to set it to 30% exactly,
// but rather we round to nearest int:
// <= 32% - ignore RPM measurement
// >= 33% - will trigger print fan error if the pwm is too low (FANCTLPRINT_RPM_MIN)
inline constexpr uint8_t FANCTLPRINT_MIN_PWM_TO_MEASURE_RPM =
#if PRINTER_IS_PRUSA_MK3_5()
    FANCTLPRINT_PWM_MAX * 0.3;
#else
    0;
#endif

// FANCTLHEATBREAK - heatbreak fan
inline constexpr uint8_t FANCTLHEATBREAK_PWM_MIN = 0;
inline constexpr uint8_t FANCTLHEATBREAK_PWM_MAX = 50;
inline constexpr uint16_t FANCTLHEATBREAK_RPM_MIN = 1000;
inline constexpr uint16_t FANCTLHEATBREAK_RPM_MAX =
#if HAS_INDX()
    20000
#elif (PRINTER_IS_PRUSA_MK4() || PRINTER_IS_PRUSA_MK3_5() || PRINTER_IS_PRUSA_iX() || PRINTER_IS_PRUSA_XL() || PRINTER_IS_PRUSA_COREONE() || PRINTER_IS_PRUSA_COREONEL())
    15180
#elif PRINTER_IS_PRUSA_MINI()
    8000
#else
    #error "You need to specify printfans max RPM"
#endif
    ;
inline constexpr uint8_t FANCTLHEATBREAK_PWM_THR = 20;
inline constexpr uint8_t FANCTLHEATBREAK_MIN_PWM_TO_MEASURE_RPM = 0;

// FANCTLENCLOSURE - enclosure fan
#if XL_ENCLOSURE_SUPPORT()
inline constexpr uint8_t FANCTLENCLOSURE_PWM_MIN = 0;
inline constexpr uint8_t FANCTLENCLOSURE_PWM_MAX = 255;
inline constexpr uint16_t FANCTLENCLOSURE_RPM_MIN = 600;
inline constexpr uint16_t FANCTLENCLOSURE_RPM_MAX = 2700;
#endif // XL_ENCLOSURE_SUPPORT
