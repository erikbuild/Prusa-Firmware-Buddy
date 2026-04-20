#include <option/has_indx.h>

#if HAS_INDX()

    #include "PrusaGcodeSuite.hpp"
    #include <feature/indx_nozzle_cleaner_calibration/indx_nozzle_cleaner_calibration.hpp>

void PrusaGcodeSuite::M1983() {
    indx_nozzle_cleaner_calibration::run();
}

#endif
