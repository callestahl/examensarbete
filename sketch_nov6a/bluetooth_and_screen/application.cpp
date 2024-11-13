#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include <Arduino.h>
#include <SPI.h>

#include "BluetoothSerial.h"
#include "wave_table.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128
#define PIN_DAC_CS 5
#define PIN_PITCH_INPUT 12
#define PIN_WAVETABLE_POSITION 13

#define WAVETABLE_SIZE 256
#define SAMPLE_RATE 10000
#define MAX_12BIT_VALUE 4095
#define MAX_16BIT_VALUE 65535

#define C0_FREQUENCY 16.35160f
#define PITCH_INPUT_RANGE 10.0f

#define OLED_DC 16
#define OLED_CS 27
#define OLED_RESET 17
#define POSITION_INPUT 34

#define RGB565(r, g, b) (((r << 11)) | (g << 5) | b)
#define SSD1351_BLACK RGB565(0, 0, 0)
#define SSD1351_WHITE RGB565(31, 63, 31)
#define SSD1351_RED RGB565(31, 0, 0)
#define SSD1351_GREEN RGB565(0, 63, 0)
#define SSD1351_BLUE RGB565(0, 0, 31)

struct Button {
  bool button_pressed;
  uint64_t last_debounce_time;
};

void look_for_incoming_bluetooth_messages();

void wave_table_draw(const WaveTable* table, uint32_t table_length);
void clear_screen(int16_t x, int16_t y);
void generate_sine_wave(WaveTable* table, uint32_t table_length);
void redraw_screen();
void process_buttons();
uint16_t analog_input_to_pitch(uint16_t analog_value);

SPIClass hspi(HSPI);

Adafruit_SSD1351 display(SCREEN_WIDTH, SCREEN_HEIGHT, &hspi, OLED_CS, OLED_DC,
                         OLED_RESET);
BluetoothSerial SerialBT;
WaveTableOscillator osci;

static char bluetooth_buffer[1024];
int32_t bluetooth_buffer_size = 0;
bool reading_bluetooth_values = false;

uint64_t millis_to_next_draw;

uint64_t time_mi = 0;
uint32_t text_index = 0;

int16_t cursor_x = 0;
int16_t cursor_y = 0;

bool device_connected = false;
int32_t display_wave_index = 0;
int32_t selected_index = 0;

const int32_t button_pin0 = 4;
const int32_t button_pin1 = 2;
const uint64_t debounce_delay = 50;
Button button0 = {};
Button button1 = {};

bool header_read = false;
int32_t table_index = 0;
int32_t sample_index = 0;

int32_t sample_viewer = 0;

int32_t header_read_count = 0;

bool test = false;

uint64_t bluetooth_timer = 0;
uint64_t bluetooth_time_to_reset = 0;
const uint64_t bluetooth_time_before_reset = 100000;

uint64_t button_repeat_reset = 0;

uint64_t sample_period_us = 1000000 / SAMPLE_RATE;
uint64_t next_sample_time;

void application_setup() {
#if 1
  pinMode(button_pin0, INPUT);
  pinMode(button_pin1, INPUT);

  hspi.begin(14, 12, 13, OLED_CS);
  display.begin();

  display.fillScreen(SSD1351_BLACK);
  display.setTextColor(SSD1351_WHITE);
  display.setTextSize(1);
  display.println("Hello");

  next_sample_time = micros();

#endif
  millis_to_next_draw = millis();

#if 1
  Serial.begin(115200);
  SerialBT.begin("WaveTablePP");
  SerialBT.enableSSP();
#endif

  osci.tables_capacity = 256;
  osci.tables = (WaveTable*)calloc(osci.tables_capacity, sizeof(WaveTable));

#if 0
  osci.tables[0].data = (uint16_t*)calloc(osci.table_length, sizeof(uint16_t));
  osci.tables[1].data = (uint16_t*)calloc(osci.table_length, sizeof(uint16_t));
  osci.tables[2].data = (uint16_t*)calloc(osci.table_length, sizeof(uint16_t));
  generate_sine_wave(&osci.tables[0], osci.table_length);
  generate_sawtooth_wave(&osci.tables[1], osci.table_length);
  generate_square_wave(&osci.tables[2], osci.table_length);

  redraw_screen();
#endif

  // xTaskCreatePinnedToCore(draw_text, "Draws texts", 1024, NULL, 1, NULL,
  // 0);
}

