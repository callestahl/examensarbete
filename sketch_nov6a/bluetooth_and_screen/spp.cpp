#include "spp.h"
#include "define.h"

#include "BluetoothSerial.h"

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
};

struct SPP
{
    BluetoothSerial bs;
    
    int32_t table_index;
    int32_t sample_index;

    bool initialized;
    bool header_read;
    bool reading_samples;

    uint64_t time_to_reset;
    uint64_t time_before_reset;
};

global WaveTableOscillator g_temp_osci = { 0 };
global SPP g_spp;

internal void spp_clear_buffer()
{
    while (g_spp.bs.available())
    {
        g_spp.bs.read();
    }
}

internal void spp_reset()
{
    g_spp.reading_samples = false;
    g_spp.header_read = false;
    g_spp.table_index = 0;
    g_spp.sample_index = 0;
}

internal bool spp_has_timed_out(uint64_t start_time, uint64_t time_to_timeout)
{
    if ((millis() - start_time) > time_to_timeout)
    {
        spp_reset();
        spp_clear_buffer();
        wave_table_oscilator_clean(&g_temp_osci);
        g_spp.bs.write(BLUETOOTH_ERROR_CODE);
        return true;
    }
    return false;
}

internal uint16_t spp_get_uint16()
{
    uint8_t high = g_spp.bs.read();
    uint8_t low = g_spp.bs.read();
    return ((uint16_t)high << 8) | ((uint16_t)low);
}

internal void spp_revert_transaction()
{
    spp_reset();
    spp_clear_buffer();
    wave_table_oscilator_clean(&g_temp_osci);
    g_spp.bs.write(BLUETOOTH_ERROR_CODE);
}

internal bool spp_read_header()
{
    uint16_t id0 = spp_get_uint16();
    uint16_t id1 = spp_get_uint16();
    uint16_t id2 = spp_get_uint16();
    uint16_t id3 = spp_get_uint16();
    if (id0 == 29960 && id1 == 62903 && id2 == 35185 && id3 == 26662)
    {
        uint16_t cycle_sample_count = spp_get_uint16();
        g_temp_osci.samples_per_cycle = cycle_sample_count;
        g_temp_osci.total_cycles = 0;
        g_spp.header_read = true;
        g_spp.table_index = 0;
        g_spp.sample_index = 0;
        g_spp.bs.write(BLUETOOTH_ACKNOWLEDGE_CODE);
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

internal bool spp_process_sample()
{
    uint16_t key = spp_get_uint16();
    uint16_t sample = spp_get_uint16();
    if (key == spp_sample_key(sample))
    {
        if (g_spp.table_index < g_temp_osci.tables_capacity)
        {
            if (g_spp.sample_index == 0)
            {
                if (g_temp_osci.tables[g_spp.table_index].samples != NULL)
                {
                    free(g_temp_osci.tables[g_spp.table_index].samples);
                }
                g_temp_osci.tables[g_spp.table_index].samples = (uint16_t*)calloc(
                    g_temp_osci.samples_per_cycle, sizeof(uint16_t));
                if (g_temp_osci.tables[g_spp.table_index].samples != NULL)
                {
                    g_temp_osci.total_cycles++;
                }
                else
                {
                    // TODO(Linus): Logging
                    spp_revert_transaction();
                    return false;
                }
            }
            g_temp_osci.tables[g_spp.table_index].samples[g_spp.sample_index++] =
                sample;
            if (g_spp.sample_index == g_temp_osci.samples_per_cycle)
            {
                g_spp.sample_index = 0;
                g_spp.table_index++;

                g_spp.bs.write(BLUETOOTH_ACKNOWLEDGE_CODE);
            }
        }
    }
    else
    {
        spp_revert_transaction();
        return false;
    }
    return true;
}

void spp_setup(const char* name)
{
    if (!g_spp.initialized)
    {
        g_spp.bs.begin(name);
        g_spp.bs.enableSSP();
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

bool spp_look_for_incoming_messages(WaveTableOscillator* oscilator)
{
    const uint64_t start_time = millis();
    const uint64_t time_to_timeout = 500;

    while (g_spp.bs.available() &&
           !spp_has_timed_out(start_time, time_to_timeout))
    {
        if (!g_spp.header_read)
        {
            if (g_spp.bs.available() >= sizeof(BluetoothHeader))
            {
                if (!spp_read_header())
                {
                    break;
                }
            }
        }
        else
        {
            if (g_spp.bs.available() >= 4)
            {
                if (!spp_process_sample())
                {
                    break;
                }
            }
            g_spp.reading_samples = true;
        }
        g_spp.time_to_reset = millis() + g_spp.time_before_reset;
    }

    if (g_spp.reading_samples)
    {
        if (millis() >= g_spp.time_to_reset)
        {
            spp_reset();
            g_spp.bs.write(BLUETOOTH_FINISHED_CODE);
            if (g_temp_osci.total_cycles > 0)
            {
                wave_table_oscilator_clean(oscilator);
                WaveTable* temp = oscilator->tables;
                *oscilator = g_temp_osci;
                g_temp_osci.tables = temp;
                return true;
            }
        }
    }
    return false;
}
