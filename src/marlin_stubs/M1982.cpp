#include <option/has_indx.h>

#if HAS_INDX()

    #include "PrusaGcodeSuite.hpp"
    #include <feature/indx_dock_calibration/indx_dock_calibration.hpp>

void PrusaGcodeSuite::M1982() {
    indx_dock_calibration::run();
}

#endif
