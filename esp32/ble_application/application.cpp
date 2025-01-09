/*
 * DISCLAIMER: 
 * For a complete implementation, refer to esp32/spp_application/.
 *
 * This implementation is not fully functional because its main
 * purpose is to test the differences between Bluetooth Classic
 * and Bluetooth LE. This is why this implementation of BLE does not
 * play any sound (It could but the logic for turning off Bluetooth LE on demand
 * has not been implemented) and only handles the Bluetooth logic.
 * 
 */
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <esp_heap_caps.h>

#include "wave_table.h"
#include "ble.h"
#include "bluetooth.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128

#define OLED_DC 16
#define OLED_CS 15
#define OLED_RESET 17

#define VSPI_SCK 18
#define VSPI_MISO 19
#define VSPI_MOSI 23
#define VSPI_CS 5

#define HSPI_SCK 14
#define HSPI_MISO 12
#define HSPI_MOSI 13
#define HSPI_CS OLED_CS

#define PIN_PITCH_INPUT 36
#define PIN_WAVETABLE_POSITION 39

#define WAVETABLE_SIZE 256
#define SAMPLE_RATE 44100
#define MAX_12BIT_VALUE 4095
#define MAX_16BIT_VALUE 65535

#define C0_FREQUENCY 16.35160f
#define PITCH_INPUT_RANGE 10.0f

#define RGB565(r, g, b) (((r << 11)) | (g << 5) | b)
#define SSD1351_BLACK RGB565(0, 0, 0)
#define SSD1351_WHITE RGB565(31, 63, 31)
#define SSD1351_RED RGB565(31, 0, 0)
#define SSD1351_GREEN RGB565(0, 63, 0)
#define SSD1351_BLUE RGB565(0, 0, 31)

#define STACK_SIZE 4096
#define SPP_QUEUE_SIZE 4096

#define BLUETOOTH_BUTTON_PIN 4
#define BLUETOOTH_LIGHT_PIN 22

#define BUTTON_DEBOUNCE_DELAY 50

struct ButtonState
{
    bool button_pressed;
    uint64_t last_debounce_time;
};

void wave_table_draw(const WaveTable* table, uint32_t table_length,
                     uint16_t color);
void clear_screen(int16_t x, int16_t y);
void generate_sine_wave(WaveTable* table, uint32_t table_length);
void redraw_screen(uint16_t cycle_index, uint16_t last_cycle_index);
bool button_is_clicked(ButtonState* button, int32_t pin);
void process_buttons();
void wavetable_oscillation();
uint16_t analog_input_to_pitch(uint16_t analog_value);
void turn_off_bluetooth(void);

SPIClass g_vspi(VSPI);
SPISettings g_vspi_settings = SPISettings(16000000, MSBFIRST, SPI_MODE0);

SPIClass g_hspi(HSPI);
SPISettings g_hspi_settings = SPISettings(16000000, MSBFIRST, SPI_MODE0);

Adafruit_SSD1351 g_display(SCREEN_WIDTH, SCREEN_HEIGHT, &g_vspi, VSPI_CS, OLED_DC,
                         OLED_RESET);

WaveTableOscillator g_osci;

ButtonState g_bluetooth_button = {};
bool g_bluetooth_enabled = false;

uint64_t g_sample_period_us = 1000000 / SAMPLE_RATE;
uint64_t g_next_sample_time;

static SemaphoreHandle_t g_oscillator_mutex = NULL;
static SemaphoreHandle_t g_oscillator_screen_mutex = NULL;

volatile uint16_t g_selected_cycle = 0;
volatile uint16_t g_last_selected_cycle = MAX_16BIT_VALUE;
static TaskHandle_t g_redraw_screen_task_handle = NULL;

void redraw_screen_task(void* data)
{
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (g_bluetooth_enabled || g_osci.total_cycles > 0)
        {
            uint16_t cycle_to_draw = g_selected_cycle;
            if (xSemaphoreTake(g_oscillator_screen_mutex, portMAX_DELAY))
            {
                redraw_screen(cycle_to_draw, g_last_selected_cycle);
                xSemaphoreGive(g_oscillator_screen_mutex);
            }
            g_last_selected_cycle = cycle_to_draw;
        }
    }
}

void print_heap_size(void)
{
    size_t heap_size = esp_get_free_heap_size();

    g_display.fillScreen(SSD1351_BLACK);
    g_display.setCursor(0, 0);
    g_display.println(heap_size);
    g_display.println(g_osci.total_cycles);
    g_display.println(g_osci.samples_per_cycle);
}

