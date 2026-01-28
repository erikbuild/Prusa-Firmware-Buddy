/// @file
#include "marlin_temptable.hpp"

float marlin_temptable_lookup(MarlinTempTable temp_table, int16_t raw) {
    const auto TBL = temp_table.data();
    const auto LEN = temp_table.size();

    // Ugly code stolen from temperature.cpp
    // One day, we want to get rid of it, but it's still used on several places
    /**
     * Bisect search for the range of the 'raw' value, then interpolate
     * proportionally between the under and over values.
     */
    uint8_t l = 0,
            r = LEN, m;
    for (;;) {
        m = (l + r) >> 1;
        if (!m) {
            return TBL[0][1];
        }
        if (m == l || m == r) {
            return TBL[LEN - 1][1];
        }
        short v00 = TBL[m - 1][0],
              v10 = TBL[m - 0][0];
        if (raw < v00) {
            r = m;
        } else if (raw > v10) {
            l = m;
        } else {
            const short v01 = TBL[m - 1][1],
                        v11 = TBL[m - 0][1];
            return v01 + (raw - v00) * float(v11 - v01) / float(v10 - v00);
        }
    }
}
