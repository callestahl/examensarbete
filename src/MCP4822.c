#include "MCP4822.h"

#include "hardware/gpio.h"

void MCP4822_init(MCP4822* mcp, spi_inst_t* spi, uint cs_pin) {
  mcp->spi = spi;
  mcp->cs_pin = cs_pin;
  gpio_init(cs_pin);
  gpio_set_dir(cs_pin, GPIO_OUT);
  gpio_put(cs_pin, 1);
}

void MCP4822_setOutput(MCP4822* mcp, uint8_t channel, uint16_t value) {
  uint16_t outputData = (channel << 15) | (0b11 << 12) | (value & 0x0FFF);
  uint8_t data[2] = {(uint8_t)(outputData >> 8), (uint8_t)(outputData & 0xFF)};

  gpio_put(mcp->cs_pin, 0);
  spi_write_blocking(mcp->spi, data, 2);
  gpio_put(mcp->cs_pin, 1);
}