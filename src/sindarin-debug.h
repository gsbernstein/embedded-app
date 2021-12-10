#ifndef SINDARIN_DEBUG_H
#define SINDARIN_DEBUG_H

#include <Arduino.h>

#define DEBUG_VAR(x) log_d("%s", String(x).c_str())
#define DEBUG_LN(y) log_d("%s", String(y).c_str())

#endif

#define SHOW_HEAP()                                                                                \
    log_d("Free heap: %dk/%dk Free PSRAM: %dk/%dk", ESP.getFreeHeap() / 1000,                      \
          ESP.getHeapSize() / 1000, ESP.getFreePsram() / 1000, ESP.getPsramSize() / 1000)

#define SIZE_OF(obj) log_d("SIZE_OF(%s): %d", #obj, sizeof(obj))
