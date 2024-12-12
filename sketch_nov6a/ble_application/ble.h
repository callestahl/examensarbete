#ifndef BLE_H
#define BLE_H
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "wave_table.h"

void ble_setup(const char* name, TaskHandle_t* task_notification_handle, WaveTableOscillator* oscillator);
bool ble_device_is_connected(void);
bool ble_copy_transfer(WaveTableOscillator* oscillator, SemaphoreHandle_t mutex, SemaphoreHandle_t mutex_screen);

#endif
