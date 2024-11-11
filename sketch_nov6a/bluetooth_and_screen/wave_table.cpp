#include "wave_table.h"

uint16_t wave_table_oscilator_linear_interpolation(const WaveTable* table, uint32_t table_length, uint64_t phase) {
  const uint32_t phase_index = phase >> 32;
  const uint32_t phase_fraction = (phase & 0xFFFFFFFF) >> 16;
  const uint16_t sample0 = table->data[phase_index];
  const uint16_t sample1 = table->data[(phase_index + 1) % table_length];
  return lerp(sample0, sample1, phase_fraction);
}

void wave_table_oscilator_update_phase(WaveTableOscillator* oscilator) {
  oscilator->phase += oscilator->phase_increment;
  if (oscilator->phase >= ((uint64_t)oscilator->table_length << 32)) {
    oscilator->phase -= ((uint64_t)oscilator->table_length << 32);
  }
}
