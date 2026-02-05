/// \file
#pragma once

struct FilamentSensorID {

public:
    enum class Position : uint8_t {
        extruder,
        side
    };

public:
    Position position : 4;
    uint8_t index : 4;
};

static_assert(sizeof(FilamentSensorID) == 1);
