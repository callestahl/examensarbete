#ifndef BLE_H
#define BLE_H
#include <stdint.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

void ble_setup(void);
String ble_get_data(void);
bool ble_device_is_connected(void);

#endif
