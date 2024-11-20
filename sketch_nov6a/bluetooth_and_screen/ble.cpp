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
global WaveTableOscillator g_temp_osci = { 0 };

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
        g_bluetooth.reading_samples = false;
    }
};

internal uint16_t ble_get_uint16(const uint8_t* data, uint32_t index)
{
    uint8_t high = data[index];
    uint8_t low = data[index + 1];
    return ((uint16_t)high << 8) | ((uint16_t)low);
}

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic* characteristic) override
    {
        String message = characteristic->getValue();
        if(message.equals("DONE"))
        {
            g_bluetooth.reading_samples = false;
            return;
        }
        uint8_t* data = characteristic->getData();
        size_t length = characteristic->getLength();

        if (length > 0)
        {
            g_bluetooth.reading_samples = true;

            uint32_t i = 0;
            if (!g_bluetooth.header_read)
            {
                i = 8;
                uint16_t cycle_sample_count = ble_get_uint16(data, i);
                g_temp_osci.samples_per_cycle = cycle_sample_count;
                g_temp_osci.total_cycles = 0;
                g_bluetooth.header_read = true;
                i = 14;
            }
            for (; i < length; i += 2)
            {
                uint16_t sample = ble_get_uint16(data, i);
                bluetooth_process_sample(&g_bluetooth, sample, &g_temp_osci);
            }
        }
    }
};

void ble_setup(void* display)
{
    g_display = (Adafruit_SSD1351*)display;

    g_temp_osci.tables_capacity = 256;
    g_temp_osci.tables =
        (WaveTable*)calloc(g_temp_osci.tables_capacity, sizeof(WaveTable));

    BLEDevice::init("WaveTablePP");
    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(new MyServerCallbacks());

    BLEService* service = server->createService(SERVICE_UUID);

    g_characteristic = service->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    g_characteristic->setCallbacks(new MyCharacteristicCallbacks());

    service->start();

#if 1
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
#endif
}

bool ble_device_is_connected(void)
{
    return g_device_connected;
}

bool ble_copy_transfer(WaveTableOscillator* oscilator)
{
    if ((!g_bluetooth.reading_samples || !g_device_connected) && g_temp_osci.total_cycles != 0)
    {
        wave_table_oscilator_clean(oscilator);
        WaveTableOscillator temp = *oscilator;
        *oscilator = g_temp_osci;
        g_temp_osci = temp;
        return true;
    }
    return false;
}
