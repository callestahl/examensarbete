#include "spp.h"
#include "define.h"
#include "bluetooth.h"

#include "BluetoothSerial.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>

#define BLUETOOTH_FINISHED_CODE 0x06
#define BLUETOOTH_ACKNOWLEDGE_CODE 0x08
#define BLUETOOTH_ERROR_CODE 0x10

struct BluetoothHeader
{
    uint16_t id0;
    uint16_t id1;
    uint16_t id2;
    uint16_t id3;
    uint16_t cycle_sample_count;
    uint16_t total_bytes_to_receive_high;
    uint16_t total_bytes_to_receive_low;
};

struct SPP
{
    BluetoothSerial bluetooth_serial;
    Bluetooth bluetooth;

    bool initialized;
    uint32_t bytes_received;
    uint32_t bytes_to_received;
    uint64_t time_to_reset;
    uint64_t time_before_reset;
};

global WaveTableOscillator g_temp_osci = { 0 };
global SPP g_spp;
global uint32_t g_chunk_bytes = 0;

internal void spp_clear_buffer()
{
    while (g_spp.bluetooth_serial.available())
    {
        g_spp.bluetooth_serial.read();
    }
}

internal bool spp_has_timed_out(uint64_t start_time, uint64_t time_to_timeout)
{
    if ((millis() - start_time) > time_to_timeout)
    {
        g_chunk_bytes = 0;
        g_spp.bytes_received = 0;
        g_spp.bytes_to_received = 0;
        bluetooth_reset(&g_spp.bluetooth);
        spp_clear_buffer();
        wave_table_oscilator_clean(&g_temp_osci);
        g_spp.bluetooth_serial.write(BLUETOOTH_ERROR_CODE);
        return true;
    }
    return false;
}


internal uint16_t spp_get_uint16()
{
    g_chunk_bytes += 2;
    g_spp.bytes_received += 2;
    uint8_t high = g_spp.bluetooth_serial.read();
    uint8_t low = g_spp.bluetooth_serial.read();
    return ((uint16_t)high << 8) | ((uint16_t)low);
}

internal void spp_revert_transaction()
{
    g_chunk_bytes = 0;
    g_spp.bytes_received = 0;
    g_spp.bytes_to_received = 0;
    bluetooth_reset(&g_spp.bluetooth);
    spp_clear_buffer();
    wave_table_oscilator_clean(&g_temp_osci);
    g_spp.bluetooth_serial.write(BLUETOOTH_ERROR_CODE);
}

internal bool spp_read_header()
{
    g_chunk_bytes = 0;
    g_spp.bytes_received = 0;
    uint16_t id0 = spp_get_uint16();
    uint16_t id1 = spp_get_uint16();
    uint16_t id2 = spp_get_uint16();
    uint16_t id3 = spp_get_uint16();
    if (id0 == 29960 && id1 == 62903 && id2 == 35185 && id3 == 26662)
    {
        uint16_t cycle_sample_count = spp_get_uint16();
        g_temp_osci.samples_per_cycle = cycle_sample_count;
        g_temp_osci.total_cycles = 0;
        g_spp.bluetooth.header_read = true;
        g_spp.bluetooth.table_index = 0;
        g_spp.bluetooth.sample_index = 0;

        uint16_t bytes_to_received_high = spp_get_uint16();
        uint16_t bytes_to_received_low = spp_get_uint16();
        g_spp.bytes_to_received = ((uint32_t)bytes_to_received_high << 16) |
                                  (uint32_t)bytes_to_received_low;

        // g_spp.bluetooth_serial.write(BLUETOOTH_ACKNOWLEDGE_CODE);
    }
    else
    {
        spp_revert_transaction();
        return false;
    }
    return true;
}

internal uint16_t spp_sample_key(uint16_t key)
{
    key = ~key + (key << 15);
    key = key ^ (key >> 12);
    key = key + (key << 2);
    key = key ^ (key >> 4);
    key = key * 2057;
    key = key ^ (key >> 16);
    return key;
}

global uint32_t te = 0;

internal bool spp_process_sample(void* display)
{
    // uint16_t key = spp_get_uint16();
    uint16_t sample = spp_get_uint16();
    // if (key == spp_sample_key(sample))
    {
        BluetoothSampleProcessCode code =
            bluetooth_process_sample(&g_spp.bluetooth, sample, &g_temp_osci);
        if (code == SAMPLE_PROCESS_ERROR)
        {
            return false;
        }
        // else if (code == SAMPLE_PROCESS_CYCLE_DONE)
        else if (g_chunk_bytes == 4096)
        {
            g_spp.bluetooth_serial.write(BLUETOOTH_ACKNOWLEDGE_CODE);
            g_chunk_bytes = 0;
        }
        return true;
    }
    // return false;
}

void spp_setup(const char* name)
{
    if (!g_spp.initialized)
    {
        g_spp.bluetooth_serial.begin(name);
        g_spp.bluetooth_serial.enableSSP();
        g_spp.initialized = true;
        g_spp.time_before_reset = 100;
        g_temp_osci.tables_capacity = 256;
        g_temp_osci.tables =
            (WaveTable*)calloc(g_temp_osci.tables_capacity, sizeof(WaveTable));
    }
    else
    {
        // TODO(Linus): Logging
    }
}

bool spp_look_for_incoming_messages(WaveTableOscillator* oscilator,
                                    void* display)
{
    const uint64_t start_time = millis();
    const uint64_t time_to_timeout = 500;

    while (g_spp.bluetooth_serial.available() &&
           !spp_has_timed_out(start_time, time_to_timeout))
    {
        if (!g_spp.bluetooth.header_read)
        {
            if (g_spp.bluetooth_serial.available() >= sizeof(BluetoothHeader))
            {
                if (!spp_read_header())
                {
                    break;
                }
            }
        }
        else
        {
            if (g_spp.bluetooth_serial.available() >= 2)
            {
                if (!spp_process_sample(display))
                {
                    spp_revert_transaction();
                    break;
                }
            }
            g_spp.bluetooth.reading_samples = true;
        }
        g_spp.time_to_reset = millis() + g_spp.time_before_reset;
    }

    if (g_spp.bluetooth.reading_samples)
    {
        // if (millis() >= g_spp.time_to_reset)
        if (g_spp.bytes_received == g_spp.bytes_to_received)
        {
            g_chunk_bytes = 0;
            g_spp.bytes_received = 0;
            g_spp.bytes_to_received = 0;
            bluetooth_reset(&g_spp.bluetooth);
            g_spp.bluetooth_serial.write(BLUETOOTH_FINISHED_CODE);
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
    return false;
}
