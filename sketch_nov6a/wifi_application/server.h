#ifndef SERVER_H
#define SERVER_H
#include "wave_table.h"

void server_setup(void* display);
bool server_loop(WaveTableOscillator* oscilator);

#endif
