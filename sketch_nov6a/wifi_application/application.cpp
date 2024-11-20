#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include <SPI.h>

#include "MCP_DAC.h"

#include "wave_table.h"
#include "server.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128

#define PIN_DAC_CS 5
#define PIN_PITCH_INPUT 32
#define PIN_WAVETABLE_POSITION 33

#define WAVETABLE_SIZE 256
#define SAMPLE_RATE 44100
#define MAX_12BIT_VALUE 4095
#define MAX_16BIT_VALUE 65535

#define C0_FREQUENCY 16.35160f
#define PITCH_INPUT_RANGE 10.0f

#define OLED_DC 16
#define OLED_CS 15
#define OLED_RESET 17

#define RGB565(r, g, b) (((r << 11)) | (g << 5) | b)
#define SSD1351_BLACK RGB565(0, 0, 0)
#define SSD1351_WHITE RGB565(31, 63, 31)
#define SSD1351_RED RGB565(31, 0, 0)
#define SSD1351_GREEN RGB565(0, 63, 0)
#define SSD1351_BLUE RGB565(0, 0, 31)

#define STACK_SIZE 2048
#define QUEUE_LENGTH 10


struct Button
{
    bool button_pressed;
    uint64_t last_debounce_time;
};

void wave_table_draw(const WaveTable* table, uint32_t table_length);
void clear_screen(int16_t x, int16_t y);
void generate_sine_wave(WaveTable* table, uint32_t table_length);
void redraw_screen(uint16_t cycle_index);
void process_buttons();
void wavetable_oscillation();
uint16_t analog_input_to_pitch(uint16_t analog_value);

SPIClass spi = SPIClass(VSPI);
MCP4822 dac(&spi);

uint16_t dac_value = 0;

SPIClass hspi(HSPI);

Adafruit_SSD1351 display(SCREEN_WIDTH, SCREEN_HEIGHT, &hspi, OLED_CS, OLED_DC,
                         OLED_RESET);
WaveTableOscillator osci;
int32_t display_wave_index = 0;
int32_t selected_index = 0;

const int32_t button_pin0 = 4;
const int32_t button_pin1 = 2;
const uint64_t debounce_delay = 50;
Button button0 = {};
Button button1 = {};

int32_t sample_viewer = 0;

uint64_t button_repeat_reset = 0;

uint64_t sample_period_us = 1000000 / SAMPLE_RATE;
uint64_t next_sample_time;

QueueHandle_t cycle_queue;

volatile uint16_t g_selected_cycle = 0;

void redraw_screen_task(void* data)
{
    uint16_t last_drawn_cycle = MAX_16BIT_VALUE;
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(100));

        if (g_selected_cycle != last_drawn_cycle)
        {
            last_drawn_cycle = g_selected_cycle;
            if (osci.total_cycles > 0)
            {
                redraw_screen(last_drawn_cycle);
            }
        }
    }
}

void application_setup()
{
    Serial.begin(115200);

    pinMode(button_pin0, INPUT);
    pinMode(button_pin1, INPUT);

    pinMode(PIN_PITCH_INPUT, INPUT);
    pinMode(PIN_WAVETABLE_POSITION, INPUT);

    hspi.begin(14, 12, 13, OLED_CS);
    display.begin();

    spi.begin();
    dac.begin(PIN_DAC_CS);

    display.fillScreen(SSD1351_BLACK);
    display.setTextColor(SSD1351_WHITE);
    display.setTextSize(1);
    display.println("Hello1");

    server_setup(&display);

    next_sample_time = micros();

    osci.tables_capacity = 256;
    osci.tables = (WaveTable*)calloc(osci.tables_capacity, sizeof(WaveTable));

    xTaskCreate(redraw_screen_task, "Screen Redraw", STACK_SIZE, NULL, 1, NULL);
}

void application_loop()
{
#if 0
    if(server_loop(&osci))
    {
        display_wave_index = 0;
        redraw_screen(0);
    }
#endif
    process_buttons();
    if (osci.total_cycles > 0)
    {
        wavetable_oscillation();
    }
}

void redraw_screen(uint16_t cycle_index)
{
    display.fillScreen(SSD1351_BLACK);

    wave_table_draw(&osci.tables[cycle_index], osci.samples_per_cycle);

    display.setCursor(0, 40 + (SCREEN_HEIGHT / 2));
    display.printf("Position: %u\n", cycle_index);
}

const uint32_t screen_width_with_fraction = SCREEN_WIDTH << 16;
const uint32_t window_height_75_procent = (SCREEN_HEIGHT * 3) / 4;

uint32_t y_position_75_procent(uint16_t data)
{
    return window_height_75_procent -
           ((data * window_height_75_procent) / MAX_16BIT_VALUE);
}

