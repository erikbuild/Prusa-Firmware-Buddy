#include "selftest_result.hpp"
#include <option/has_switched_fan_test.h>

bool SelftestTool::has_heatbreak_fan_passed() {
    return heatBreakFan == TestResult_Passed
#if HAS_SWITCHED_FAN_TEST()
        && fansSwitched == TestResult_Passed
#endif /* HAS_SWITCHED_FAN_TEST */
        ;
}

TestResult SelftestTool::evaluate_fans() {
    if (printFan == TestResult_Passed && heatBreakFan == TestResult_Passed
#if HAS_SWITCHED_FAN_TEST()
        && fansSwitched == TestResult_Passed
#endif /* HAS_SWITCHED_FAN_TEST() */
    ) {
        return TestResult_Passed;
    }

    if (printFan == TestResult_Failed || heatBreakFan == TestResult_Failed
#if HAS_SWITCHED_FAN_TEST()
        || fansSwitched == TestResult_Failed
#endif /* HAS_SWITCHED_FAN_TEST() */
    ) {
        return TestResult_Failed;
    }

    if (printFan == TestResult_Skipped || heatBreakFan == TestResult_Skipped
#if HAS_SWITCHED_FAN_TEST()
        || fansSwitched == TestResult_Skipped
#endif /* HAS_SWITCHED_FAN_TEST() */
    ) {
        return TestResult_Skipped;
    }

    return TestResult_Unknown;
}
