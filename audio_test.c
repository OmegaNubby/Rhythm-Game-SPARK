#include <stdio.h>
#include "pico/stdlib.h"
#include "ff.h"
#include "hw_config.h"
#include "audio.h"


#define AUDIO_PIN 44
#define TONE_ONLY 0     // 1 = test wiring/speaker only, 0 = play song1.wav from SD

DWORD get_fattime(void) { return ((DWORD)(2026 - 1980) << 25) | ((DWORD)1 << 21) | ((DWORD)1 << 16); }
// delete the line above if it's already defined in your audio.c or elsewhere

int main(void) {
    stdio_init_all();
    sleep_ms(3000);
    printf("\naudio test\n");

#if TONE_ONLY
    printf("tone only test\n");
    gpio_init(AUDIO_PIN);
    gpio_set_dir(AUDIO_PIN, GPIO_OUT);
    while (true) {                       // ~440 Hz square wave
        gpio_put(AUDIO_PIN, 1); sleep_us(1136);
        gpio_put(AUDIO_PIN, 0); sleep_us(1136);
    }
#else
    sd_card_t *sd = sd_get_by_num(0);
    FRESULT fr = f_mount(&sd->fatfs, sd->pcName, 1);
    printf("mount: %d\n", fr);
    if (fr != FR_OK) while (1) tight_loop_contents();
    f_chdrive(sd->pcName);

    audio_init();
    if (!audio_load("song1.wav")) { printf("open failed\n"); while (1) tight_loop_contents(); }
    audio_start();
    printf("playing\n");

    uint32_t last = 0;
    while (!audio_is_finished()) {
        audio_refill();
        uint32_t t = audio_time_ms();
        if (t - last >= 1000) { printf("t = %lu ms\n", (unsigned long)t); last = t; }
    }
    audio_stop();
    printf("done\n");
    while (true) tight_loop_contents();
#endif
}