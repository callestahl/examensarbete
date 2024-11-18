#ifndef SPP_H
#define SPP_H
#include "wave_table.h"

void spp_setup(const char* name);
bool spp_look_for_incoming_messages(WaveTableOscillator* oscilator, void* display);

#endif