void wave_table_draw(const WaveTable* table, uint32_t table_length)
{
    const uint32_t x_step = (screen_width_with_fraction / (table_length / 2));

    uint32_t x0 = 0;
    uint32_t y0 =
        window_height_75_procent -
        ((table->samples[0] * window_height_75_procent) / MAX_16BIT_VALUE);

    for (uint32_t i = 2; i < table_length; i += 2)
    {
        uint32_t x1 = x0 + x_step;
        uint32_t y1 = y_position_75_procent(table->samples[i]);
        display.drawLine(x0 >> 16, y0, x1 >> 16, y1, SSD1351_RED);

        x0 = x1;
        y0 = y1;
    }
}

void clear_screen(int16_t x, int16_t y)
{
    display.fillScreen(SSD1351_BLACK);
    display.setCursor(x, y);
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

void process_buttons()
{
    const int32_t button_state0 = digitalRead(button_pin0);
    if (button_state0 == HIGH)
    {
        if ((millis() >= button_repeat_reset) ||
            (!button0.button_pressed &&
             ((millis() - button0.last_debounce_time) > debounce_delay)))
        {
            display_wave_index =
                plus_one_wrap(display_wave_index, osci.total_cycles);

            button0.button_pressed = true;
            button0.last_debounce_time = millis();
            button_repeat_reset = millis() + 200;
        }
    }
    else
    {
        if (button0.button_pressed &&
            ((millis() - button0.last_debounce_time) > debounce_delay))
        {
            button0.button_pressed = false;
            button0.last_debounce_time = millis();
        }
    }

    const int32_t button_state1 = digitalRead(button_pin1);
    if (button_state1 == HIGH)
    {
        if (!button1.button_pressed &&
            ((millis() - button1.last_debounce_time) > debounce_delay))
        {
            display_wave_index =
                minus_one_wrap(display_wave_index, osci.total_cycles);

            button1.button_pressed = true;
            button1.last_debounce_time = millis();
        }
    }
    else
    {
        if (button1.button_pressed &&
            ((millis() - button1.last_debounce_time) > debounce_delay))
        {
            button1.button_pressed = false;
            button1.last_debounce_time = millis();
        }
    }
}

uint16_t last_selected_cycle = MAX_16BIT_VALUE;

uint64_t timer = millis();

const uint8_t LAST_ANALOG_VALUES_SIZE = 50;

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

void wavetable_oscillation()
{
    if (osci.total_cycles == 0 || osci.tables[0].samples == NULL)
    {
        return;
    }

    uint64_t wavetable_size = osci.samples_per_cycle;

#if 0
    uint16_t pitch_analog_value = analogRead(PIN_PITCH_INPUT);
    last_analog_pitch_values[analog_pitch_index] = pitch_analog_value;
    analog_pitch_index =
        plus_one_wrap(analog_pitch_index, LAST_ANALOG_VALUES_SIZE);

    pitch_analog_value = get_last_analog_average(last_analog_pitch_values);
    uint16_t frequency = analog_input_to_pitch(pitch_analog_value);
#else
    uint16_t frequency = analog_input_to_pitch(1000);
#endif

#if 0
    uint16_t selected_cycle_analog_value = analogRead(PIN_WAVETABLE_POSITION);

    last_analog_position_values[analog_position_index] =
        selected_cycle_analog_value;
    analog_position_index =
        plus_one_wrap(analog_position_index, LAST_ANALOG_VALUES_SIZE);

    selected_cycle_analog_value =
        get_last_analog_average(last_analog_position_values);

    uint16_t selected_cycle =
        (selected_cycle_analog_value * osci.total_cycles) / MAX_12BIT_VALUE;
#else
    uint16_t selected_cycle = display_wave_index;
#endif

    frequency = min(frequency, SAMPLE_RATE / 2);
    osci.phase_increment =
        (((uint64_t)frequency * wavetable_size) << 32) / SAMPLE_RATE;

    uint64_t current_time = micros();

#if 0
    if (millis() >= timer)
    {
        display.fillScreen(SSD1351_BLACK);
        display.setCursor(10, 10);
        display.printf("analog: %u\n", pitch_analog_value);
        display.printf("Fre: %u\n", (uint32_t)frequency);
        timer = millis() + 100;
    }
#endif

    uint64_t difference = current_time - next_sample_time;
    if (difference >= 0)
    {
        if (selected_cycle >= osci.total_cycles)
        {
            selected_cycle = osci.total_cycles - 1;
        }
        g_selected_cycle = selected_cycle;

        if (selected_cycle != last_selected_cycle)
        {
            // redraw_screen(selected_cycle);
            last_selected_cycle = selected_cycle;
        }

        uint16_t value = wave_table_linear_interpolation(
            osci.tables + selected_cycle, osci.samples_per_cycle, osci.phase);

        //dac.write(value >> 4, 0);

        wave_table_oscilator_update_phase(&osci);

        next_sample_time += sample_period_us;
    }
}

uint16_t analog_input_to_pitch(uint16_t analog_value)
{
    float voltage = (float)analog_value / MAX_12BIT_VALUE * PITCH_INPUT_RANGE;
    float frequency = C0_FREQUENCY * pow(2, voltage);
    return frequency;
}
