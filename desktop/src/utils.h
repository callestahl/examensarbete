#ifndef UTILS_H
#define UTILS_H
#include <stdlib.h>
#include <stdint.h>

void raddix_counting_sort(uint32_t* array, uint32_t size);
int32_t max_int(int32_t first, int32_t second);
int32_t min_int(int32_t first, int32_t second);
float abs_float(float value);
int32_t abs_int32(int32_t value);

#define array_create_d(array) array_create(array, 10)
#define array_create(array, array_capacity)                                    \
    do                                                                         \
    {                                                                          \
        (array)->size = 0;                                                     \
        (array)->capacity = (array_capacity);                                  \
        (array)->data = (decltype((array)->data))calloc(                       \
            (array_capacity), sizeof((*(array)->data)));                       \
    } while (0)

#define array_append(array, value)                                             \
    do                                                                         \
    {                                                                          \
        if ((array)->size >= (array)->capacity)                                \
        {                                                                      \
            (array)->capacity *= 2;                                            \
            (array)->data = (decltype((array)->data))realloc(                  \
                (array)->data, (array)->capacity * sizeof((*(array)->data)));  \
        }                                                                      \
        (array)->data[(array)->size++] = (value);                              \
    } while (0)

#define array_free(array) free((array)->data)

#endif