void application_loop() {
#if 1
  look_for_incoming_bluetooth_messages();
  process_buttons();
#endif
}

void look_for_incoming_bluetooth_messages() {
  while (SerialBT.available()) {
    if (!header_read) {
      if (SerialBT.available() >= 2) {
        uint8_t high = SerialBT.read();
        uint8_t low = SerialBT.read();
        uint16_t cycle_sample_count =
          ((uint16_t)high << 8) | ((uint16_t)low);
        osci.samples_per_cycle = cycle_sample_count;
        osci.total_cycles = 0;
        header_read = true;
        table_index = 0;
        sample_index = 0;
      }
    } else {
      if (SerialBT.available() >= 2) {
        uint8_t sample_high = SerialBT.read();
        uint8_t sample_low = SerialBT.read();
        uint16_t sample =
          ((uint16_t)sample_high << 8) | ((uint16_t)sample_low);
        if (table_index < osci.tables_capacity) {
          if (sample_index == 0) {
            if (osci.tables[table_index].samples != NULL) {
              free(osci.tables[table_index].samples);
            }
            osci.tables[table_index].samples = (uint16_t*)calloc(
              osci.samples_per_cycle, sizeof(uint16_t));
            if (osci.tables[table_index].samples != NULL) {
              osci.total_cycles++;
            } else {
              // NOTE(Linus): Logging
            }
          }
          osci.tables[table_index].samples[sample_index++] = sample;
          if (sample_index == osci.samples_per_cycle) {
            sample_index = 0;
            table_index++;

            SerialBT.write(0x08);
          }
        }
      }
      reading_bluetooth_values = true;
    }
    bluetooth_time_to_reset = micros() + bluetooth_time_before_reset;
  }

  if (reading_bluetooth_values) {
    bluetooth_timer = micros();
    if (bluetooth_timer >= bluetooth_time_to_reset) {
      reading_bluetooth_values = false;
      header_read = false;
      table_index = 0;
      sample_index = 0;

      if (osci.total_cycles > 0) {
        display_wave_index = 0;
        redraw_screen();
      }
    }
  }
}

void redraw_screen() {
  display.fillScreen(SSD1351_BLACK);
#if 1

  wave_table_draw(&osci.tables[display_wave_index], osci.samples_per_cycle);

  const uint32_t third_size = SCREEN_WIDTH / 3;
  // display.fillRoundRect(third_size * selected_index, 20 + (SCREEN_HEIGHT /
  // 2), third_size, 36, 5, SSD1351_GREEN);
  display.setCursor(0, 40 + (SCREEN_HEIGHT / 2));
  display.println(display_wave_index);
#endif

  // display.setCursor(0, 0);
  // display.printf("Sample: %u\nIndex: %d\n",
  // osci.tables[0].data[sample_viewer], sample_viewer);

  // display.printf("   %s\n\n", osci.tables[display_wave_index].name);
  // display.printf("   Table size: %u", osci.table_length);
}

const uint32_t screen_width_with_fraction = SCREEN_WIDTH << 16;
const uint32_t window_height_75_procent = (SCREEN_HEIGHT * 3) / 4;

uint32_t y_position_75_procent(uint16_t data) {
  return window_height_75_procent - ((data * window_height_75_procent) / MAX_16BIT_VALUE);
}

void wave_table_draw(const WaveTable* table, uint32_t table_length) {
  const uint32_t x_step = (screen_width_with_fraction / (table_length / 2));

  uint32_t x0 = 0;
  uint32_t y0 =
    window_height_75_procent - ((table->samples[0] * window_height_75_procent) / MAX_16BIT_VALUE);

  for (uint32_t i = 2; i < table_length; i += 2) {
    uint32_t x1 = x0 + x_step;
    uint32_t y1 = y_position_75_procent(table->samples[i]);
    display.drawLine(x0 >> 16, y0, x1 >> 16, y1, SSD1351_RED);

    x0 = x1;
    y0 = y1;
  }
}

void clear_screen(int16_t x, int16_t y) {
  display.fillScreen(SSD1351_BLACK);
  display.setCursor(x, y);
}

