#include "tft.h"
#include "pins.h"

#include "pico/stdlib.h"
#include "hardware/spi.h"

#define TFT_SPI_BAUD 10000000

#define CMD_SWRESET  0x01
#define CMD_SLPOUT   0x11
#define CMD_NORON    0x13
#define CMD_DISPON   0x29
#define CMD_CASET    0x2A
#define CMD_PASET    0x2B
#define CMD_RAMWR    0x2C
#define CMD_MADCTL   0x36
#define CMD_PIXFMT   0x3A

static uint16_t display_width = TFT_NATIVE_WIDTH;
static uint16_t display_height = TFT_NATIVE_HEIGHT;

static void select_display(bool selected) {
    gpio_put(PIN_TFT_CS, !selected);
}

static void write_command(uint8_t command) {
    select_display(true);
    gpio_put(PIN_TFT_DC, 0);
    spi_write_blocking(SPI_TFT, &command, 1);
    select_display(false);
}

static void write_data(const uint8_t *data, size_t length) {
    select_display(true);
    gpio_put(PIN_TFT_DC, 1);
    spi_write_blocking(SPI_TFT, data, length);
    select_display(false);
}

static void write_command_data(uint8_t command,
                               const uint8_t *data,
                               size_t length) {
    select_display(true);

    gpio_put(PIN_TFT_DC, 0);
    spi_write_blocking(SPI_TFT, &command, 1);

    if (length > 0) {
        gpio_put(PIN_TFT_DC, 1);
        spi_write_blocking(SPI_TFT, data, length);
    }

    select_display(false);
}

static void set_address_window(uint16_t x0, uint16_t y0,
                               uint16_t x1, uint16_t y1) {
    uint8_t data[4];

    data[0] = x0 >> 8;
    data[1] = x0 & 0xFF;
    data[2] = x1 >> 8;
    data[3] = x1 & 0xFF;
    write_command_data(CMD_CASET, data, 4);

    data[0] = y0 >> 8;
    data[1] = y0 & 0xFF;
    data[2] = y1 >> 8;
    data[3] = y1 & 0xFF;
    write_command_data(CMD_PASET, data, 4);

    write_command(CMD_RAMWR);
}

void tft_set_rotation(uint8_t rotation) {
    uint8_t madctl;

    switch (rotation & 3) {
        case 0:
            madctl = 0x48;
            display_width = 240;
            display_height = 320;
            break;

        case 1:
            madctl = 0x28;
            display_width = 320;
            display_height = 240;
            break;

        case 2:
            madctl = 0x88;
            display_width = 240;
            display_height = 320;
            break;

        default:
            madctl = 0xE8;
            display_width = 320;
            display_height = 240;
            break;
    }

    write_command_data(CMD_MADCTL, &madctl, 1);
}

uint16_t tft_width(void) {
    return display_width;
}

uint16_t tft_height(void) {
    return display_height;
}

void tft_init(void) {
    // Configure SPI pins.
    spi_init(SPI_TFT, TFT_SPI_BAUD);

    gpio_set_function(PIN_TFT_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_TFT_MOSI, GPIO_FUNC_SPI);

    spi_set_format(SPI_TFT, 8, SPI_CPOL_0,
                   SPI_CPHA_0, SPI_MSB_FIRST);

    // Configure control pins.
    gpio_init(PIN_TFT_CS);
    gpio_set_dir(PIN_TFT_CS, GPIO_OUT);
    gpio_put(PIN_TFT_CS, 1);

    gpio_init(PIN_TFT_DC);
    gpio_set_dir(PIN_TFT_DC, GPIO_OUT);
    gpio_put(PIN_TFT_DC, 1);

    gpio_init(PIN_TFT_RST);
    gpio_set_dir(PIN_TFT_RST, GPIO_OUT);

    // Hardware reset.
    gpio_put(PIN_TFT_RST, 1);
    sleep_ms(10);
    gpio_put(PIN_TFT_RST, 0);
    sleep_ms(20);
    gpio_put(PIN_TFT_RST, 1);
    sleep_ms(120);

    // Software reset.
    write_command(CMD_SWRESET);
    sleep_ms(150);

    // Exit sleep mode.
    write_command(CMD_SLPOUT);
    sleep_ms(120);

    // Pixel format: 16-bit RGB565.
    uint8_t pixel_format = 0x55;
    write_command_data(CMD_PIXFMT, &pixel_format, 1);

    // Frame rate control.
    uint8_t frame_control[] = {0x00, 0x18};
    write_command_data(0xB1, frame_control, 2);

    // Display function control.
    uint8_t display_function[] = {0x08, 0x82, 0x27};
    write_command_data(0xB6, display_function, 3);

    // Power control.
    uint8_t power_control_1 = 0x23;
    write_command_data(0xC0, &power_control_1, 1);

    uint8_t power_control_2 = 0x10;
    write_command_data(0xC1, &power_control_2, 1);

    // VCOM control.
    uint8_t vcom_control[] = {0x3E, 0x28};
    write_command_data(0xC5, vcom_control, 2);

    uint8_t vcom_control_2 = 0x86;
    write_command_data(0xC7, &vcom_control_2, 1);

    // Positive gamma correction.
    uint8_t gamma_positive[] = {
        0x0F, 0x31, 0x2B, 0x0C, 0x0E,
        0x08, 0x4E, 0xF1, 0x37, 0x07,
        0x10, 0x03, 0x0E, 0x09, 0x00
    };
    write_command_data(0xE0, gamma_positive, 15);

    // Negative gamma correction.
    uint8_t gamma_negative[] = {
        0x00, 0x0E, 0x14, 0x03, 0x11,
        0x07, 0x31, 0xC1, 0x48, 0x08,
        0x0F, 0x0C, 0x31, 0x36, 0x0F
    };
    write_command_data(0xE1, gamma_negative, 15);

    // Normal display mode and display on.
    write_command(CMD_NORON);
    sleep_ms(10);

    tft_set_rotation(0);

    write_command(CMD_DISPON);
    sleep_ms(100);
}

void tft_fill_rect(uint16_t x, uint16_t y,
                   uint16_t width, uint16_t height,
                   uint16_t color) {
    if (width == 0 || height == 0 ||
        x >= display_width || y >= display_height) {
        return;
    }

    // Clip rectangle to screen boundaries.
    if (width > display_width - x)
        width = display_width - x;

    if (height > display_height - y)
        height = display_height - y;

    set_address_window(x, y, x + width - 1, y + height - 1);

    uint8_t buffer[256 * 2];

    for (int i = 0; i < 256; i++) {
        buffer[i * 2] = color >> 8;
        buffer[i * 2 + 1] = color & 0xFF;
    }

    uint32_t remaining = (uint32_t)width * height;

    select_display(true);
    gpio_put(PIN_TFT_DC, 1);

    while (remaining > 0) {
        uint32_t pixels = remaining > 256 ? 256 : remaining;
        spi_write_blocking(SPI_TFT, buffer, pixels * 2);
        remaining -= pixels;
    }

    select_display(false);
}

void tft_fill_screen(uint16_t color) {
    tft_fill_rect(0, 0, display_width, display_height, color);
}

void tft_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= display_width || y >= display_height)
        return;

    set_address_window(x, y, x, y);

    uint8_t data[] = {
        color >> 8,
        color & 0xFF
    };

    write_data(data, 2);
}
