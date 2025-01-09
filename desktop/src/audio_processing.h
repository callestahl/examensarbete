#ifndef AUDIO_PROCESSING_H
#define AUDIO_PROCESSING_H
#include <stdint.h>

typedef struct ByteArray
{
    uint32_t size;
    uint8_t* data;
} ByteArray;

ByteArray process_audio_buffer(const char* file_name, uint16_t total_cycles_to_send);

#endif
