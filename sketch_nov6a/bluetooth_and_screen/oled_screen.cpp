#include "oled_screen.h"
#include "define.h"
#include <stdarg.h>

global bool g_initialized = false;
global Adafruit_SSD1351* g_oled = NULL;

void oled_setup(uint16_t width, uint16_t height, int8_t cs_pin, int8_t dc_pin,
                int8_t mosi_pin, int8_t sclk_pin, int8_t rst_pin)
{
    if (!g_initialized)
    {
        g_oled = new Adafruit_SSD1351(width, height, cs_pin, dc_pin, mosi_pin,
                                      sclk_pin, rst_pin);
        g_oled->begin();
        g_oled->fillScreen(SSD1351_BLACK);
        g_oled->setTextColor(SSD1351_WHITE);
        g_oled->setTextSize(1);
        g_initialized = true;
    }
}

Adafruit_SSD1351* oled(void)
{
    return g_oled;
}

void oled_clear(void)
{
    g_oled->fillScreen(SSD1351_BLACK);
}

void oled_debug(void)
{
    oled_clear();
    g_oled->setCursor(10, 10);
}

bool oled_debug_timer(uint64_t* timer, uint64_t intervall)
{
    if (millis() >= *timer)
    {
        oled_debug();
        *timer = millis() + intervall;
        return true;
    }
    return false;
}
