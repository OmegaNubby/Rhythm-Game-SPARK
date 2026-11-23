#include "seg7.h"
#include "pins.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"

// Segment encoding from the previous ECE lab.
// Assumes D1=A, D2=B, ..., D7=G, D8=DP.
static const uint8_t DIGIT_TABLE[10] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F  // 9
};

// One digit is refreshed every 1 ms.
// All eight digits are refreshed every 8 ms (125 Hz).
#define SEG7_REFRESH_US 1000

static volatile uint32_t current_score = 0;
static uint8_t current_digit = 0;
static repeating_timer_t seg7_timer;

static const uint32_t DIVISORS[8] = {
    10000000, 1000000, 100000, 10000,
    1000, 100, 10, 1
};

// Send one 16-bit message, MSB first.
// Bits 15:11 = 00000
// Bits 10:8  = display position
// Bits 7:0   = segment pattern

static void display_bitbang(uint16_t value) {
    gpio_put(PIN_SEG_LATCH, 0);
    busy_wait_us(10);

    for (int j = 15; j >= 0; j--) {
        bool data_bit = (value >> j) & 0x01;

        gpio_put(PIN_SEG_DATA, data_bit);
        busy_wait_us(1);

        gpio_put(PIN_SEG_CLK, 1);
        busy_wait_us(5);

        gpio_put(PIN_SEG_CLK, 0);
        busy_wait_us(5);
    }

    gpio_put(PIN_SEG_LATCH, 1);
    busy_wait_us(10);
}

// Refresh one display position.
static bool seg7_refresh_callback(repeating_timer_t *timer) {
    (void)timer;

    uint32_t score = current_score % 100000000;

    // Extract the digit for the current display position.
    uint8_t value =
        (score / DIVISORS[current_digit]) % 10;

    // Same message format as the previous lab:
    // (position << 8) | segment pattern
    uint16_t msg =
        ((uint16_t)current_digit << 8) |
        DIGIT_TABLE[value];

    display_bitbang(msg);

    current_digit++;

    if (current_digit >= 8) {
        current_digit = 0;
    }

    return true;
}

void seg7_init(void) {
    gpio_init(PIN_SEG_DATA);
    gpio_init(PIN_SEG_CLK);
    gpio_init(PIN_SEG_LATCH);

    gpio_set_dir(PIN_SEG_DATA, GPIO_OUT);
    gpio_set_dir(PIN_SEG_CLK, GPIO_OUT);
    gpio_set_dir(PIN_SEG_LATCH, GPIO_OUT);

    gpio_put(PIN_SEG_DATA, 0);
    gpio_put(PIN_SEG_CLK, 0);
    gpio_put(PIN_SEG_LATCH, 1);

    current_score = 0;
    current_digit = 0;

    // Start automatic display refreshing.
    add_repeating_timer_us(
        -SEG7_REFRESH_US,
        seg7_refresh_callback,
        NULL,
        &seg7_timer
    );
}

void seg7_update(uint32_t score) {
    current_score = score;
}
