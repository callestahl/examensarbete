#include "ble.h"
#include "define.h"
#include "oled_screen.h"

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

global BLECharacteristic* g_characteristic = NULL;

global bool g_device_connected = false;
global String g_data;

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer* server)
    {
        oled_debug();
        oled()->printf("Connected\n");
        g_device_connected = true;
    }
    void onDisconnect(BLEServer* server)
    {
        oled_debug();
        oled()->printf("Disconnected\n");
        g_device_connected = false;
    }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic* characteristic) override
    {
        g_data = characteristic->getValue();

        if (!g_data.isEmpty())
        {
            oled_debug();
            oled()->printf("%s\n", g_data.c_str());
        }
    }
};

void ble_setup(void)
{
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

String ble_get_data(void)
{
    return g_data;
}
