#include <hardware/adc.h>
#include <hardware/gpio.h>
#include <math.h>
#include <pico/stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "MCP4822.h"
#include "pinDefinitions.h"

#define WAVETABLE_SIZE 256
#define SAMPLE_RATE 44100
#define MAX_12BIT_VALUE 4095
#define MAX_16BIT_VALUE 65535

uint16_t sine_wave[WAVETABLE_SIZE];
bool led_state = false;

MCP4822 mcp;

static uint64_t phase = 0;
uint64_t phase_increment;
uint64_t sample_period_us = 1000000 / SAMPLE_RATE;
uint64_t next_sample_time;

void setup();
void loop();
void generate_sine_wave();
void wavetable_oscillation(uint16_t* wavetable);

void setup() {
  stdio_init_all();

  gpio_init(ONBOARD_LED);
  gpio_set_dir(ONBOARD_LED, true);
  gpio_put(ONBOARD_LED, led_state);

  generate_sine_wave();

  spi_inst_t* spi = spi0;
  spi_init(spi, 1000000);
  gpio_set_function(SCK, GPIO_FUNC_SPI);
  gpio_set_function(TX, GPIO_FUNC_SPI);
  MCP4822_init(&mcp, spi, CS);

  adc_init();
  adc_gpio_init(26);
  adc_select_input(0);

  next_sample_time = time_us_64();
}

void loop() {
  while (true) {
    wavetable_oscillation(sine_wave);
    // MCP4822_setOutput(&mcp, 0, MAX_12BIT_VALUE);
  }
}

int main() {
  setup();
  loop();
}

void generate_sine_wave() {
  for (int i = 0; i < WAVETABLE_SIZE; i++) {
    sine_wave[i] = (uint16_t)(MAX_16BIT_VALUE / 2 *
                              (1 + sin(2 * M_PI * i / WAVETABLE_SIZE)));
  }
}

void wavetable_oscillation(uint16_t* wavetable) {
  uint16_t adc_value = adc_read();

  phase_increment = ((uint64_t)adc_value * WAVETABLE_SIZE << 32) / SAMPLE_RATE;

  uint64_t current_time = time_us_64();
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
    uint16_t value = sample1 + ((sample2 - sample1) * phase_fraction >> 16);

    // Output the interpolated value to the DAC
    MCP4822_setOutput(&mcp, 0, value >> 4);  // Scale 16-bit value to 12-bit DAC

    // Increment the phase
    phase += phase_increment;
    if (phase >= ((uint64_t)WAVETABLE_SIZE << 32)) {
      phase -= ((uint64_t)WAVETABLE_SIZE << 32);
    }

    // Update the next sample time
    next_sample_time += sample_period_us;
  }
}