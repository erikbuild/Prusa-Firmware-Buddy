#include <freertos/task.hpp>
#include <FreeRTOS.h>

namespace freertos {

bool is_inside_interrupt() {
    return xPortIsInsideInterrupt();
}

} // namespace freertos
