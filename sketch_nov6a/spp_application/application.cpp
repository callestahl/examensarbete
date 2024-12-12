#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <esp_heap_caps.h>

#include "wave_table.h"
#include "spp.h"
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
#define SPP_QUEUE_SIZE 16384

#define BLUETOOTH_BUTTON_PIN 4
#define BLUETOOTH_LIGHT_PIN 22

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

SPIClass vspi(VSPI);
SPISettings vspi_settings = SPISettings(16000000, MSBFIRST, SPI_MODE0);

SPIClass hspi(HSPI);
SPISettings hspi_settings = SPISettings(16000000, MSBFIRST, SPI_MODE0);

Adafruit_SSD1351 display(SCREEN_WIDTH, SCREEN_HEIGHT, &vspi, VSPI_CS, OLED_DC,
                         OLED_RESET);

WaveTableOscillator osci;
int32_t display_wave_index = 0;
int32_t selected_index = 0;

const int32_t button_pin0 = 4;
const int32_t button_pin1 = 2;
const uint64_t debounce_delay = 50;
ButtonState button0 = {};
ButtonState button1 = {};

ButtonState bluetooth_button = {};
bool bluetooth_enabled = false;

int32_t sample_viewer = 0;

uint64_t button_repeat_reset = 0;

uint64_t sample_period_us = 1000000 / SAMPLE_RATE;
uint64_t next_sample_time;
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
        if (bluetooth_enabled || osci.total_cycles > 0)
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

    display.fillScreen(SSD1351_BLACK);
    display.setCursor(0, 0);
    display.println(heap_size);
    display.println(osci.total_cycles);
    display.println(osci.samples_per_cycle);
}

static TaskHandle_t g_spp_task_handle = NULL;

void spp_task(void* data)
{
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        BluetoothCode bluetooth_code = spp_look_for_incoming_messages(
            &osci, g_oscillator_mutex, g_oscillator_screen_mutex);

        if (bluetooth_code == BLUETOOTH_DONE)
        {
            // wave_table_oscilator_write_to_file(&osci);

            // turn_off_bluetooth();

            display_wave_index = 0;
            // if (g_redraw_screen_task_handle != NULL)
            {
                // xTaskNotifyGive(g_redraw_screen_task_handle);
            }
        }
        else if (bluetooth_code == BLUETOOTH_ERROR)
        {
            wave_table_oscilator_read_from_file(&osci);
        }
    }
}

void turn_off_bluetooth(void)
{
    hspi.begin();
    spp_end();
    bluetooth_enabled = false;
    digitalWrite(BLUETOOTH_LIGHT_PIN, LOW);
}

void application_setup()
{
    Serial.begin(115200);
    SPIFFS.begin(true);

    // pinMode(button_pin0, INPUT);
    // pinMode(button_pin1, INPUT);

    pinMode(BLUETOOTH_BUTTON_PIN, INPUT_PULLUP);
    pinMode(BLUETOOTH_LIGHT_PIN, OUTPUT);
    digitalWrite(BLUETOOTH_LIGHT_PIN, LOW);

    pinMode(PIN_PITCH_INPUT, INPUT);
    pinMode(PIN_WAVETABLE_POSITION, INPUT);

#if 1
    display.begin();
    display.fillScreen(SSD1351_BLACK);
    display.setTextColor(SSD1351_WHITE, SSD1351_BLACK);
    display.setTextSize(1);
    display.println("Hello1");
#endif
    hspi.begin();
    pinMode(HSPI_CS, OUTPUT);
    digitalWrite(HSPI_CS, HIGH);

    next_sample_time = micros();

    g_oscillator_mutex = xSemaphoreCreateMutex();
    g_oscillator_screen_mutex = xSemaphoreCreateMutex();

    spp_setup(&g_spp_task_handle, SPP_QUEUE_SIZE);

#if 0
    spp_begin("WaveTablePP_2");
    digitalWrite(BLUETOOTH_LIGHT_PIN, HIGH);
    bluetooth_enabled = true;
#endif

    osci.tables_capacity = 256;
    osci.tables = (WaveTable*)calloc(osci.tables_capacity, sizeof(WaveTable));

    wave_table_oscilator_read_from_file(&osci);

#if 0

    osci.tables[osci.total_cycles++].samples =
        (uint16_t*)calloc(osci.samples_per_cycle, sizeof(uint16_t));
    osci.tables[osci.total_cycles++].samples =
        (uint16_t*)calloc(osci.samples_per_cycle, sizeof(uint16_t));
    osci.tables[osci.total_cycles++].samples =
        (uint16_t*)calloc(osci.samples_per_cycle, sizeof(uint16_t));
    osci.tables[osci.total_cycles++].samples =
        (uint16_t*)calloc(osci.samples_per_cycle, sizeof(uint16_t));
    osci.tables[osci.total_cycles++].samples =
        (uint16_t*)calloc(osci.samples_per_cycle, sizeof(uint16_t));
#endif

    xTaskCreatePinnedToCore(redraw_screen_task, "Screen Redraw", STACK_SIZE,
                            NULL, 1, &g_redraw_screen_task_handle, 0);
    xTaskCreatePinnedToCore(spp_task, "SPP messages", STACK_SIZE, NULL, 1,
                            &g_spp_task_handle, 0);

    // print_heap_size();
}

