#include <stdio.h>
#include "pico/stdlib.h"
#include "ff.h"
#include "hw_config.h"
#include "audio.h"
#include "diskio.h"


#define AUDIO_PIN 47
#define TONE_ONLY 0     // 1 = test wiring/speaker only, 0 = play song1.wav from SD


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
    printf("SD card test\n");
    uint8_t sector[512];

    DRESULT result = disk_read(0, sector, 0, 1);

    printf("Read result: %d\n", result);

    printf("First 16 bytes: ");
    for (int i = 0; i < 16; i++) {
        printf("%02X ", sector[i]);
    }
    printf("\n");

    printf("Boot signature: %02X %02X\n",
       sector[510], sector[511]);
    sd_card_t *sd = sd_get_by_num(0);
    FRESULT fr = f_mount(&sd->fatfs, sd->pcName, 1);
    
    printf("mount: %d\n", fr);
    if (fr != FR_OK) while (1) tight_loop_contents();
    f_chdrive(sd->pcName);

    audio_init();
    if (!audio_load("twinkle.wav")) { printf("open failed\n"); while (1) tight_loop_contents(); }
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