
#include "include/tft.h"

#include "pico/stdlib.h"
#include <stdio.h>

int main(void) {
    stdio_init_all();
    sleep_ms(3000);

    printf("Starting TFT test...\n");

    tft_init();

    while (true) {
        printf("RED\n");
        tft_fill_screen(TFT_RED);
        sleep_ms(1500);

        printf("GREEN\n");
        tft_fill_screen(TFT_GREEN);
        sleep_ms(1500);

        printf("BLUE\n");
        tft_fill_screen(TFT_BLUE);
        sleep_ms(1500);

        printf("WHITE\n");
        tft_fill_screen(TFT_WHITE);
        sleep_ms(1500);

        printf("BLACK\n");
        tft_fill_screen(TFT_BLACK);
        sleep_ms(1500);
    }
}
