#include "ble.h"
#include "define.h"
#include "bluetooth.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

global BLECharacteristic* g_characteristic = NULL;

global bool g_device_connected = false;
global Bluetooth g_bluetooth = { 0 };
global bool g_header_read = false;
global uint32_t g_table_index = 0;
global uint32_t g_sample_index = 0;
global WaveTableOscillator* g_temp_osci = NULL;

global uint64_t g_read_bytes = 0;
global uint64_t g_bytes_to_read = 0;
global TaskHandle_t* g_task_notification_handle;

global Adafruit_SSD1351* g_display = NULL;

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer* server)
    {
        bluetooth_reset(&g_bluetooth);
        g_device_connected = true;
    }
    void onDisconnect(BLEServer* server)
    {
        bluetooth_reset(&g_bluetooth);
        g_device_connected = false;
        BLEDevice::startAdvertising();
    }
};

internal uint16_t ble_get_uint16(const uint8_t* data, uint32_t index)
{
    uint8_t high = data[index];
    uint8_t low = data[index + 1];
    return ((uint16_t)high << 8) | ((uint16_t)low);
}

void ble_read_header(uint8_t* data)
{
    g_read_bytes = 0;
    g_bytes_to_read = 0;

    uint16_t cycle_sample_count = ble_get_uint16(data, 8);
    g_temp_osci->samples_per_cycle = cycle_sample_count;
    g_temp_osci->total_cycles = 0;
    g_bluetooth.header_read = true;

    uint16_t bytes_to_received_high = ble_get_uint16(data, 10);
    uint16_t bytes_to_received_low = ble_get_uint16(data, 12);
    g_bytes_to_read = ((uint32_t)bytes_to_received_high << 16) |
                      (uint32_t)bytes_to_received_low;
}

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic* characteristic) override
    {
        uint8_t* data = characteristic->getData();
        size_t length = characteristic->getLength();

        if (length > 0)
        {
            g_bluetooth.reading_samples = true;

            uint32_t i = 0;
            if (!g_bluetooth.header_read)
            {
                ble_read_header(data);
                i = 14;
            }
            for (; i < length; i += 2)
            {
                uint16_t sample = ble_get_uint16(data, i);
                bluetooth_process_sample(&g_bluetooth, sample, g_temp_osci);
            }
        }
        g_read_bytes += length;
        if (g_read_bytes == g_bytes_to_read)
        {
            g_bluetooth.reading_samples = false;
            if (*g_task_notification_handle != NULL)
            {
                xTaskNotifyGive(*g_task_notification_handle);
            }
        }
    }
};

void ble_setup(const char* name, TaskHandle_t* task_notification_handle,
               WaveTableOscillator* oscillator)
{
    g_task_notification_handle = task_notification_handle;

    g_temp_osci = oscillator;

    BLEDevice::init(name);
    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(new MyServerCallbacks());

    BLEService* service = server->createService(SERVICE_UUID);

    g_characteristic = service->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    g_characteristic->setCallbacks(new MyCharacteristicCallbacks());

    service->start();

    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setScanResponse(true);
    BLEDevice::startAdvertising();
}

bool ble_device_is_connected(void)
{
    return g_device_connected;
}

bool ble_copy_transfer(WaveTableOscillator* oscillator, SemaphoreHandle_t mutex,
                       SemaphoreHandle_t mutex_screen)
{

    if ((!g_bluetooth.reading_samples || !g_device_connected) &&
        g_temp_osci->total_cycles != 0 && (g_read_bytes == g_bytes_to_read))
    {
#if 0
        if (xSemaphoreTake(mutex, portMAX_DELAY))
        {
            if (xSemaphoreTake(mutex_screen, portMAX_DELAY))
            {
                wave_table_oscilator_clean(oscillator);
                WaveTableOscillator temp = *oscillator;
                *oscillator = g_temp_osci;
                g_temp_osci = temp;
                xSemaphoreGive(mutex_screen);
            }
            xSemaphoreGive(mutex);
        }
#endif
        return true;
    }
    return false;
}
