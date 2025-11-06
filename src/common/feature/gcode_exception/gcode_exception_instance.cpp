#include "gcode_exception.hpp"

GCodeExceptionManager &gcode_exceptions() {
    static GCodeExceptionManager instance;
    return instance;
}
