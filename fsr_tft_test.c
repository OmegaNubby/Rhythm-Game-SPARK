#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "include/tft.h"

// Your four FSR connections
#define FSR_COUNT 4

static const uint FSR_GPIO[FSR_COUNT] = {
    40, 41, 42, 43
};

static const uint16_t LANE_COLORS[FSR_COUNT] = {
    TFT_CYAN, TFT_MAGENTA, TFT_YELLOW, 0xA01F
};

#define BAR_TOP     65
#define BAR_BOTTOM  260
#define BAR_HEIGHT  (BAR_BOTTOM - BAR_TOP)
#define BAR_WIDTH   44
#define BAR_GAP     12

int main(void)
{
    stdio_init_all();
    sleep_ms(1500);

    adc_init();

    for (int i = 0; i < FSR_COUNT; i++) {
        adc_gpio_init(FSR_GPIO[i]);
    }

    tft_init();
    tft_set_rotation(0);
    tft_fill_screen(TFT_BLACK);

    // Draw four lane backgrounds
    for (int i = 0; i < FSR_COUNT; i++) {
        int x = 8 + i * 60;
        tft_fill_rect(x, BAR_TOP, BAR_WIDTH, BAR_HEIGHT,
                      0x2104);
    }

    while (true) {
        for (int i = 0; i < FSR_COUNT; i++) {
            // GP40 = ADC channel 0, GP41 = channel 1, etc.
            adc_select_input(FSR_GPIO[i] - 40);
            sleep_us(10);

            uint16_t raw = adc_read();

            // Map ADC value to a bar height.
            int height = (raw * BAR_HEIGHT) / 4095;

            if (height > BAR_HEIGHT)
                height = BAR_HEIGHT;

            int x = 8 + i * 60;

            // Clear the old bar.
            tft_fill_rect(x, BAR_TOP, BAR_WIDTH, BAR_HEIGHT,
                          0x2104);

            // Draw the current reading.
            if (height > 0) {
                tft_fill_rect(
                    x,
                    BAR_BOTTOM - height,
                    BAR_WIDTH,
                    height,
                    LANE_COLORS[i]
                );
            }

            printf("FSR %d: %u\n", i + 1, raw);
        }

        printf("--------------------\n");
        sleep_ms(100);
    }
}
