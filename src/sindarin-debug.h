#ifndef SINDARIN_DEBUG_H
#define SINDARIN_DEBUG_H

#include <Arduino.h>

#define DEBUG_VAR(x) if (Serial) Serial.print(x)
#define DEBUG_LN(y) if (Serial) Serial.println(y)
#define DEBUG_F(vargs...) \
    if (Serial)           \
    Serial.printf(vargs)

#endif

#define SHOW_HEAP() DEBUG_F("%s:%d Free heap: %dk/%dk Free PSRAM: %dk/%dk\n", __FILE__, __LINE__, ESP.getFreeHeap() / 1000, ESP.getHeapSize() / 1000, ESP.getFreePsram() / 1000, ESP.getPsramSize() / 1000)