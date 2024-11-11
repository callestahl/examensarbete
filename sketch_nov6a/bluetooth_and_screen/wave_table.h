#ifndef WAVE_TABLE_H
#define WAVE_TABLE_H
#include <stdint.h>

struct WaveTable {
    const char* name;
    uint16_t* data;
};

struct WaveTableOscillator {
    uint64_t phase;
    uint64_t phase_increment;

    uint32_t table_length;
    uint32_t table_capacity;
    uint32_t table_count;
    WaveTable* tables;
};

uint16_t wave_table_oscilator_linear_interpolation(const WaveTable* table, uint32_t table_length, uint64_t phase);
void wave_table_oscilator_update_phase(WaveTableOscillator* oscilator);
uint16_t lerp(uint16_t a, uint16_t b, uint32_t fraction);

#endif
