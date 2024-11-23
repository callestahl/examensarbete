#include "utils.h"

#include <string.h>

void raddix_counting_sort(uint32_t* array, uint32_t size)
{
    if (size <= 1) return;

    uint32_t* output = (uint32_t*)calloc(size, sizeof(uint32_t));
    uint32_t count[256] = { 0 };

    for (uint32_t shift = 0, s = 0; shift < 8; ++shift, s += 8)
    {
        memset(count, 0, sizeof(count));

        for (uint32_t i = 0; i < size; ++i)
        {
            count[(array[i] >> s) & 0xFF]++;
        }

        for (uint32_t i = 1; i < 256; ++i)
        {
            count[i] += count[i - 1];
        }

        for (int32_t i = (int32_t)size - 1; i >= 0; --i)
        {
            uint32_t index = (array[i] >> s) & 0xFF;
            output[--count[index]] = array[i];
        }
        uint32_t* tmp = array;
        array = output;
        output = tmp;
    }
    free(output);
}

int32_t max_int(int32_t first, int32_t second)
{
    return first > second ? first : second;
}

int32_t min_int(int32_t first, int32_t second)
{
    return first < second ? first : second;
}

float abs_float(float value)
{
    return value + (value * -1.0f * 2.0f * (value < 0));
}

int32_t abs_int32(int32_t value)
{
    return value + (value * -1 * 2 * (value < 0));
}
