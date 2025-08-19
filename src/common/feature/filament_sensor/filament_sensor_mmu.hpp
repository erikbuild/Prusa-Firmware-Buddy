/**
 * @file filament_sensor_mmu.hpp
 * @brief clas representing filament sensor of MMU
 */

#pragma once

#include <feature/filament_sensor/filament_sensor.hpp>

class FSensorMMU : public IFSensor {
protected:
    virtual void cycle() override;
};
