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
  uint32_t table_capacity;
  uint32_t table_count;
  WaveTable* tables;
};

struct Button {
  bool button_pressed;
  uint64_t last_debounce_time;
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


void setup() {
#if 1
  pinMode(button_pin0, INPUT);
  pinMode(button_pin1, INPUT);

  display.begin();

  display.fillScreen(SSD1351_BLACK);
  display.setTextColor(SSD1351_WHITE);
  display.setTextSize(1);


#endif
  millis_to_next_draw = millis();

#if 1
  Serial.begin(115200);
  SerialBT.begin("WaveTablePP");
  SerialBT.enableSSP();
#endif


  osci.table_capacity = 256;
  osci.tables = (WaveTable*)calloc(osci.table_capacity, sizeof(WaveTable));

#if 0
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

bool header_read = false;
int32_t table_index = 0;
int32_t sample_index = 0;

int32_t sample_viewer = 0;

int32_t header_read_count = 0;

bool test = false;

uint64_t bluetooth_timer = 0;
uint64_t bluetooth_time_to_reset = 0;
const uint64_t bluetooth_time_before_reset = 100000;


void loop() {
#if 1
  while (SerialBT.available()) {
    if (!header_read) {
      if (SerialBT.available() >= 2) {
        uint8_t high = SerialBT.read();
        uint8_t low = SerialBT.read();
        uint16_t cycle_sample_count = ((uint16_t)high << 8) | ((uint16_t)low);
        osci.table_length = cycle_sample_count;
        osci.table_count = 0;
        header_read = true;
        table_index = 0;
        sample_index = 0;
      }
    } else {
      if (SerialBT.available() >= 2) {
        uint8_t sample_high = SerialBT.read();
        uint8_t sample_low = SerialBT.read();
        uint16_t sample = ((uint16_t)sample_high << 8) | ((uint16_t)sample_low);
        if (table_index < osci.table_capacity) {
          if (sample_index == 0) {
            if (osci.tables[table_index].data != NULL) {
              free(osci.tables[table_index].data);
            }
            osci.tables[table_index].data = (uint16_t*)calloc(osci.table_length, sizeof(uint16_t));
            if (osci.tables[table_index].data != NULL) {
              osci.table_count++;
            } else {
              // NOTE(Linus): Logging
            }
          }
          osci.tables[table_index].data[sample_index++] = sample;
          if (sample_index == osci.table_length) {
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

      if (osci.table_count > 0) {
        display_wave_index = 0;
        redraw_screen();
      }
    }
  }
#endif

#if 1
  const int32_t button_state0 = digitalRead(button_pin0);
  if (button_state0 == HIGH) {
    if (!button0.button_pressed && ((millis() - button0.last_debounce_time) > debounce_delay)) {
      //display.println("Button0");
      display_wave_index = (display_wave_index + 1) % osci.table_count;
      redraw_screen();

      button0.button_pressed = true;
      button0.last_debounce_time = millis();
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
      //display.println("Button1");
      display_wave_index = (display_wave_index + (osci.table_count - 1)) % osci.table_count;
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
#endif
}

void redraw_screen() {
  display.fillScreen(SSD1351_BLACK);
#if 1

  wave_table_draw(&osci.tables[display_wave_index], osci.table_length);

  const uint32_t third_size = SCREEN_WIDTH / 3;
  //display.fillRoundRect(third_size * selected_index, 20 + (SCREEN_HEIGHT / 2), third_size, 36, 5, SSD1351_GREEN);
  display.setCursor(0, 40 + (SCREEN_HEIGHT / 2));
  display.println(display_wave_index);
#endif

  //display.setCursor(0, 0);
  //display.printf("Sample: %u\nIndex: %d\n", osci.tables[0].data[sample_viewer], sample_viewer);

  //display.printf("   %s\n\n", osci.tables[display_wave_index].name);
  //display.printf("   Table size: %u", osci.table_length);
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
  const uint32_t screen_width_with_fraction = SCREEN_WIDTH << 16;
  const uint32_t x_step = (screen_width_with_fraction / (table_length / 2));
  const uint32_t window_height_75_procent = (SCREEN_HEIGHT * 3) / 4;

  uint32_t x0 = 0;
  uint32_t y0 = window_height_75_procent - ((table->data[0] * window_height_75_procent) / MAX_16BIT_VALUE);

  for (uint32_t i = 2; i < table_length; i += 2) {
    uint32_t x1 = x0 + x_step;
    uint32_t y1 = window_height_75_procent - ((table->data[i] * window_height_75_procent) / MAX_16BIT_VALUE);

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
