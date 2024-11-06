/**
 * @file MCP4822.h
 * @brief This file contains the MCP4822 interface for the MCP4822 12-bit DAC.
 */
#pragma once

#include "hardware/spi.h"

/**
 * @brief MCP4822 structure to hold SPI instance and chip select pin.
 */
typedef struct {
  spi_inst_t* spi;
  uint cs_pin;
} MCP4822;

/**
 * @brief Initialize the MCP4822 structure.
 *
 * @param mcp Pointer to MCP4822 structure
 * @param spi SPI instance
 * @param cs_pin Chip select pin
 */
void MCP4822_init(MCP4822* mcp, spi_inst_t* spi, uint cs_pin);

/**
 * @brief Set the output voltage of a specific channel.
 *
 * @param mcp Pointer to MCP4822 structure
 * @param channel Channel number
 * @param value 12-bit value to set the output voltage
 */
void MCP4822_setOutput(MCP4822* mcp, uint8_t channel, uint16_t value);