#include "wave_table.h"
#include <stdlib.h>

uint16_t lerp(uint16_t a, uint16_t b, uint32_t fraction) {
  return a + (((b - a) * fraction) >> 16);
}

uint16_t wave_table_linear_interpolation(const WaveTable* table, uint32_t table_length, uint64_t phase) {
  const uint32_t phase_index = phase >> 32;
  const uint32_t phase_fraction = (phase & 0xFFFFFFFF) >> 16;
  const uint16_t sample0 = table->samples[phase_index];
  const uint16_t sample1 = table->samples[(phase_index + 1) % table_length];
  return lerp(sample0, sample1, phase_fraction);
}

void wave_table_oscilator_update_phase(WaveTableOscillator* oscilator) {
  oscilator->phase += oscilator->phase_increment;
  if (oscilator->phase >= ((uint64_t)oscilator->samples_per_cycle << 32)) {
    oscilator->phase -= ((uint64_t)oscilator->samples_per_cycle << 32);
  }
}

void wave_table_oscilator_clean(WaveTableOscillator* oscilator)
{
    oscilator->phase = 0;
    oscilator->phase_increment = 0;
    oscilator->samples_per_cycle = 0;
    oscilator->total_cycles = 0;
    if (oscilator->tables != NULL)
    {
        for (uint32_t i = 0; i < oscilator->total_cycles; ++i)
        {
            if (oscilator->tables[i].samples != NULL)
            {
                free(oscilator->tables[i].samples);
            }
        }
    }
}
