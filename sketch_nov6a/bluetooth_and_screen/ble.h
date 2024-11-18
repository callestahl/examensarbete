#ifndef BLE_H
#define BLE_H
#include <stdint.h>
#include "wave_table.h"

void ble_setup(void* display);
bool ble_device_is_connected(void);
bool ble_copy_transfer(WaveTableOscillator* oscilator);

#endif
