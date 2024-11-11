#include "MCP_DAC.h"
#include "BluetoothSerial.h"

#define PIN_DAC_CS 5
#define PIN_PITCH_INPUT 12
#define PIN_WAVETABLE_POSITION 13 

#define WAVETABLE_SIZE 256
#define SAMPLE_RATE 10000
#define MAX_12BIT_VALUE 4095
#define MAX_16BIT_VALUE 65535

#define C0_FREQUENCY 16.35160f
#define PITCH_INPUT_RANGE 10.0f 

struct WaveTable {
  const char* name;
  uint16_t* data;
};

struct WaveTableOscillator {
  uint64_t phase;
  uint64_t phase_increment;

  uint32_t samples_per_cycle;
  uint32_t max_samples_per_cycle;
  uint32_t total_cycles;
  WaveTable* tables;
};

uint64_t last_sample_time;

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

SPIClass spi = SPIClass(VSPI);
MCP4822 dac(&spi);

uint16_t dac_value = 0;

void generate_sine_wave();
void wavetable_oscillation();
uint16_t analog_input_to_pitch(uint16_t analog_value);

uint16_t sine_wave[WAVETABLE_SIZE];

static uint64_t phase = 0;
uint64_t phase_increment;
uint64_t sample_period_us = 1000000 / SAMPLE_RATE;
uint64_t next_sample_time;

void setup() {
  Serial.begin(9600);

  last_sample_time = micros();

  pinMode(2, OUTPUT);
  pinMode(PIN_PITCH_INPUT, INPUT);
  pinMode(PIN_WAVETABLE_POSITION, INPUT);
  digitalWrite(2, HIGH);
  
  generate_sine_wave();

  spi.begin();
  dac.begin(PIN_DAC_CS);

  SerialBT.begin("WaveTablePP_2");
  SerialBT.enableSSP();

  next_sample_time = micros();

  osci.max_samples_per_cycle = 256;
  osci.tables = (WaveTable*)calloc(osci.max_samples_per_cycle, sizeof(WaveTable));
}

uint16_t val = 0;

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
        uint16_t sample = ((uint16_t)sample_high << 8) | ((uint16_t)sample_low);
        if (table_index < osci.max_samples_per_cycle) {
          if (sample_index == 0) {
            if (osci.tables[table_index].data != NULL) {
              free(osci.tables[table_index].data);
            }
            osci.tables[table_index].data = (uint16_t*)calloc(osci.samples_per_cycle, sizeof(uint16_t));
            if (osci.tables[table_index].data != NULL) {
              osci.total_cycles++;
            } else {
              // NOTE(Linus): Logging
            }
          }
          osci.tables[table_index].data[sample_index++] = sample;
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
        if (osci.total_cycles > 0 && osci.tables[0].data != NULL) {
        Serial.println(osci.samples_per_cycle);
        Serial.println(osci.max_samples_per_cycle);
        Serial.println(osci.total_cycles);
        for (uint32_t i = 0; i <  osci.samples_per_cycle; i++) {
          Serial.print(i);
          Serial.print(": ");
          Serial.println(osci.tables[0].data[i]);
        }
}

        //redraw_screen();
      }
    }
  }
#endif
  if (!reading_bluetooth_values) {
    wavetable_oscillation();
  }
//Serial.println(micros());
  
  //Serial.println(analogRead(PIN_WAVETABLE_POSITION));
}

void generate_sine_wave() {
  for (int i = 0; i < WAVETABLE_SIZE; i++) {
    sine_wave[i] = (uint16_t)(MAX_16BIT_VALUE / 2 *
                              (1 + sin(2 * M_PI * i / WAVETABLE_SIZE)));
  }
}


