#include "selftest_dependencies.hpp"

#if HAS_SELFTEST_DEPENDENCIES()
    #include <string_builder.hpp>
    #include <window_msgbox.hpp>
#endif

namespace SelftestSnake {

bool is_completed(TestResult test_result) {
    // Skipped is also considered completed - it marks non-obligatory tests that have been explicitly skipped by the user
    return test_result == TestResult::passed || test_result == TestResult::skipped;
}

#if HAS_SELFTEST_DEPENDENCIES()

bool are_dependencies_met(Action action) {
    const auto dependencies = get_dependencies(action);
    for (Action dependency : valid_actions()) {
        if (!dependencies.test(dependency)) {
            continue;
        }
        if (!is_completed(get_test_result(dependency, AllTools {}))) {
            return false;
        }
    }
    return true;
}

bool are_all_actions_completed() {
    for (Action action : valid_actions()) {
        if (!is_completed(get_test_result(action, AllTools {}))) {
            return false;
        }
    }
    return true;
}

void show_unmet_dependencies_warning(Action action) {
    constexpr int msg_size = 2 * (sizeof("Complete these calibrations first:") + 4 * sizeof("Filament Sensor Calibration"));
    char msg[msg_size];
    StringBuilder sb(msg);
    sb.append_string_view(_("Complete these calibrations first:"));
    const auto dependencies = get_dependencies(action);
    for (Action dependency : valid_actions()) {
        if (!dependencies.test(dependency)) {
            continue;
        }
        if (!is_completed(get_test_result(dependency, AllTools {}))) {
            sb.append_printf("\n- ");
            sb.append_string_view(_(get_action_label(dependency)));
        }
    }
    MsgBoxWarning(string_view_utf8::MakeRAM(msg), Responses_Ok);
}

#else

bool are_previous_completed(Action action) {
    for (Action act : valid_actions()) {
        if (act == action) {
            break;
        }
        if (!is_completed(get_test_result(act, AllTools {}))) {
            return false;
        }
    }

    return true;
}

#endif

}; // namespace SelftestSnake
