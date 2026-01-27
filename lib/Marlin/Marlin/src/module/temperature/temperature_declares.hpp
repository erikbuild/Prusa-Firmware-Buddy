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
    int16_t mintemp = 0, maxtemp = 0;
    MarlinTemptableRawMinMax raw;

    temp_range_t() = default;

    temp_range_t(MarlinTempTable temptable, int16_t mintemp, int16_t maxtemp)
        : mintemp(mintemp)
        , maxtemp(maxtemp)
        , raw(MarlinTemptableRawMinMax::compute(temptable, mintemp, maxtemp)) {}
};

inline StrongIndexArray<temp_range_t, HOTENDS, PhysicalToolIndex, PhysicalToolIndex::to_raw_static, strong_index_array::AllowWeakIndexing::yes> temp_range;

#if WATCH_HOTENDS
inline constexpr HeaterWatch::Config watch_hotend_config {
    .temp_increase = WATCH_TEMP_INCREASE,
    .period_s = WATCH_TEMP_PERIOD,
    .min_temp_diff = WATCH_TEMP_INCREASE + TEMP_HYSTERESIS + 1,
    .error_code = ErrCode::ERR_TEMPERATURE_HOTEND_PREHEAT_ERROR,
};

inline StrongIndexArray<HeaterWatchWithConfig<watch_hotend_config>, HOTENDS, PhysicalToolIndex, PhysicalToolIndex::to_raw_static, strong_index_array::AllowWeakIndexing::yes> watch_hotend;
#endif