void generate_sine_wave(WaveTable* table, uint32_t table_length) {
  for (int i = 0; i < table_length; i++) {
    table->samples[i] = (uint16_t)(MAX_16BIT_VALUE / 2 * (1 + sin(2 * M_PI * i / table_length)));
  }
}

void generate_sawtooth_wave(WaveTable* table, uint32_t table_length) {
  for (int i = 0; i < table_length; i++) {
    table->samples[i] = (uint16_t)((MAX_16BIT_VALUE * i) / table_length);
  }
}

void generate_square_wave(WaveTable* table, uint32_t table_length) {
  for (int i = 0; i < table_length; i++) {
    table->samples[i] = (i < table_length / 2) ? MAX_16BIT_VALUE : 0;
  }
}

void process_buttons() {
  const int32_t button_state0 = digitalRead(button_pin0);
  if (button_state0 == HIGH) {
    if ((millis() >= button_repeat_reset) || (!button0.button_pressed && ((millis() - button0.last_debounce_time) > debounce_delay))) {
      display_wave_index = (display_wave_index + 1) % osci.samples_per_cycle;
      redraw_screen();

      button0.button_pressed = true;
      button0.last_debounce_time = millis();
      button_repeat_reset = millis() + 400;
    }
  } else {
    if (button0.button_pressed && ((millis() - button0.last_debounce_time) > debounce_delay)) {
      button0.button_pressed = false;
      button0.last_debounce_time = millis();
    }
  }

  const int32_t button_state1 = digitalRead(button_pin1);
  if (button_state1 == HIGH) {
    if (!button1.button_pressed && ((millis() - button1.last_debounce_time) > debounce_delay)) {
      display_wave_index = (display_wave_index + (osci.samples_per_cycle - 1)) % osci.samples_per_cycle;
      redraw_screen();

      button1.button_pressed = true;
      button1.last_debounce_time = millis();
    }
  } else {
    if (button1.button_pressed && ((millis() - button1.last_debounce_time) > debounce_delay)) {
      button1.button_pressed = false;
      button1.last_debounce_time = millis();
    }
  }
}

uint16_t last_position = 0;

uint32_t plus_one_wrap(uint32_t value,
                       uint32_t length) {
  return (value + 1) % length;
}

uint32_t minus_one_wrap(uint32_t value, uint32_t length) {
  return (value + (length - 1)) % length;
}

void wavetable_oscillation() {
  if (osci.total_cycles == 0 || osci.tables[0].samples == NULL) {
    return;
  }

  uint32_t wavetable_size = osci.samples_per_cycle;

  //uint16_t pitch_analog_value = analogRead(PIN_PITCH_INPUT);
  uint16_t frequency = analog_input_to_pitch(3000);

  //uint16_t selected_cycle_analog_value = analogRead(PIN_WAVETABLE_POSITION);
  //uint16_t selected_cycle = (selected_cycle_analog_value * osci.samples_per_cycle) / MAX_12BIT_VALUE;
  uint16_t selected_cycle = display_wave_index;

  osci.phase_increment =
    ((uint64_t)frequency * wavetable_size << 32) / SAMPLE_RATE;

  uint64_t current_time = micros();

  if (current_time >= next_sample_time) {
    const uint32_t phase_index = osci.phase >> 32;
    const uint32_t phase_fraction = (osci.phase & 0xFFFFFFFF) >> 16;
    const uint32_t last_index =
      minus_one_wrap(phase_index, wavetable_size);
    const uint32_t x_step = (screen_width_with_fraction / wavetable_size);
    const uint32_t last_x = x_step * last_index;
    const uint32_t last_y =
      y_position_75_procent(osci.tables->samples[last_index]);
    const uint32_t current_x = x_step * phase_index;
    const uint32_t current_y =
      y_position_75_procent(osci.tables->samples[phase_index]);

    uint16_t value = wave_table_linear_interpolation(
      osci.tables + selected_cycle, osci.samples_per_cycle, osci.phase);

    //dac.write(value >> 4, 0);

    wave_table_oscilator_update_phase(&osci);

    next_sample_time += sample_period_us;
  }
}

uint16_t analog_input_to_pitch(uint16_t analog_value) {
  float voltage = (float)analog_value / MAX_12BIT_VALUE * PITCH_INPUT_RANGE;
  float frequency = C0_FREQUENCY * pow(2, voltage);
  return frequency;
}
