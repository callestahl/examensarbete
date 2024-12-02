#include "audio_processing.h"
#include "utils.h"

#include <stdlib.h>
#include <stdio.h>

#include <dr_wav/dr_wav.h>

typedef struct AudioBuffer
{
    uint32_t channels;
    uint32_t sample_rate;
    uint64_t size;
    float* data;
} AudioBuffer;

typedef struct FloatArray
{
    uint32_t size;
    uint32_t capacity;
    float* data;
} FloatArray;

typedef struct Uint32Array
{
    uint32_t size;
    uint32_t capacity;
    uint32_t* data;
} Uint32Array;

static void convert_stereo_to_mono_sum_average(AudioBuffer* audio_buffer)
{
    uint64_t half_size = audio_buffer->size / 2;
    float* converted_buffer = (float*)calloc(half_size, sizeof(float));

    for (uint64_t i = 0, j = 0; i < half_size; ++i, j += 2)
    {
        converted_buffer[i] =
            (audio_buffer->data[j] + audio_buffer->data[j + 1]) / 2.0f;
    }
    free(audio_buffer->data);
    audio_buffer->channels = 1;
    audio_buffer->size = half_size;
    audio_buffer->data = converted_buffer;
}

static AudioBuffer get_sample_data(const char* file_name)
{
    AudioBuffer audio_buffer = {};
    uint64_t frame_count = 0;
    audio_buffer.data = drwav_open_file_and_read_pcm_frames_f32(
        file_name, &audio_buffer.channels, &audio_buffer.sample_rate,
        &frame_count, NULL);

    audio_buffer.size = frame_count * audio_buffer.channels;
    if (audio_buffer.channels == 2)
    {
        convert_stereo_to_mono_sum_average(&audio_buffer);
    }
    else if (audio_buffer.channels > 2)
    {
        // TODO: Log error
        free(audio_buffer.data);
        return audio_buffer;
    }

    return audio_buffer;
}

static void average_magnitude_difference(const AudioBuffer& audio_buffer,
                                         FloatArray* shift_avg_difference)
{
    int32_t max_shift = (int32_t)audio_buffer.sample_rate / 20;
    array_create(shift_avg_difference, max_shift);
    for (int32_t shift = 0; shift < max_shift; shift++)
    {
        float total_difference = 0;
        for (int32_t j = shift; j < max_shift - shift; j++)
        {
            total_difference += abs_float(
                (audio_buffer.data[j] - audio_buffer.data[j + shift]));
        }
        float avg = total_difference / (audio_buffer.size - shift);
        if (avg > 0.00001f)
        {
            array_append(shift_avg_difference, avg);
        }
    }
}

static float* smooth_avg_difference(const FloatArray& shift_avg_difference,
                                    int32_t window_size)
{
    float* smoothed = (float*)calloc(shift_avg_difference.size, sizeof(float));
    for (int32_t i = 0; i < (int32_t)shift_avg_difference.size; i++)
    {
        int32_t start = max_int(0, i - (window_size / 2));
        int32_t end = min_int((int32_t)shift_avg_difference.size,
                              i + (window_size / 2) + 1);
        float sum = 0;
        for (int32_t j = start; j < end; j++)
        {
            sum += shift_avg_difference.data[j];
        }
        smoothed[i] = sum / (end - start);
    }
    return smoothed;
}

static void find_local_minima(Uint32Array* local_minima, float* smooth,
                              int32_t size)
{
    int32_t scan_count = max_int(size / 10, 10);
    for (int32_t j = 0; j < size; ++j)
    {
        bool jump = false;
        for (int32_t h = 1; h <= scan_count; ++h)
        {
            int32_t rightIndex = j + h;
            int32_t leftIndex = j - h;
            if (leftIndex >= 0)
            {
                if (smooth[leftIndex] < smooth[j])
                {
                    jump = true;
                    break;
                }
            }
            if (rightIndex < size)
            {
                if (smooth[rightIndex] < smooth[j])
                {
                    jump = true;
                    break;
                }
            }
        }
        if (!jump)
        {
            array_append(local_minima, (uint32_t)j);

            if (local_minima->size > 1)
            {
                int32_t last_minima = local_minima->data[local_minima->size - 2];
                int32_t current_minima = local_minima->data[local_minima->size - 1];
                int32_t period = current_minima - last_minima;

                scan_count = max_int(period / 2, 10);
            }
        }
    }
}

