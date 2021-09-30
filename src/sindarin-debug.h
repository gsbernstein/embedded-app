#ifndef SINDARIN_DEBUG_H
#define SINDARIN_DEBUG_H

#include <Arduino.h>

#define DEBUG_VAR(x) if (Serial) Serial.print(x)
#define DEBUG_LN(y) if (Serial) Serial.println(y)
#define DEBUG_F(format, ...) \
    if (Serial)           \
    Serial.printf(format, ##__VA_ARGS__)

#endif

#define SHOW_HEAP() DEBUG_F("%s:%d Free heap: %dk/%dk Free PSRAM: %dk/%dk\n", __FILE__, __LINE__, ESP.getFreeHeap() / 1000, ESP.getHeapSize() / 1000, ESP.getFreePsram() / 1000, ESP.getPsramSize() / 1000)

#define SIZE_OF(obj) DEBUG_F("%s:%d SIZE_OF(%s): %d\n",  __FILE__, __LINE__, #obj, sizeof(obj))
