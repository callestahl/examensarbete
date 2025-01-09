#ifndef SPP_H
#define SPP_H
#include "wave_table.h"
#include "bluetooth.h"
#include <stddef.h> // for size_t
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

void spp_setup(TaskHandle_t* task_notification_handle, uint32_t queue_size);
void spp_begin(const char* name);
void spp_end(void);
BluetoothCode spp_look_for_incoming_messages(WaveTableOscillator* oscilator);

#endif