static TaskHandle_t g_ble_task_handle = NULL;

void ble_task(void* data)
{
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (ble_copy_transfer(&g_osci, g_oscillator_mutex,
                              g_oscillator_screen_mutex))
        {
            wave_table_oscilator_write_to_file(&g_osci);

            if (g_redraw_screen_task_handle != NULL)
            {
                xTaskNotifyGive(g_redraw_screen_task_handle);
            }
        }
    }
}

void application_setup()
{
    Serial.begin(115200);
    SPIFFS.begin(true);

    pinMode(BLUETOOTH_BUTTON_PIN, INPUT_PULLUP);
    pinMode(BLUETOOTH_LIGHT_PIN, OUTPUT);
    digitalWrite(BLUETOOTH_LIGHT_PIN, LOW);

    pinMode(PIN_PITCH_INPUT, INPUT);
    pinMode(PIN_WAVETABLE_POSITION, INPUT);

#if 1
    g_display.begin();
    g_display.fillScreen(SSD1351_BLACK);
    g_display.setTextColor(SSD1351_WHITE, SSD1351_BLACK);
    g_display.setTextSize(1);
    g_display.println("Hello1");
#endif

    g_hspi.begin();
    pinMode(HSPI_CS, OUTPUT);
    digitalWrite(HSPI_CS, HIGH);

    g_next_sample_time = micros();

    g_oscillator_mutex = xSemaphoreCreateMutex();
    g_oscillator_screen_mutex = xSemaphoreCreateMutex();

#if 1
    ble_setup("WaveTablePP_2", &g_ble_task_handle, &g_osci);
    digitalWrite(BLUETOOTH_LIGHT_PIN, HIGH);
    g_bluetooth_enabled = true;
#endif

    g_hspi.end();

    g_osci.tables_capacity = 256;
    g_osci.tables = (WaveTable*)calloc(g_osci.tables_capacity, sizeof(WaveTable));

    wave_table_oscilator_read_from_file(&g_osci);

    xTaskCreatePinnedToCore(redraw_screen_task, "Screen Redraw", STACK_SIZE,
                            NULL, 1, &g_redraw_screen_task_handle, 0);
    xTaskCreatePinnedToCore(ble_task, "BLE messages", STACK_SIZE, NULL, 1,
                            &g_ble_task_handle, 0);

    xTaskNotifyGive(g_redraw_screen_task_handle);
}

void application_loop()
{
    if (!g_bluetooth_enabled && g_osci.total_cycles > 0)
    {
        wavetable_oscillation();
    }
}

uint16_t g_analog_value = 0;

#if 1
void redraw_screen(uint16_t cycle_index, uint16_t last_cycle_index)
{
    static bool bluetooth_last_on = false;
    if (g_bluetooth_enabled)
    {
        const uint16_t half_screen_width = SCREEN_WIDTH / 2;
        const uint16_t half_screen_height = SCREEN_WIDTH / 2;

        g_display.fillScreen(SSD1351_BLACK);
        g_display.setCursor(10, 10);
        g_display.setTextSize(2);
        g_display.setTextColor(SSD1351_BLUE, SSD1351_BLACK);
        g_display.println("BLUETOOTH");
        g_display.setCursor(30, half_screen_height);
        g_display.setTextColor(SSD1351_WHITE, SSD1351_BLACK);
        g_display.println("UPLOAD");
        g_display.setCursor(42, half_screen_height + 30);
        g_display.println("FILE");
        bluetooth_last_on = true;
    }
    else
    {
        if (bluetooth_last_on)
        {
            g_display.fillScreen(SSD1351_BLACK);
            g_display.setTextSize(1);
            bluetooth_last_on = false;
        }
        g_display.setTextColor(SSD1351_WHITE, SSD1351_BLACK);
        if (last_cycle_index < g_osci.total_cycles)
        {
            wave_table_draw(&g_osci.tables[last_cycle_index],
                            g_osci.samples_per_cycle, SSD1351_BLACK);
        }
        wave_table_draw(&g_osci.tables[cycle_index], g_osci.samples_per_cycle,
                        SSD1351_RED);
        g_display.setCursor(0, 40 + (SCREEN_HEIGHT / 2));
        g_display.printf("Position: %03u\n", cycle_index);
        g_display.printf("Analog: %04u\n", g_analog_value);
    }
}
#endif

