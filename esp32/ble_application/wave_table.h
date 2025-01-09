#ifndef WAVE_TABLE_H
#define WAVE_TABLE_H
#include <stdint.h>

struct WaveTable {
    uint16_t* samples;
};

struct WaveTableOscillator {
    uint64_t phase;
    uint64_t phase_increment;

    uint32_t samples_per_cycle;
    uint32_t tables_capacity;
    uint32_t total_cycles;
    WaveTable* tables;
};

uint16_t wave_table_linear_interpolation(const WaveTable* table, uint32_t table_length, uint64_t phase);
void wave_table_oscilator_update_phase(WaveTableOscillator* oscilator);
uint16_t lerp(uint16_t a, uint16_t b, uint32_t fraction);
void wave_table_oscilator_clean(WaveTableOscillator* oscilator);
void wave_table_oscilator_write_to_file(const WaveTableOscillator* oscillator);
void wave_table_oscilator_read_from_file(WaveTableOscillator* oscillator);

#endif
