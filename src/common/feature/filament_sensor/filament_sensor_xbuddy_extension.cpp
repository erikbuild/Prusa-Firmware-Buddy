/// @file
#include "filament_sensor_xbuddy_extension.hpp"

#include <feature/xbuddy_extension/xbuddy_extension.hpp>
#include <config_store/store_instance.hpp>
#include <option/has_side_fsensor.h>

FSensorXBuddyExtension::FSensorXBuddyExtension(FilamentSensorID id, Source source)
    : IFSensor(id)
    , source_(source) {}

bool FSensorXBuddyExtension::is_calibrated() const {
    return get_selftest_result() == TestResult::passed;
}

TestResult FSensorXBuddyExtension::get_selftest_result() const {
#if HAS_SIDE_FSENSOR()
    return config_store().selftest_result_side_fsensor.get(id_.index);
#else
    return TestResult::passed;
#endif
}

void FSensorXBuddyExtension::cycle() {
    state = interpret_state();
}

int32_t FSensorXBuddyExtension::GetFilteredValue() const {
    return static_cast<int32_t>(raw_state_);
}

FilamentSensorState FSensorXBuddyExtension::interpret_state() const {
    switch (buddy::xbuddy_extension().status()) {

    case buddy::XBuddyExtension::Status::disabled:
        return FilamentSensorState::Disabled;

    case buddy::XBuddyExtension::Status::not_connected:
        return FilamentSensorState::NotConnected;

    case buddy::XBuddyExtension::Status::ready:
        // Continue
        break;
    }

    std::optional<buddy::XBuddyExtension::FilamentSensorState> hw_state;
    switch (source_) {

    case Source::gpio:
        hw_state = buddy::xbuddy_extension().gpio_filament_sensor();
        break;

    case Source::ext:
        hw_state = buddy::xbuddy_extension().ext_filament_sensor(id_.index);
        break;
    }

    raw_state_ = hw_state.value_or(buddy::XBuddyExtension::FilamentSensorState::uninitialized);

    switch (raw_state_) {

    case buddy::XBuddyExtension::FilamentSensorState::disconnected:
        return FilamentSensorState::NotConnected;

    case buddy::XBuddyExtension::FilamentSensorState::uninitialized:
        return FilamentSensorState::NotInitialized;

    case buddy::XBuddyExtension::FilamentSensorState::has_filament:
        return FilamentSensorState::HasFilament;

    case buddy::XBuddyExtension::FilamentSensorState::no_filament:
        return FilamentSensorState::NoFilament;
    }

    return FilamentSensorState::NotInitialized;
}
