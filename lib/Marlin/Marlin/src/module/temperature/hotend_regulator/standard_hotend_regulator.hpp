/// @file
#pragma once

#include <cstdint>

#include <inc/MarlinConfig.h>
#include <module/temperature/temp_defines.hpp>

class StandardHotendRegulator {

public:
    float get_pid_output_hotend(
        const uint8_t e);

private:
    hotend_pid_t work_pid;
    float temp_iState = 0;
    float temp_dState = 0;
    bool pid_reset = false;
};
