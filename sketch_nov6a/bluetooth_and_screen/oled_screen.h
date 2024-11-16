#ifndef OLED_SCREEN_H
#define OLED_SCREEN_H
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>

#define RGB565(r, g, b) (((r << 11)) | (g << 5) | b)
#define SSD1351_BLACK RGB565(0, 0, 0)
#define SSD1351_WHITE RGB565(31, 63, 31)
#define SSD1351_RED RGB565(31, 0, 0)
#define SSD1351_GREEN RGB565(0, 63, 0)
#define SSD1351_BLUE RGB565(0, 0, 31)

void oled_setup(uint16_t width, uint16_t height, int8_t cs_pin, int8_t dc_pin,
                int8_t mosi_pin, int8_t sclk_pin, int8_t rst_pin);
Adafruit_SSD1351* oled(void);
void oled_clear(void);
void oled_debug(void);
bool oled_debug_timer(uint64_t* timer, uint64_t intervall);

#endif
