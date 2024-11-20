#include "server.h"
#include "bluetooth.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>

#include <WiFi.h>

struct ClientHeader
{
    uint16_t id0;
    uint16_t id1;
    uint16_t id2;
    uint16_t id3;
    uint16_t cycle_sample_count;
    uint16_t total_bytes_to_receive_high;
    uint16_t total_bytes_to_receive_low;
};

struct ServerData
{
    Bluetooth bluetooth;
    uint32_t bytes_received;
    uint32_t bytes_to_received;
};

const char* ssid = "ASUS_C8_2G";
const char* password = "Niftylotus18";

WaveTableOscillator g_temp_osci = { 0 };
uint32_t g_chunk_bytes = 0;
ServerData g_server_data = { 0 };

WiFiServer server(80);

String header;

uint16_t server_get_uint16(WiFiClient* client)
{
    g_chunk_bytes += 2;
    g_server_data.bytes_received += 2;
    uint8_t high = client->read();
    uint8_t low = client->read();
    return ((uint16_t)high << 8) | ((uint16_t)low);
}

bool server_read_header(WiFiClient* client)
{
    g_chunk_bytes = 0;
    g_server_data.bytes_received = 0;
    uint16_t id0 = server_get_uint16(client);
    uint16_t id1 = server_get_uint16(client);
    uint16_t id2 = server_get_uint16(client);
    uint16_t id3 = server_get_uint16(client);
    if (id0 == 29960 && id1 == 62903 && id2 == 35185 && id3 == 26662)
    {
        uint16_t cycle_sample_count = server_get_uint16(client);
        g_temp_osci.samples_per_cycle = cycle_sample_count;
        g_temp_osci.total_cycles = 0;
        g_server_data.bluetooth.header_read = true;
        g_server_data.bluetooth.table_index = 0;
        g_server_data.bluetooth.sample_index = 0;

        uint16_t bytes_to_received_high = server_get_uint16(client);
        uint16_t bytes_to_received_low = server_get_uint16(client);
        g_server_data.bytes_to_received =
            ((uint32_t)bytes_to_received_high << 16) |
            (uint32_t)bytes_to_received_low;

        // g_server_data.bluetooth_serial.write(BLUETOOTH_ACKNOWLEDGE_CODE);
    }
    else
    {
        return false;
    }
    return true;
}

bool server_process_sample(WiFiClient* client)
{
    uint16_t sample = server_get_uint16(client);

    BluetoothSampleProcessCode code = bluetooth_process_sample(
        &g_server_data.bluetooth, sample, &g_temp_osci);
    if (code == SAMPLE_PROCESS_ERROR)
    {
        return false;
    }
    else if (g_chunk_bytes == 4096)
    {
        // g_spp.bluetooth_serial.write(BLUETOOTH_ACKNOWLEDGE_CODE);
        g_chunk_bytes = 0;
    }
    return true;
}

void server_setup(void* display)
{
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
    }

    server.begin();
}

bool server_loop(WaveTableOscillator* oscilator)
{
#if 0
    WiFiClient client = server.available();

    if (client)
    {
        while (client.connected())

        {
            if (!g_server_data.bluetooth.header_read)
            {
                if (client.available() >= sizeof(ClientHeader))
                {
                    if (!server_read_header(&client))
                    {
                        break;
                    }
                }
            }
            else
            {
                if (client.available() >= 2)
                {
                    if (!server_process_sample(&client))
                    {
                        break;
                    }
                }
                g_server_data.bluetooth.reading_samples = true;
            }
        }
    }

    if (g_server_data.bluetooth.reading_samples)
    {
        if (g_server_data.bytes_received == g_server_data.bytes_to_received)
        {
            g_chunk_bytes = 0;
            g_server_data.bytes_received = 0;
            g_server_data.bytes_to_received = 0;
            bluetooth_reset(&g_server_data.bluetooth);
            //g_server_data.bluetooth_serial.write(BLUETOOTH_FINISHED_CODE);
            if (g_temp_osci.total_cycles > 0)
            {
                wave_table_oscilator_clean(oscilator);
                WaveTableOscillator temp = *oscilator;
                *oscilator = g_temp_osci;
                g_temp_osci = temp;
                return true;
            }
        }
    }
#endif
    return false;
}
