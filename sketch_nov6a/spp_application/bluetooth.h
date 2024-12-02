#ifndef BLUETOOTH_H
#define BLUETOOTH_H
#include <stdint.h>
#include "wave_table.h"

struct Bluetooth {
    bool header_read;
    bool reading_samples;
    int32_t table_index;
    int32_t sample_index;
};

enum BluetoothSampleProcessCode {
    SAMPLE_PROCESS_OK,
    SAMPLE_PROCESS_CYCLE_DONE,
    SAMPLE_PROCESS_ERROR,
};

enum BluetoothCode {
    BLUETOOTH_OK,
    BLUETOOTH_DONE,
    BLUETOOTH_ERROR,
};

void bluetooth_reset(Bluetooth* bluetooth);
BluetoothSampleProcessCode bluetooth_process_sample(Bluetooth* bluetooth, uint16_t sample, WaveTableOscillator* oscilator);

#endif
