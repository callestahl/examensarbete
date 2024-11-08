#include "MCP_DAC.h"

#define PIN_DAC_CS 5
#define PIN_PITCH_INPUT 12
#define PIN_WAVETABLE_POSITION 13 

#define WAVETABLE_SIZE 256
#define SAMPLE_RATE 44100
#define MAX_12BIT_VALUE 4095
#define MAX_16BIT_VALUE 65535

#define C0_FREQUENCY 16.35160f
#define PITCH_INPUT_RANGE 10.0f 

SPIClass spi = SPIClass(VSPI);
MCP4822 dac(&spi);

uint16_t dac_value = 0;

void generate_sine_wave();
void wavetable_oscillation(uint16_t* wavetable);
uint16_t analog_input_to_pitch(uint16_t analog_value);

uint16_t sine_wave[WAVETABLE_SIZE];

static uint64_t phase = 0;
uint64_t phase_increment;
uint64_t sample_period_us = 1000000 / SAMPLE_RATE;
uint64_t next_sample_time;

void setup() {
  Serial.begin(9600);

  pinMode(2, OUTPUT);
  pinMode(PIN_PITCH_INPUT, INPUT);
  pinMode(PIN_WAVETABLE_POSITION, INPUT);
  digitalWrite(2, HIGH);
  
  generate_sine_wave();

  spi.begin();
  dac.begin(PIN_DAC_CS);

  next_sample_time = micros();
}

uint16_t val = 0;

void loop() {
  wavetable_oscillation(sine_wave);
  Serial.println(analogRead(PIN_WAVETABLE_POSITION));
}

void generate_sine_wave() {
  for (int i = 0; i < WAVETABLE_SIZE; i++) {
    sine_wave[i] = (uint16_t)(MAX_16BIT_VALUE / 2 *
                              (1 + sin(2 * M_PI * i / WAVETABLE_SIZE)));
  }
}


void wavetable_oscillation(uint16_t* wavetable) {
  uint16_t adc_value = analogRead(PIN_PITCH_INPUT);
  uint16_t frequency = analog_input_to_pitch(adc_value);
  //Serial.println(frequency);

  //phase_increment = ((uint64_t)adc_value * WAVETABLE_SIZE << 32) / SAMPLE_RATE;
  phase_increment = ((uint64_t)frequency * WAVETABLE_SIZE << 32) / SAMPLE_RATE;

  uint64_t current_time = micros();
  if (current_time >= next_sample_time) {
    // Calculate the integer and fractional parts of the phase
    uint32_t phase_int = phase >> 32;
    uint32_t phase_fraction =
        (phase & 0xFFFFFFFF) >>
        16;  // Use the upper 16 bits of the fractional part

    // Get the current and next sample values
    uint16_t sample1 = wavetable[phase_int];
    uint16_t sample2 = wavetable[(phase_int + 1) % WAVETABLE_SIZE];

    // Perform linear interpolation
    uint16_t value = sample1 + (((sample2 - sample1) * phase_fraction) >> 16);
    // Output the interpolated value to the DAC
    //MCP4822_setOutput(&mcp, 0, value >> 4);  // Scale 16-bit value to 12-bit DAC

    dac.write(value >> 4, 0);

    // Increment the phase
    phase += phase_increment;
    if (phase >= ((uint64_t)WAVETABLE_SIZE << 32)) {
      phase -= ((uint64_t)WAVETABLE_SIZE << 32);
    }

    // Update the next sample time
    next_sample_time += sample_period_us;
  }
}

uint16_t analog_input_to_pitch(uint16_t analog_value) {
  float voltage = (float)analog_value / MAX_12BIT_VALUE * PITCH_INPUT_RANGE;
  float frequency = C0_FREQUENCY * pow(2, voltage);
  return frequency;
}
