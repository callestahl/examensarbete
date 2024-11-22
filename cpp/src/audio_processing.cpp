#include "audio_processing.h"
#include <stdlib.h>
#include <vector>
#include <algorithm>

#include <dr_wav/dr_wav.h>

typedef struct AudioBuffer
{
    uint32_t channels;
    uint32_t sample_rate;
    uint64_t size;
    float* data;
} AudioBuffer;

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

static void
average_magnitude_difference(const AudioBuffer& audio_buffer,
                             std::vector<float>& shift_avg_difference)
{
    int maxShift = (int)audio_buffer.sample_rate / 20;
    for (int shift = 0; shift < maxShift; shift++)
    {
        float total_difference = 0;
        for (int j = shift; j < maxShift - shift; j++)
        {
            total_difference +=
                abs(audio_buffer.data[j] - audio_buffer.data[j + shift]);
        }
        float avg = total_difference / (audio_buffer.size - shift);
        if (avg > 0.001f)
        {
            shift_avg_difference.push_back(avg);
        }
    }
}

int max_int(int first, int second)
{
    return first > second ? first : second;
}

int min_int(int first, int second)
{
    return first < second ? first : second;
}

static float*
smooth_avg_difference(const std::vector<float>& shift_avg_difference,
                      int window_size)
{
    float* smoothed =
        (float*)calloc(shift_avg_difference.size(), sizeof(float));
    for (int i = 0; i < shift_avg_difference.size(); i++)
    {
        int start = max_int(0, i - (window_size / 2));
        int end = min_int((int)shift_avg_difference.size(),
                          i + (window_size / 2) + 1);
        float sum = 0;
        for (int j = start; j < end; j++)
        {
            sum += shift_avg_difference[j];
        }
        smoothed[i] = sum / (end - start);
    }
    return smoothed;
}

static void find_local_minima(std::vector<uint32_t>& local_minima,
                              float* smooth, int size)
{
    int scan_count = size / 10;
    for (int j = 0; j < size; ++j)
    {
        bool jump = false;
        for (int h = 1; h <= scan_count; ++h)
        {
            int rightIndex = j + h;
            int leftIndex = j - h;
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
            local_minima.push_back((uint32_t)j);
        }
    }
}

static uint16_t
calculate_samples_per_cycle(const std::vector<uint32_t>& local_minima)
{
    std::vector<uint32_t> counts;
    for (int j = 1; j < local_minima.size(); ++j)
    {
        uint32_t sample_count = local_minima[j] - local_minima[j - 1];
        counts.push_back(sample_count);
    }

    std::sort(counts.begin(), counts.end());

    int median = 0;
    size_t size = counts.size();
    if (size % 2 == 0)
    {
        median = (counts[size / 2 - 1] + counts[size / 2]) / 2;
    }
    else
    {
        median = counts[size / 2];
    }

    int threshold = 8;
    std::vector<uint32_t> filtered_counts;
    for (int count : counts)
    {
        if (abs(count - median) <= threshold)
        {
            filtered_counts.push_back(count);
        }
    }

    uint32_t sum = 0;
    for (uint32_t filtered_count : filtered_counts)
    {
        sum += filtered_count;
    }
    return (uint16_t)(sum / filtered_counts.size());
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

    std::vector<float> shift_avg_difference;
    average_magnitude_difference(audio_buffer, shift_avg_difference);
    float* smoothed = smooth_avg_difference(shift_avg_difference, 5);
    std::vector<uint32_t> local_minima;
    find_local_minima(local_minima, smoothed, (int)shift_avg_difference.size());
    free(smoothed);
    uint16_t samples_per_cycle = calculate_samples_per_cycle(local_minima);
    uint16_t total_cycles =
        (uint16_t)(audio_buffer.size / (uint64_t)samples_per_cycle);
    audio_buffer.size -=
        (total_cycles - total_cycles_to_send) * samples_per_cycle;

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

    ByteArray result = {};
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
    return result;
}
