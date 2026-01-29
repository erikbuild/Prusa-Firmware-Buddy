/// @file
#pragma once

class HeatbreakRegulator {

public:
    /// Steps the regulator.
    /// @returns the new PWM of the heatbreak fan
    float step();
};