void application_loop()
{
#if 1
    if (button_is_clicked(&bluetooth_button, BLUETOOTH_BUTTON_PIN))
    {
        if (!bluetooth_enabled)
        {
            hspi.end();
            wave_table_oscilator_write_to_file(&osci);
            spp_begin("WaveTablePP_2");
            digitalWrite(BLUETOOTH_LIGHT_PIN, HIGH);
            bluetooth_enabled = true;
            if (g_redraw_screen_task_handle != NULL)
            {
                xTaskNotifyGive(g_redraw_screen_task_handle);
            }
        }
        else
        {
            turn_off_bluetooth();
            if (g_redraw_screen_task_handle != NULL)
            {
                xTaskNotifyGive(g_redraw_screen_task_handle);
            }
        }
    }
#endif

    if (!bluetooth_enabled && osci.total_cycles > 0)
    {
        wavetable_oscillation();
    }
}

uint16_t g_analog_value = 0;

#if 1
void redraw_screen(uint16_t cycle_index, uint16_t last_cycle_index)
{
    static bool bluetooth_last_on = false;
    if (bluetooth_enabled)
    {
        const uint16_t half_screen_width = SCREEN_WIDTH / 2;
        const uint16_t half_screen_height = SCREEN_WIDTH / 2;

        display.fillScreen(SSD1351_BLACK);
        display.setCursor(10, 10);
        display.setTextSize(2);
        display.setTextColor(SSD1351_BLUE, SSD1351_BLACK);
        display.println("BLUETOOTH");
        display.setCursor(30, half_screen_height);
        display.setTextColor(SSD1351_WHITE, SSD1351_BLACK);
        display.println("UPLOAD");
        display.setCursor(42, half_screen_height + 30);
        display.println("FILE");
        bluetooth_last_on = true;
    }
    else
    {
        if (bluetooth_last_on)
        {
            display.fillScreen(SSD1351_BLACK);
            display.setTextSize(1);
            bluetooth_last_on = false;
        }
        display.setTextColor(SSD1351_WHITE, SSD1351_BLACK);
        if (last_cycle_index < osci.total_cycles)
        {
            wave_table_draw(&osci.tables[last_cycle_index],
                            osci.samples_per_cycle, SSD1351_BLACK);
        }
        wave_table_draw(&osci.tables[cycle_index], osci.samples_per_cycle,
                        SSD1351_RED);
        display.setCursor(0, 40 + (SCREEN_HEIGHT / 2));
        display.printf("Position: %03u\n", cycle_index);
        display.printf("Analog: %04u\n", g_analog_value);
    }
}
#endif

const uint32_t screen_width_with_fraction = SCREEN_WIDTH << 16;
const uint32_t window_height_75_procent = (SCREEN_HEIGHT * 3) / 4;

uint32_t y_position_75_procent(uint16_t data)
{
    return window_height_75_procent -
           ((data * window_height_75_procent) / MAX_16BIT_VALUE);
}

void wave_table_draw(const WaveTable* table, uint32_t table_length,
                     uint16_t color)
{
    const uint32_t samples_per_draw = 2;
    const uint32_t x_step =
        (screen_width_with_fraction / (table_length / samples_per_draw));

    uint32_t x0 = 0;
    uint32_t y0 =
        window_height_75_procent -
        ((table->samples[0] * window_height_75_procent) / MAX_16BIT_VALUE);

    for (uint32_t i = samples_per_draw; i < table_length; i += samples_per_draw)
    {
        uint32_t x1 = x0 + x_step;
        uint32_t y1 = y_position_75_procent(table->samples[i]);
        display.drawLine(x0 >> 16, y0, x1 >> 16, y1, color);

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
            ((millis() - button->last_debounce_time) > debounce_delay))
        {
            button->button_pressed = true;
            button->last_debounce_time = millis();
            return true;
        }
    }
    else
    {
        if (button->button_pressed &&
            ((millis() - button->last_debounce_time) > debounce_delay))
        {
            button->button_pressed = false;
            button->last_debounce_time = millis();
        }
    }
    return false;
}

