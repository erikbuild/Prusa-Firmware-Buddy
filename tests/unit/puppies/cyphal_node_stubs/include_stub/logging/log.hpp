#pragma once

#include <cstdint>

namespace logging {

struct Timestamp {
    uint32_t sec; ///< Seconds since the start of the system
    uint32_t us; ///< Microseconds consistent with sec
};

enum class Severity {
    debug = 1,
    info = 2,
    warning = 3,
    error = 4,
    critical = 5
};

struct FormattedEvent {
    Timestamp timestamp;
    Severity severity;
};

} // namespace logging

#define LOG_COMPONENT_DEF(...)
#define LOG_COMPONENT_REF(...)

#define log_info(...)
#define log_warning(...)
#define log_error(...)

inline void log_format_honeybee_node(...) {}