const uint32_t SCREEN_WIDTH_WITH_FRACTION = SCREEN_WIDTH << 16;
const uint32_t WINDOW_HEIGHT_75_PROCENT = (SCREEN_HEIGHT * 3) / 4;

uint32_t y_position_75_procent(uint16_t data)
{
    return WINDOW_HEIGHT_75_PROCENT -
           ((data * WINDOW_HEIGHT_75_PROCENT) / MAX_16BIT_VALUE);
}

void wave_table_draw(const WaveTable* table, uint32_t table_length,
                     uint16_t color)
{
    const uint32_t samples_per_draw = 2;
    const uint32_t x_step =
        (SCREEN_WIDTH_WITH_FRACTION / (table_length / samples_per_draw));

    uint32_t x0 = 0;
    uint32_t y0 =
        WINDOW_HEIGHT_75_PROCENT -
        ((table->samples[0] * WINDOW_HEIGHT_75_PROCENT) / MAX_16BIT_VALUE);

    for (uint32_t i = samples_per_draw; i < table_length; i += samples_per_draw)
    {
        uint32_t x1 = x0 + x_step;
        uint32_t y1 = y_position_75_procent(table->samples[i]);
        g_display.drawLine(x0 >> 16, y0, x1 >> 16, y1, color);

        x0 = x1;
        y0 = y1;
    }
}

void generate_sine_wave(WaveTable* table, uint32_t table_length)
{
    for (int i = 0; i < table_length; i++)
    {
        table->samples[i] = (uint16_t)(MAX_16BIT_VALUE / 2 *
                                       (1 + sin(2 * M_PI * i / table_length)));
    }
}

void generate_sawtooth_wave(WaveTable* table, uint32_t table_length)
{
    for (int i = 0; i < table_length; i++)
    {
        table->samples[i] = (uint16_t)((MAX_16BIT_VALUE * i) / table_length);
    }
}

void generate_square_wave(WaveTable* table, uint32_t table_length)
{
    for (int i = 0; i < table_length; i++)
    {
        table->samples[i] = (i < table_length / 2) ? MAX_16BIT_VALUE : 0;
    }
}

uint32_t plus_one_wrap(uint32_t value, uint32_t length)
{
    return (value + 1) % length;
}

uint32_t minus_one_wrap(uint32_t value, uint32_t length)
{
    return (value + (length - 1)) % length;
}

bool button_is_clicked(ButtonState* button, int32_t pin)
{
    const int32_t button_state = digitalRead(pin);
    if (button_state == LOW)
    {
        if (!button->button_pressed &&
            ((millis() - button->last_debounce_time) > BUTTON_DEBOUNCE_DELAY))
        {
            button->button_pressed = true;
            button->last_debounce_time = millis();
            return true;
        }
    }
    else
    {
        if (button->button_pressed &&
            ((millis() - button->last_debounce_time) > BUTTON_DEBOUNCE_DELAY))
        {
            button->button_pressed = false;
            button->last_debounce_time = millis();
        }
    }
    return false;
}

uint16_t g_last_selected_cycle_osci = MAX_16BIT_VALUE;

uint64_t g_timer = millis();

const uint8_t LAST_ANALOG_VALUES_SIZE = 20;

uint16_t g_analog_pitch_index = 0;
uint16_t g_last_analog_pitch_values[LAST_ANALOG_VALUES_SIZE] = { 0 };

uint16_t g_analog_position_index = 0;
uint16_t g_last_analog_position_values[LAST_ANALOG_VALUES_SIZE] = { 0 };

uint16_t get_last_analog_average(uint16_t* values)
{
    uint32_t sum = 0;
    for (uint32_t i = 0; i < LAST_ANALOG_VALUES_SIZE; ++i)
    {
        sum += values[i];
    }
    return (uint16_t)(sum / LAST_ANALOG_VALUES_SIZE);
}

uint16_t min(uint16_t first, uint16_t second)
{
    return first < second ? first : second;
}

uint16_t get_cycle_from_analog(int32_t current_analog_value,
                               uint16_t total_cycles)
{
    uint32_t values_per_sample = MAX_12BIT_VALUE / total_cycles;
    uint32_t half_point = values_per_sample / 2;

    uint16_t current_index = current_analog_value / values_per_sample;
    if (current_index == g_selected_cycle)
    {
        return current_index;
    }

    int32_t last_analog_value =
        (g_selected_cycle * values_per_sample) + half_point;
    int32_t analog_direction = current_analog_value - last_analog_value;
    if (analog_direction > 0)
    {
        int32_t next_index_midpoint =
            (((uint32_t)g_selected_cycle + 1) * values_per_sample) + half_point;
        if (current_analog_value >= next_index_midpoint)
        {
            return current_index;
        }
    }
    else if (analog_direction < 0)
    {
        int32_t prev_index_midpoint =
            (int32_t)((uint32_t)g_selected_cycle * values_per_sample) -
            half_point;
        if (current_analog_value < prev_index_midpoint)
        {
            return current_index;
        }
    }
    return g_selected_cycle;
}

