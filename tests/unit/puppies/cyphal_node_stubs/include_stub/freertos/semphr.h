#pragma once

#include "FreeRTOS.h"

struct StaticSemaphore_t {};

inline StaticSemaphore_t semaphore;

using SemaphoreHandle_t = StaticSemaphore_t *;

inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(...) {
    return &semaphore;
}

inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutexStatic(...) {
    return &semaphore;
}

inline SemaphoreHandle_t xSemaphoreCreateBinaryStatic(...) {
    return &semaphore;
}

inline bool xSemaphoreTake(SemaphoreHandle_t, TickType_t) {
    return true;
}

inline bool xSemaphoreTakeRecursive(...) {
    return true;
}

inline bool xSemaphoreGive(...) {
    return true;
}

inline bool xSemaphoreGiveRecursive(...) {
    return true;
}
