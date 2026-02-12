/// @file
#pragma once

#include <inc/MarlinConfig.h>
#include <module/thermistor/thermistors.h>

#if HOTENDS <= 1
    #define HOTEND_INDEX 0
    #define E_NAME
#else
    #define HOTEND_INDEX e
    #define E_NAME       e
#endif

// Identifiers for other heaters
enum heater_ind_t : int8_t {
    INDEX_NONE = -4,
    H_REDUNDANT,
    H_BOARD,
    H_BED,
    H_NOZZLE_FIRST,
    H_NOZZLE_LAST = H_NOZZLE_FIRST + HOTENDS - 1,
    H_HEATBREAK_FIRST,
    H_HEATBREAK_LAST = H_HEATBREAK_FIRST + HOTENDS - 1,
};
static_assert(H_NOZZLE_FIRST == 0); // lots of places in are indexed by this, and assumes H_E0 is zero

/// A bold assumption used by steady_state_hotend and temp regulator
static constexpr float ambient_temp = 21.0f;

// PID storage
struct PID_t {
    float Kp = 0, Ki = 0, Kd = 0;
};

// Minimum number of Temperature::ISR loops between sensor readings.
// Multiplied by 16 (OVERSAMPLENR) to obtain the total time to
// get all oversampled sensor readings
#define MIN_ADC_ISR_LOOPS 10

/**
 * States for ADC reading in the ISR
 */
enum ADCSensorState : char {
    StartSampling,
#if HAS_TEMP_ADC_0
    PrepareTemp_0,
    MeasureTemp_0,
#endif
#if HAS_LOCAL_BED()
    PrepareTemp_BED,
    MeasureTemp_BED,
#endif
#if HAS_TEMP_HEATBREAK
    PrepareTemp_HEATBREAK,
    MeasureTemp_HEATBREAK,
#endif
#if HAS_TEMP_BOARD
    PrepareTemp_BOARD,
    MeasureTemp_BOARD,
#endif
#if PRINTER_IS_PRUSA_iX()
    PrepareTemp_PSU,
    MeasureTemp_PSU,
    PrepareTemp_AMBIENT,
    MeasureTemp_AMBIENT,
#endif
#if HAS_TEMP_ADC_1
    PrepareTemp_1,
    MeasureTemp_1,
#endif
#if HAS_TEMP_ADC_2
    PrepareTemp_2,
    MeasureTemp_2,
#endif
#if HAS_TEMP_ADC_3
    PrepareTemp_3,
    MeasureTemp_3,
#endif
#if HAS_TEMP_ADC_4
    PrepareTemp_4,
    MeasureTemp_4,
#endif
#if HAS_TEMP_ADC_5
    PrepareTemp_5,
    MeasureTemp_5,
#endif
    SensorsReady, // Temperatures ready. Delay the next round of readings to let ADC pins settle.
    StartupDelay // Startup, delay initial temp reading a tiny bit so the hardware can settle
};

#define ACTUAL_ADC_SAMPLES _MAX(int(MIN_ADC_ISR_LOOPS), int(SensorsReady))

#define PID_dT ((OVERSAMPLENR * float(ACTUAL_ADC_SAMPLES)) / TEMP_TIMER_FREQUENCY)

// Apply the scale factors to the PID values
#define scalePID_i(i)   (float(i) * PID_dT)
#define unscalePID_i(i) (float(i) / PID_dT)
#define scalePID_d(d)   (float(d) / PID_dT)
#define unscalePID_d(d) (float(d) * PID_dT)

#if ENABLED(HW_PWM_HEATERS)
static constexpr uint8_t soft_pwm_bit_shift = 0;
#else
static constexpr uint8_t soft_pwm_bit_shift = 1;
#endif
