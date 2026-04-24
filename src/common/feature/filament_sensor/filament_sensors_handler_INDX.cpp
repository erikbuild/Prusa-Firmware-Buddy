/// @file
#include <feature/filament_sensor/filament_sensors_handler.hpp>
#include <feature/filament_sensor/filament_sensor_xbuddy_extension.hpp>

namespace {

FSensorXBuddyExtension *get_side_sensor_slot(uint8_t index) {
    static std::array<FSensorXBuddyExtension, PhysicalToolIndex::count> sensors = { {
        { FilamentSensorID { .position = FilamentSensorID::Position::side, .index = 0 }, FSensorXBuddyExtension::Source::ext },
        { FilamentSensorID { .position = FilamentSensorID::Position::side, .index = 1 }, FSensorXBuddyExtension::Source::ext },
        { FilamentSensorID { .position = FilamentSensorID::Position::side, .index = 2 }, FSensorXBuddyExtension::Source::ext },
        { FilamentSensorID { .position = FilamentSensorID::Position::side, .index = 3 }, FSensorXBuddyExtension::Source::ext },
        { FilamentSensorID { .position = FilamentSensorID::Position::side, .index = 4 }, FSensorXBuddyExtension::Source::ext },
        { FilamentSensorID { .position = FilamentSensorID::Position::side, .index = 5 }, FSensorXBuddyExtension::Source::ext },
        { FilamentSensorID { .position = FilamentSensorID::Position::side, .index = 6 }, FSensorXBuddyExtension::Source::ext },
        { FilamentSensorID { .position = FilamentSensorID::Position::side, .index = 7 }, FSensorXBuddyExtension::Source::ext },
    } };
    static_assert(PhysicalToolIndex::count == 8);
    if (index >= sensors.size()) {
        return nullptr;
    }
    return &sensors[index];
}

} // namespace

// function returning abstract sensor - used in higher level api
IFSensor *GetExtruderFSensor([[maybe_unused]] uint8_t index) {
    /// No extruder sensor on INDX. The sensors are side sensors (on the XBuddy Extension).
    return nullptr;
}

// function returning abstract sensor - used in higher level api
IFSensor *GetSideFSensor(uint8_t index) {
    if (index >= PhysicalToolIndex::count || !PhysicalToolIndex::from_raw(index).is_enabled()) {
        return nullptr;
    }
    return get_side_sensor_slot(index);
}

IFSensor *GetSideFSensorIgnoreEnabled(uint8_t index) {
    return get_side_sensor_slot(index);
}
