#include "selftest_result.hpp"
#include <option/has_switched_fan_test.h>

TestResult SelftestResult::get_print_fan(uint8_t tool) const {
    return _tools[tool]._printFan;
}

void SelftestResult::set_print_fan(uint8_t tool, TestResult result) {
    _tools[tool]._printFan = result;
}

TestResult SelftestResult::get_heatbreak_fan(uint8_t tool) const {
    return _tools[tool]._heatBreakFan;
}

void SelftestResult::set_heatbreak_fan(uint8_t tool, TestResult result) {
    _tools[tool]._heatBreakFan = result;
}

TestResult SelftestResult::get_fans_switched(uint8_t tool) const {
    return _tools[tool]._fansSwitched;
}

void SelftestResult::set_fans_switched(uint8_t tool, TestResult result) {
    _tools[tool]._fansSwitched = result;
}

TestResult SelftestResult::evaluate_fans(uint8_t tool) const {
#if HAS_SWITCHED_FAN_TEST()
    return test_result::evaluate_results(_tools[tool]._printFan, _tools[tool]._heatBreakFan, _tools[tool]._fansSwitched);
#else
    return test_result::evaluate_results(_tools[tool]._printFan, _tools[tool]._heatBreakFan);
#endif
}

bool SelftestResult::has_heatbreak_fan_passed(uint8_t tool) const {
    return _tools[tool]._heatBreakFan == TestResult::passed
#if HAS_SWITCHED_FAN_TEST()
        && _tools[tool]._fansSwitched == TestResult::passed
#endif /* HAS_SWITCHED_FAN_TEST */
        ;
}

TestResult SelftestResult::get_nozzle_heater(uint8_t tool) const {
    return _tools[tool]._nozzle;
}

void SelftestResult::set_nozzle_heater(uint8_t tool, TestResult result) {
    _tools[tool]._nozzle = result;
}

TestResult SelftestResult::get_loadcell(uint8_t tool) const {
    return _tools[tool]._loadcell;
}

void SelftestResult::set_loadcell(uint8_t tool, TestResult result) {
    _tools[tool]._loadcell = result;
}

TestResult SelftestResult::get_dock_offset(uint8_t tool) const {
    return _tools[tool]._dockoffset;
}

void SelftestResult::set_dock_offset(uint8_t tool, TestResult result) {
    _tools[tool]._dockoffset = result;
}

TestResult SelftestResult::get_tool_offset(uint8_t tool) const {
    return _tools[tool]._tooloffset;
}

void SelftestResult::set_tool_offset(uint8_t tool, TestResult result) {
    _tools[tool]._tooloffset = result;
}

TestResult SelftestResult::get_gearbox(uint8_t tool) const {
    return _tools[tool]._gears;
}

void SelftestResult::set_gearbox(uint8_t tool, TestResult result) {
    _tools[tool]._gears = result;
}

TestResult SelftestResult::get_xaxis() const {
    return _xaxis;
}

void SelftestResult::set_xaxis(TestResult result) {
    _xaxis = result;
}

TestResult SelftestResult::get_yaxis() const {
    return _yaxis;
}

void SelftestResult::set_yaxis(TestResult result) {
    _yaxis = result;
}

TestResult SelftestResult::get_zaxis() const {
    return _zaxis;
}

void SelftestResult::set_zaxis(TestResult result) {
    _zaxis = result;
}

TestResult SelftestResult::get_bed_heater() const {
    return _bed;
}

void SelftestResult::set_bed_heater(TestResult result) {
    _bed = result;
}

TestResultNet SelftestResult::get_ethernet() const {
    return _eth;
}

void SelftestResult::set_ethernet(TestResultNet result) {
    _eth = result;
}

TestResultNet SelftestResult::get_wifi() const {
    return _wifi;
}

void SelftestResult::set_wifi(TestResultNet result) {
    _wifi = result;
}

TestResult SelftestResult::get_zalign() const {
    return _zalign;
}

void SelftestResult::set_zalign(TestResult result) {
    _zalign = result;
}

TestResult SelftestResult::get_deprecated_gears() const {
    return _deprecated_gears;
}