void wavetable_oscillation() {
  // Check if there's at least one wavetable available
  if (osci.total_cycles == 0 || osci.tables[0].data == NULL) {
    //Serial.println("No wavetable available.");
    return;
  }

  uint16_t* wavetable = osci.tables[0].data;  // Use the first wavetable
  uint32_t wavetable_size = osci.samples_per_cycle;

  uint16_t pitch_analog_value = analogRead(PIN_PITCH_INPUT);
  uint16_t frequency = analog_input_to_pitch(pitch_analog_value);

  uint16_t selected_cycle_analog_value = analogRead(PIN_WAVETABLE_POSITION);
  uint8_t selected_cycle = (selected_cycle_analog_value * osci.total_cycles) / MAX_12BIT_VALUE;

  // Calculate the phase increment based on frequency and wavetable size
  phase_increment = ((uint64_t)frequency * wavetable_size << 32) / SAMPLE_RATE;

  uint64_t current_time = micros();

  if (current_time >= next_sample_time) {
  uint32_t phase_int = phase >> 32;
  uint32_t phase_fraction = (phase & 0xFFFFFFFF) >> 16;

  // Get the current and next sample values, wrapping the index as needed
  uint16_t sample1 = wavetable[selected_cycle * phase_int];
  uint16_t sample2 = wavetable[((selected_cycle * phase_int) + 1) % wavetable_size];

  // Perform linear interpolation
  uint16_t value = sample1 + (((sample2 - sample1) * phase_fraction) >> 16);

  // Output the interpolated value to the DAC, scaled to 12-bit range
  dac.write(value >> 4, 0);

  // Increment the phase, wrapping it at the end of the cycle
  phase += phase_increment;
  if (phase >= ((uint64_t)wavetable_size << 32)) {
    phase -= ((uint64_t)wavetable_size << 32);
  }

  // Update the next sample time
  next_sample_time += sample_period_us;
  }
}

// void wavetable_oscillation() {
//     if (osci.total_cycles == 0 || osci.tables[0].data == NULL) {
//     //Serial.println("No wavetable available.");
//     return;
//   }

//   uint16_t* wavetable = osci.tables[0].data;  // Use the first wavetable
//   uint32_t wavetable_size = osci.samples_per_cycle;

//   uint16_t pitch_analog_value = analogRead(PIN_PITCH_INPUT);
//   uint16_t frequency = analog_input_to_pitch(pitch_analog_value);

//   uint16_t selected_cycle_analog_value = analogRead(PIN_WAVETABLE_POSITION);
//   uint8_t selected_cycle = (selected_cycle_analog_value * osci.total_cycles) / MAX_12BIT_VALUE;

//   // Calculate the phase increment based on frequency and wavetable size
//   phase_increment = ((uint64_t)frequency * (uint64_t)wavetable_size << 32) / SAMPLE_RATE;


//   uint64_t current_time = micros();

//   if (current_time >= next_sample_time) {
//   uint32_t phase_int = phase >> 32;
//   uint32_t phase_fraction = (phase & 0xFFFFFFFF) >> 16;

//   // Get the current and next sample values, wrapping the index as needed
//   uint16_t sample1 = wavetable[selected_cycle * phase_int];
//   uint16_t sample2 = wavetable[((selected_cycle * phase_int) + 1) % wavetable_size];

//   // Perform linear interpolation
//   uint16_t value = sample1 + (((sample2 - sample1) * phase_fraction) >> 16);

//   // Output the interpolated value to the DAC, scaled to 12-bit range
//   dac.write(value >> 4, 0);

//   // Increment the phase, wrapping it at the end of the cycle
//   phase += phase_increment;
//   if (phase >= wavetable_size) {
//     phase -= wavetable_size;
//   }

//   // Update the next sample time
//   next_sample_time += sample_period_us;
// }
// }



uint16_t analog_input_to_pitch(uint16_t analog_value) {
  float voltage = (float)analog_value / MAX_12BIT_VALUE * PITCH_INPUT_RANGE;
  float frequency = C0_FREQUENCY * pow(2, voltage);
  return frequency;
}
