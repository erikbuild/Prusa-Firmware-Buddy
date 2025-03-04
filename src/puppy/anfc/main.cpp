#include "hal.hpp"
#include "nfc.hpp"

extern "C" int main() {
    hal::init();
    nfc::init();

    while (true) {
        hal::status_led_on();
        hal::delay(1'000);
        hal::status_led_off();
        hal::delay(1'000);
    }

    hal::panic();
}