void dac_write(uint16_t value)
{
    value |= 0x1000;
    value |= 0x2000;
    digitalWrite(HSPI_CS, LOW);

    g_hspi.beginTransaction(g_hspi_settings);
    g_hspi.transfer((uint8_t)(value >> 8));
    g_hspi.transfer((uint8_t)(value & 0xFF));
    g_hspi.endTransaction();

    digitalWrite(HSPI_CS, HIGH);
}

bool should_redraw = false;

void wavetable_oscillation()
{
    if (g_osci.total_cycles == 0 || g_osci.tables[0].samples == NULL)
    {
        return;
    }

#if 1
    uint16_t pitch_analog_value = analogRead(PIN_PITCH_INPUT);
    g_last_analog_pitch_values[g_analog_pitch_index] = pitch_analog_value;
    g_analog_pitch_index =
        plus_one_wrap(g_analog_pitch_index, LAST_ANALOG_VALUES_SIZE);

    pitch_analog_value = get_last_analog_average(g_last_analog_pitch_values);
    uint16_t frequency = analog_input_to_pitch(pitch_analog_value);
#else
    uint16_t frequency = analog_input_to_pitch(1000);
#endif

    uint16_t selected_cycle_analog_value = g_analog_value =
        analogRead(PIN_WAVETABLE_POSITION);

    g_last_analog_position_values[g_analog_position_index] =
        selected_cycle_analog_value;
    g_analog_position_index =
        plus_one_wrap(g_analog_position_index, LAST_ANALOG_VALUES_SIZE);

    selected_cycle_analog_value =
        get_last_analog_average(g_last_analog_position_values);

#if 1
    uint16_t selected_cycle =
        get_cycle_from_analog(selected_cycle_analog_value, g_osci.total_cycles);
#else

    uint16_t selected_cycle =
        (selected_cycle_analog_value * g_osci.total_cycles) / MAX_12BIT_VALUE;
#endif

    frequency = min(frequency, SAMPLE_RATE / 2);

    uint64_t difference = micros() - g_next_sample_time;
    if (difference >= 0)
    {
        uint16_t value = 0;
        if (xSemaphoreTake(g_oscillator_mutex, portMAX_DELAY))
        {
            uint64_t wavetable_size = g_osci.samples_per_cycle;
            g_osci.phase_increment =
                (((uint64_t)frequency * wavetable_size) << 32) / SAMPLE_RATE;

            if (selected_cycle >= g_osci.total_cycles)
            {
                selected_cycle = g_osci.total_cycles - 1;
            }

            value = wave_table_linear_interpolation(
                g_osci.tables + selected_cycle, g_osci.samples_per_cycle,
                g_osci.phase);

            wave_table_oscilator_update_phase(&g_osci);

            xSemaphoreGive(g_oscillator_mutex);
        }

#if 0
        if (millis() >= timer)
        {
            g_display.fillScreen(SSD1351_BLACK);
            g_display.setCursor(10, 10);
            g_display.printf("%u\n", value >> 4);
            timer = millis() + 500;
        }
#endif

        dac_write(value >> 4);

        g_selected_cycle = selected_cycle;

        if (selected_cycle != g_last_selected_cycle_osci)
        {
            should_redraw = true;
            g_last_selected_cycle_osci = selected_cycle;
        }
#if 1
        if (should_redraw)
        {
            if (millis() >= g_timer)
            {
                if (g_redraw_screen_task_handle != NULL)
                {
                    xTaskNotifyGive(g_redraw_screen_task_handle);
                }
                g_timer = millis() + 150;

                should_redraw = false;
            }
        }
#endif

        g_next_sample_time += g_sample_period_us;
    }
}

uint16_t analog_input_to_pitch(uint16_t analog_value)
{
    float voltage = (float)analog_value / MAX_12BIT_VALUE * PITCH_INPUT_RANGE;
    float frequency = C0_FREQUENCY * pow(2, voltage);
    return frequency;
}
