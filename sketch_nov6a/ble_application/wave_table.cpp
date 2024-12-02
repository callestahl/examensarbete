#include "wave_table.h"
#include "bluetooth.h"
#include <stdlib.h>
#include <SPIFFS.h>

uint16_t lerp(uint16_t a, uint16_t b, uint32_t fraction)
{
    return a + (((b - a) * fraction) >> 16);
}

uint16_t wave_table_linear_interpolation(const WaveTable* table,
                                         uint32_t table_length, uint64_t phase)
{
    const uint32_t phase_index = phase >> 32;
    const uint32_t phase_fraction = (phase & 0xFFFFFFFF) >> 16;
    const uint16_t sample0 = table->samples[phase_index];
    const uint16_t sample1 = table->samples[(phase_index + 1) % table_length];
    return lerp(sample0, sample1, phase_fraction);
}

void wave_table_oscilator_update_phase(WaveTableOscillator* oscillator)
{
    oscillator->phase += oscillator->phase_increment;
    if (oscillator->phase >= ((uint64_t)oscillator->samples_per_cycle << 32))
    {
        oscillator->phase -= ((uint64_t)oscillator->samples_per_cycle << 32);
    }
}

void wave_table_oscilator_clean(WaveTableOscillator* oscillator)
{
    oscillator->phase = 0;
    oscillator->phase_increment = 0;
    oscillator->samples_per_cycle = 0;
    if (oscillator->tables != NULL)
    {
        for (uint32_t i = 0; i < oscillator->total_cycles; ++i)
        {
            if (oscillator->tables[i].samples != NULL)
            {
                free(oscillator->tables[i].samples);
                oscillator->tables[i].samples = NULL;
            }
        }
    }
    oscillator->total_cycles = 0;
}

void wave_table_oscilator_write_to_file(const WaveTableOscillator* oscillator)
{
    File file = SPIFFS.open("/osci.txt", FILE_WRITE);

    if (file)
    {
        uint8_t cycle_sample_count_high =
            (uint8_t)((oscillator->samples_per_cycle >> 8) & 0xFF);
        uint8_t cycle_sample_count_low =
            (uint8_t)(oscillator->samples_per_cycle & 0xFF);
        file.write(cycle_sample_count_high);
        file.write(cycle_sample_count_low);

        for (uint32_t cycle = 0; cycle < oscillator->total_cycles; ++cycle)
        {
            for (uint32_t sample_index = 0;
                 sample_index < oscillator->samples_per_cycle; ++sample_index)
            {
                uint16_t sample =
                    oscillator->tables[cycle].samples[sample_index];
                uint8_t sample_high = (uint8_t)((sample >> 8) & 0xFF);
                uint8_t sample_low = (uint8_t)(sample & 0xFF);
                file.write(sample_high);
                file.write(sample_low);
            }
        }
        file.close();
    }
}

uint16_t file_get_uint16(File* file)
{
    uint8_t high = file->read();
    uint8_t low = file->read();
    return ((uint16_t)high << 8) | ((uint16_t)low);
}

void wave_table_oscilator_read_from_file(WaveTableOscillator* oscillator)
{
    File file = SPIFFS.open("/osci.txt", FILE_READ);

    if (file && file.available())
    {
        uint16_t cycle_sample_count = file_get_uint16(&file);
        oscillator->samples_per_cycle = cycle_sample_count;
        Bluetooth bluetooth = { 0 };
        while (file.available())
        {
            uint16_t sample = file_get_uint16(&file);
            bluetooth_process_sample(&bluetooth, sample, oscillator);
        }
        file.close();
    }
}
