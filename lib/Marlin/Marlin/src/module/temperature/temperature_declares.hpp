/// @file
#pragma once

#include <cstdint>
#include <inc/MarlinConfigPre.h>
#include <module/temperature/marlin_temptable.hpp>
#include <tool_index.hpp>
#include <module/thermistor/thermistors.h>
#include <module/temperature/heater_watch.hpp>

// Temporary declares until everything is moved from temperature.cpp to hotends

struct temp_range_t {
    int16_t mintemp, maxtemp;
    MarlinTemptableRawMinMax raw;

    temp_range_t(MarlinTempTable temptable, int16_t mintemp, int16_t maxtemp)
        : mintemp(mintemp)
        , maxtemp(maxtemp)
        , raw(MarlinTemptableRawMinMax::compute(temptable, mintemp, maxtemp)) {}
};

#define TEMP_RANGE_INIT(i) temp_range_t(TT_NAME(THERMISTOR_HEATER_##i), HEATER_##i##_MINTEMP, HEATER_##i##_MAXTEMP)

inline StrongIndexArray<temp_range_t, HOTENDS, PhysicalToolIndex, PhysicalToolIndex::to_raw_static, strong_index_array::AllowWeakIndexing::yes> temp_range {
#if HOTENDS > 0
    TEMP_RANGE_INIT(0),
#endif
#if HOTENDS > 1
        TEMP_RANGE_INIT(1),
#endif
#if HOTENDS > 2
        TEMP_RANGE_INIT(2),
#endif
#if HOTENDS > 3
        TEMP_RANGE_INIT(3),
#endif
#if HOTENDS > 4
        TEMP_RANGE_INIT(4),
#endif
#if HOTENDS > 5
        TEMP_RANGE_INIT(5),
#endif
};
static_assert(HOTENDS <= 6);

#undef TEMP_RANGE_INIT

#if WATCH_HOTENDS
inline constexpr HeaterWatch::Config watch_hotend_config {
    .temp_increase = WATCH_TEMP_INCREASE,
    .period_s = WATCH_TEMP_PERIOD,
    .min_temp_diff = WATCH_TEMP_INCREASE + TEMP_HYSTERESIS + 1,
    .error_code = ErrCode::ERR_TEMPERATURE_HOTEND_PREHEAT_ERROR,
};

inline StrongIndexArray<HeaterWatchWithConfig<watch_hotend_config>, HOTENDS, PhysicalToolIndex, PhysicalToolIndex::to_raw_static, strong_index_array::AllowWeakIndexing::yes> watch_hotend;
#endif
