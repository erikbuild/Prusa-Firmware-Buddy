/// @file
#include "filament_sensor_states.hpp"

#include <feature/filament_sensor/filament_sensor.hpp>

bool is_fsensor_working_state(IFSensor *sensor) {
    return sensor && is_fsensor_working_state(sensor->get_state());
}