void process_buttons()
{
    if (button_is_clicked(&button0, button_pin0))
    {
        // display_wave_index =
        // plus_one_wrap(display_wave_index, osci.total_cycles);
        g_analog_value += 5;
        xTaskNotifyGive(g_redraw_screen_task_handle);
    }
    if (button_is_clicked(&button1, button_pin1))
    {
        // display_wave_index =
        // minus_one_wrap(display_wave_index, osci.total_cycles);
        g_analog_value -= 5;
        xTaskNotifyGive(g_redraw_screen_task_handle);
    }
}

uint16_t last_selected_cycle = MAX_16BIT_VALUE;

uint64_t timer = millis();

const uint8_t LAST_ANALOG_VALUES_SIZE = 20;

uint16_t analog_pitch_index = 0;
uint16_t last_analog_pitch_values[LAST_ANALOG_VALUES_SIZE] = { 0 };

uint16_t analog_position_index = 0;
uint16_t last_analog_position_values[LAST_ANALOG_VALUES_SIZE] = { 0 };

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

    hspi.beginTransaction(hspi_settings);
    hspi.transfer((uint8_t)(value >> 8));
    hspi.transfer((uint8_t)(value & 0xFF));
    hspi.endTransaction();

    digitalWrite(HSPI_CS, HIGH);
}

bool should_redraw = false;

void wavetable_oscillation()
{
    if (osci.total_cycles == 0 || osci.tables[0].samples == NULL)
    {
        return;
    }

#if 1
    uint16_t pitch_analog_value = analogRead(PIN_PITCH_INPUT);
    last_analog_pitch_values[analog_pitch_index] = pitch_analog_value;
    analog_pitch_index =
        plus_one_wrap(analog_pitch_index, LAST_ANALOG_VALUES_SIZE);

    pitch_analog_value = get_last_analog_average(last_analog_pitch_values);
    uint16_t frequency = analog_input_to_pitch(pitch_analog_value);
#else
    uint16_t frequency = analog_input_to_pitch(1000);
#endif

#if 1
    uint16_t selected_cycle_analog_value = g_analog_value =
        analogRead(PIN_WAVETABLE_POSITION);

    last_analog_position_values[analog_position_index] =
        selected_cycle_analog_value;
    analog_position_index =
        plus_one_wrap(analog_position_index, LAST_ANALOG_VALUES_SIZE);

    selected_cycle_analog_value =
        get_last_analog_average(last_analog_position_values);

#if 1
    uint16_t selected_cycle =
        get_cycle_from_analog(selected_cycle_analog_value, osci.total_cycles);
#else

    uint16_t selected_cycle =
        (selected_cycle_analog_value * osci.total_cycles) / MAX_12BIT_VALUE;
#endif
#else
    uint16_t selected_cycle = display_wave_index;
#endif

    frequency = min(frequency, SAMPLE_RATE / 2);

    uint64_t difference = micros() - next_sample_time;
    if (difference >= 0)
    {
        uint16_t value = 0;
        if (xSemaphoreTake(g_oscillator_mutex, portMAX_DELAY))
        {
            uint64_t wavetable_size = osci.samples_per_cycle;
            osci.phase_increment =
                (((uint64_t)frequency * wavetable_size) << 32) / SAMPLE_RATE;

            if (selected_cycle >= osci.total_cycles)
            {
                selected_cycle = osci.total_cycles - 1;
            }

            value = wave_table_linear_interpolation(
                osci.tables + selected_cycle, osci.samples_per_cycle,
                osci.phase);

            wave_table_oscilator_update_phase(&osci);

            xSemaphoreGive(g_oscillator_mutex);
        }

#if 0
        if (millis() >= timer)
        {
            display.fillScreen(SSD1351_BLACK);
            display.setCursor(10, 10);
            display.printf("%u\n", value >> 4);
            timer = millis() + 500;
        }
#endif

        dac_write(value >> 4);

        g_selected_cycle = selected_cycle;

        if (selected_cycle != last_selected_cycle)
        {
            should_redraw = true;
            last_selected_cycle = selected_cycle;
        }
#if 1
        if (should_redraw)
        {
            if (millis() >= timer)
            {
                if (g_redraw_screen_task_handle != NULL)
                {
                    xTaskNotifyGive(g_redraw_screen_task_handle);
                }
                timer = millis() + 150;

                should_redraw = false;
            }
        }
#endif

        next_sample_time += sample_period_us;
    }
}

uint16_t analog_input_to_pitch(uint16_t analog_value)
{
    float voltage = (float)analog_value / MAX_12BIT_VALUE * PITCH_INPUT_RANGE;
    float frequency = C0_FREQUENCY * pow(2, voltage);
    return frequency;
}
