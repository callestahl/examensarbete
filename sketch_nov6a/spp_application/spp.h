#ifndef SPP_H
#define SPP_H
#include "wave_table.h"
#include <stddef.h> // for size_t
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

void spp_setup(const char* name, TaskHandle_t* task_notification_handle, uint32_t queue_size);
bool spp_look_for_incoming_messages(WaveTableOscillator* oscilator, SemaphoreHandle_t mutex);

#endif
