#include <SPI.h>
#include "BluetoothSerial.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>  // Use Adafruit_SSD1331.h if your display uses SSD1331

// Display dimensions for a 1.5-inch OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128
#define MAX_12BIT_VALUE 4095
#define MAX_16BIT_VALUE 65535

// Updated OLED pin definitions
#define OLED_DC 16     // Data/Command pin on GPIO 16
#define OLED_CS 5      // Chip Select pin (you can set this to any available GPIO)
#define OLED_RESET 17  // Reset pin on GPIO 17
#define POSITION_INPUT 34

#define RGB565(r, g, b) (((r << 11)) | (g << 5) | b)
#define SSD1351_BLACK RGB565(0, 0, 0)
#define SSD1351_WHITE RGB565(31, 63, 31)
#define SSD1351_RED RGB565(31, 0, 0)
#define SSD1351_GREEN RGB565(0, 63, 0)
#define SSD1351_BLUE RGB565(0, 0, 31)

struct WaveTable {
  const char* name;
  uint16_t* data;
};

struct WaveTableOscillator {
  uint64_t phase;
  uint64_t phase_increment;

  uint32_t table_length;
  uint32_t table_count;
  WaveTable* tables;
};

uint16_t lerp(uint16_t a, uint16_t b, uint32_t fraction);
uint16_t wave_table_oscilator_linear_interpolation(const WaveTable* table, uint32_t table_length, uint64_t phase);
void wave_table_oscilator_update_phase(WaveTableOscillator* oscilator);
void wave_table_draw(const WaveTable* table, uint32_t table_length);
void clear_screen(int16_t x, int16_t y);
void generate_sine_wave(WaveTable* table, uint32_t table_length);
void redraw_screen();

Adafruit_SSD1351 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_CS, OLED_DC, OLED_RESET);
BluetoothSerial SerialBT;

WaveTableOscillator osci;

static char bluetooth_buffer[1024];
int32_t bluetooth_buffer_size = 0;
bool reading_bluetooth_values = false;

uint64_t millis_to_next_draw;
const char* texts[] = { "Hello", "This", "is", "ff" };

uint64_t time_mi = 0;
uint32_t text_index = 0;

int16_t cursor_x = 0;
int16_t cursor_y = 0;

bool device_connected = false;

const int32_t button_pin = 4;
bool button_pressed = false;
int32_t display_wave_index = 0;
uint64_t last_debounce_time = 0;
const uint64_t debounce_delay = 50;

void setup() {
#if 1
  pinMode(button_pin, INPUT);

  display.begin();

  display.fillScreen(SSD1351_BLACK);
  display.setTextColor(SSD1351_WHITE);
  display.setTextSize(1);
  display.print("Hello");


#endif
  millis_to_next_draw = millis();

  Serial.begin(115200);
  SerialBT.begin("WaveTablePP");

#if 0
  osci.table_length = 256;
  osci.table_count = 3;
  osci.tables = (WaveTable*)calloc(osci.table_count, sizeof(WaveTable));
  osci.tables[0].data = (uint16_t*)calloc(osci.table_length, sizeof(uint16_t));
  osci.tables[1].data = (uint16_t*)calloc(osci.table_length, sizeof(uint16_t));
  osci.tables[2].data = (uint16_t*)calloc(osci.table_length, sizeof(uint16_t));
  generate_sine_wave(&osci.tables[0], osci.table_length);
  generate_sawtooth_wave(&osci.tables[1], osci.table_length);
  generate_square_wave(&osci.tables[2], osci.table_length);

  redraw_screen();
#endif


  //xTaskCreatePinnedToCore(draw_text, "Draws texts", 1024, NULL, 1, NULL, 0);
}

void loop() {
  if (SerialBT.available()) {
    char c = SerialBT.read();
    if(bluetooth_buffer_size < 1024) {
      bluetooth_buffer[bluetooth_buffer_size++] = c;
      bluetooth_buffer[bluetooth_buffer_size] = '\0';
    }
    reading_bluetooth_values = true;
  } else if (reading_bluetooth_values) {
    clear_screen(10, 10);
    display.printf("%s\n", bluetooth_buffer);
    for (int32_t i = 0; i < bluetooth_buffer_size; ++i) {
      bluetooth_buffer[i] = '\0';
    }
    bluetooth_buffer_size = 0;
    reading_bluetooth_values = false;
  }

#if 0
  const int32_t button_state = digitalRead(button_pin);
  if (button_state == HIGH) {
    if (!button_pressed && ((millis() - last_debounce_time) > debounce_delay)) {
      display_wave_index = (display_wave_index + 1) % osci.table_count;
      redraw_screen();

      button_pressed = true;
      last_debounce_time = millis();
    }
  } else {
    if (button_pressed && ((millis() - last_debounce_time) > debounce_delay)) {
      button_pressed = false;
      last_debounce_time = millis();
    }
  }
#endif
}

void redraw_screen() {
  display.fillScreen(SSD1351_BLACK);
  wave_table_draw(&osci.tables[display_wave_index], osci.table_length);

  display.setCursor(0, 40 + (SCREEN_HEIGHT / 2));
  display.printf("   %s\n\n", osci.tables[display_wave_index].name);
  display.printf("   Table size: %u", osci.table_length);
}

uint16_t lerp(uint16_t a, uint16_t b, uint32_t fraction) {
  return a + (((b - a) * fraction) >> 16);
}

uint16_t wave_table_oscilator_linear_interpolation(const WaveTable* table, uint32_t table_length, uint64_t phase) {
  const uint32_t phase_index = phase >> 32;
  const uint32_t phase_fraction = (phase & 0xFFFFFFFF) >> 16;
  const uint16_t sample0 = table->data[phase_index];
  const uint16_t sample1 = table->data[(phase_index + 1) % table_length];
  return lerp(sample0, sample1, phase_fraction);
}

void wave_table_oscilator_update_phase(WaveTableOscillator* oscilator) {
  oscilator->phase += oscilator->phase_increment;
  if (oscilator->phase >= ((uint64_t)oscilator->table_length << 32)) {
    oscilator->phase -= ((uint64_t)oscilator->table_length << 32);
  }
}

void wave_table_draw(const WaveTable* table, uint32_t table_length) {
  const uint32_t half_window_width = SCREEN_WIDTH / 2;
  const uint32_t x_step = (SCREEN_WIDTH / (table_length / 2));
  const uint32_t half_window_height = SCREEN_HEIGHT / 2;

  uint32_t x0 = 0;
  uint32_t y0 = half_window_height - ((table->data[0] * half_window_height) / MAX_16BIT_VALUE);

  for (uint32_t i = 2; i < table_length; i += 2) {
    uint32_t x1 = x0 + x_step;
    uint32_t y1 = half_window_height - ((table->data[i] * half_window_height) / MAX_16BIT_VALUE);

    display.drawLine(x0, y0, x1, y1, SSD1351_RED);

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
    table->data[i] = (uint16_t)(MAX_16BIT_VALUE / 2 * (1 + sin(2 * M_PI * i / table_length)));
  }
  table->name = "Sine wave";
}

void generate_sawtooth_wave(WaveTable* table, uint32_t table_length) {
  for (int i = 0; i < table_length; i++) {
    table->data[i] = (uint16_t)((MAX_16BIT_VALUE * i) / table_length);
  }
  table->name = "Sawtooth wave";
}

void generate_square_wave(WaveTable* table, uint32_t table_length) {
  for (int i = 0; i < table_length; i++) {
    table->data[i] = (i < table_length / 2) ? MAX_16BIT_VALUE : 0;
  }
  table->name = "Square wave";
}
