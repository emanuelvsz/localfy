#include "core/Lock.h"

#include <Arduino.h>

namespace {
SemaphoreHandle_t s_mutex = nullptr;
}

void lock::begin() { s_mutex = xSemaphoreCreateMutex(); }

bool lock::tryTake(uint32_t timeoutMs) {
  return xSemaphoreTake(s_mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void lock::give() { xSemaphoreGive(s_mutex); }

Guard::Guard() { xSemaphoreTake(s_mutex, portMAX_DELAY); }
Guard::~Guard() { xSemaphoreGive(s_mutex); }
