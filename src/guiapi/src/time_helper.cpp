#include "time_helper.hpp"
#include <ctime>

void format_duration(StringBuilder &sb, std::uint32_t duration, bool display_seconds) {
    static constexpr std::uint32_t SECONDS_PER_DAY = 24 * 60 * 60;
    struct tm tm_buffer;
    time_t time = (time_t)duration;

    const struct tm *timeinfo = gmtime_r(&time, &tm_buffer);
    [[maybe_unused]] int count = 0;
    if (timeinfo->tm_yday) {
        // days are recalculated, because timeinfo shows number of days in year and we want more days than 365
        uint16_t days = duration / SECONDS_PER_DAY;
        sb.append_printf("%ud %uh", days, timeinfo->tm_hour);
    } else if (timeinfo->tm_hour) {
        sb.append_printf("%ih %2im", timeinfo->tm_hour, timeinfo->tm_min);
    } else if (display_seconds) {
        if (timeinfo->tm_min) {
            sb.append_printf("%im %2is", timeinfo->tm_min, timeinfo->tm_sec);
        } else {
            sb.append_printf("%is", timeinfo->tm_sec);
        }
    } else {
        sb.append_printf("%im", timeinfo->tm_min);
    }
}
