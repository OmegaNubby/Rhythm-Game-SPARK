
#ifndef TFT_H
#define TFT_H

#include <stdint.h>

#define TFT_NATIVE_WIDTH  240
#define TFT_NATIVE_HEIGHT 320

// RGB565 colors
#define TFT_BLACK   0x0000
#define TFT_WHITE   0xFFFF
#define TFT_RED     0xF800
#define TFT_GREEN   0x07E0
#define TFT_BLUE    0x001F
#define TFT_YELLOW  0xFFE0
#define TFT_CYAN    0x07FF
#define TFT_MAGENTA 0xF81F

void tft_init(void);
void tft_set_rotation(uint8_t rotation);

uint16_t tft_width(void);
uint16_t tft_height(void);

void tft_fill_screen(uint16_t color);
void tft_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void tft_fill_rect(uint16_t x, uint16_t y,
                   uint16_t width, uint16_t height,
                   uint16_t color);

#endif
