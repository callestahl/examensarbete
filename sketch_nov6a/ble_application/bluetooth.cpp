#include "bluetooth.h"

#include <stdlib.h>

void bluetooth_reset(Bluetooth* bluetooth)
{
    bluetooth->reading_samples = false;
    bluetooth->header_read = false;
    bluetooth->table_index = 0;
    bluetooth->sample_index = 0;
}

BluetoothSampleProcessCode bluetooth_process_sample(Bluetooth* bluetooth, uint16_t sample, WaveTableOscillator* oscilator)
{
    if (bluetooth->table_index < oscilator->tables_capacity)
    {
        if (bluetooth->sample_index == 0)
        {
            if (oscilator->tables[bluetooth->table_index].samples != NULL)
            {
                free(oscilator->tables[bluetooth->table_index].samples);
            }
            oscilator->tables[bluetooth->table_index].samples = (uint16_t*)calloc(
                oscilator->samples_per_cycle, sizeof(uint16_t));
            if (oscilator->tables[bluetooth->table_index].samples != NULL)
            {
                oscilator->total_cycles++;
            }
            else
            {
                return SAMPLE_PROCESS_ERROR;
            }
        }
        oscilator->tables[bluetooth->table_index].samples[bluetooth->sample_index++] =
            sample;
        if (bluetooth->sample_index == oscilator->samples_per_cycle)
        {
            bluetooth->sample_index = 0;
            bluetooth->table_index++;

            return SAMPLE_PROCESS_CYCLE_DONE;
        }
    }
    return SAMPLE_PROCESS_OK;
}

