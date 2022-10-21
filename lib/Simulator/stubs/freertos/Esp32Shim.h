#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t pvTaskCode, const char *const pcName,
                                   const uint32_t usStackDepth, void *const pvParameters,
                                   UBaseType_t uxPriority, TaskHandle_t *const pvCreatedTask,
                                   const BaseType_t xCoreID);