static uint16_t calculate_samples_per_cycle(const Uint32Array& local_minima)
{
    uint32_t count_size = local_minima.size - 1;
    uint32_t* counts = (uint32_t*)calloc(count_size, sizeof(uint32_t));
    for (int32_t j = 1; j < (int32_t)local_minima.size; ++j)
    {
        uint32_t sample_count = local_minima.data[j] - local_minima.data[j - 1];
        counts[j - 1] = sample_count;
    }

    raddix_counting_sort(counts, count_size);

    int32_t median = 0;
    if (count_size % 2 == 0)
    {
        median =
            (int32_t)(counts[count_size / 2 - 1] + counts[count_size / 2]) / 2;
    }
    else
    {
        median = (int32_t)counts[count_size / 2];
    }

    uint32_t filtered_counts_size = 0;
    uint32_t* filtered_counts = (uint32_t*)calloc(count_size, sizeof(uint32_t));
    int32_t threshold = 8;
    for (uint32_t i = 0; i < count_size; ++i)
    {
        int32_t count = (int32_t)counts[i];
        if (abs_int32(count - median) <= threshold)
        {
            filtered_counts[filtered_counts_size++] = count;
        }
    }

    uint32_t sum = 0;
    for (uint32_t i = 0; i < filtered_counts_size; ++i)
    {
        sum += filtered_counts[i];
    }

    uint16_t result = (uint16_t)(sum / filtered_counts_size);

    free(counts);
    free(filtered_counts);

    return result;
}

static uint8_t low(uint16_t value)
{
    return (uint8_t)(value & 0xFF);
}

static uint8_t high(uint16_t value)
{
    return (uint8_t)((value >> 8) & 0xFF);
}

ByteArray process_audio_buffer(const char* file_name,
                               uint16_t total_cycles_to_send)
{
    AudioBuffer audio_buffer = get_sample_data(file_name);

    FloatArray shift_avg_difference = { 0 };
    average_magnitude_difference(audio_buffer, &shift_avg_difference);

    float* smoothed = smooth_avg_difference(shift_avg_difference, 5);

    Uint32Array local_minima = { 0 };
    array_create(&local_minima, 100);
    find_local_minima(&local_minima, smoothed,
                      (int32_t)shift_avg_difference.size);
    ByteArray result = {};
    if (local_minima.size <= 1)
    {
        printf("Error: local_minima\n");
        return result;
    }

    uint16_t samples_per_cycle = calculate_samples_per_cycle(local_minima);
    uint16_t total_cycles =
        (uint16_t)(audio_buffer.size / (uint64_t)samples_per_cycle);

    printf("Samples per cycle: %u\n", samples_per_cycle);
    printf("Total cycles: %u\n", total_cycles);

    audio_buffer.size -=
        ((total_cycles - total_cycles_to_send) * samples_per_cycle) *
        (total_cycles_to_send != 0);

    uint32_t header_size = 14;
    uint32_t total_bytes = ((uint32_t)audio_buffer.size * 2) + header_size;

    uint16_t id0 = 29960;
    uint16_t id1 = 62903;
    uint16_t id2 = 35185;
    uint16_t id3 = 26662;

    uint8_t samples_per_cycle_low = low(samples_per_cycle);
    uint8_t samples_per_cycle_high = high(samples_per_cycle);

    uint8_t total_bytes_high0 = (uint8_t)((total_bytes >> 24) & 0xFF);
    uint8_t total_bytes_high1 = (uint8_t)((total_bytes >> 16) & 0xFF);
    uint8_t total_bytes_low0 = (uint8_t)((total_bytes >> 8) & 0xFF);
    uint8_t total_bytes_low1 = (uint8_t)((total_bytes) & 0xFF);

    uint8_t header[] = {
        high(id0),
        low(id0),
        high(id1),
        low(id1),
        high(id2),
        low(id2),
        high(id3),
        low(id3),
        samples_per_cycle_high,
        samples_per_cycle_low,
        total_bytes_high0,
        total_bytes_high1,
        total_bytes_low0,
        total_bytes_low1,
    };

    result.size = total_bytes;
    result.data = (uint8_t*)calloc(total_bytes, sizeof(uint8_t));

    for (uint32_t i = 0; i < header_size; ++i)
    {
        result.data[i] = header[i];
    }

    for (uint64_t i = 0, j = header_size; i < audio_buffer.size; ++i)
    {
        uint16_t converted_value =
            (uint16_t)((audio_buffer.data[i] + 1.0f) * 32767.5f);
        result.data[j++] = high(converted_value);
        result.data[j++] = low(converted_value);
    }

    array_free(&local_minima);
    array_free(&shift_avg_difference);
    free(smoothed);

    printf("Total bytes: %u\n", result.size - header_size);

    return result;
}
