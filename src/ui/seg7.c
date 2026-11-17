#include "seg7.h"
#include "pins.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

// 7-segment digit encoding lookup table (Common Cathode)
static const uint8_t DIGIT_TABLE[10] = {
    0b00111111, // 0
    0b00000110, // 1
    0b01011011, // 2
    0b01001111, // 3
    0b01100110, // 4
    0b01101101, // 5
    0b01111101, // 6
    0b00000111, // 7
    0b01111111, // 8
    0b01101111  // 9
};

void seg7_init(void) {
    gpio_init(PIN_SEG_DATA);
    gpio_init(PIN_SEG_CLK);
    gpio_init(PIN_SEG_LATCH);

    gpio_set_dir(PIN_SEG_DATA, GPIO_OUT);
    gpio_set_dir(PIN_SEG_CLK, GPIO_OUT);
    gpio_set_dir(PIN_SEG_LATCH, GPIO_OUT);

    gpio_put(PIN_SEG_DATA, 0);
    gpio_put(PIN_SEG_CLK, 0);
    gpio_put(PIN_SEG_LATCH, 0);
}

static void shift_out(uint8_t data) {
    for (int i = 7; i >= 0; i--) {
        gpio_put(PIN_SEG_DATA, (data >> i) & 0x01);
        gpio_put(PIN_SEG_CLK, 1);
        gpio_put(PIN_SEG_CLK, 0);
    }
}

void seg7_update(uint32_t score) {
    if (score > 9999) {
        score = 9999;
    }

    uint8_t thousands = (score / 1000) % 10;
    uint8_t hundreds  = (score / 100) % 10;
    uint8_t tens      = (score / 10) % 10;
    uint8_t ones      = score % 10;

    gpio_put(PIN_SEG_LATCH, 0);

    shift_out(DIGIT_TABLE[thousands]);
    shift_out(DIGIT_TABLE[hundreds]);
    shift_out(DIGIT_TABLE[tens]);
    shift_out(DIGIT_TABLE[ones]);

    gpio_put(PIN_SEG_LATCH, 1);
}