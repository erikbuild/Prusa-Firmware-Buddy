#pragma once

namespace nfc {
void init();
void irq();
void tick();
void task(void *);
} // namespace nfc
