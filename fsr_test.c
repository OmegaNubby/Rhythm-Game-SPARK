// fsr_test.c - standalone FSR + audio feedback test (no game code needed).
// Build with:  pio run -e fsr_test -t upload
// Then open the serial monitor.
//
// What it does:
//   * reads the 4 FSRs (GP40..43 = ADC0..3) about 100 times per second
//   * calibrates a "no pressure" baseline at boot (DON'T touch the tiles for ~1 s)
//   * plays a different tone on GP44 while a tile is pressed (lane 0..3 = low..high)
//   * prints raw values + PRESSED/released over serial every 200 ms

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#define NUM_LANES        4
#define FSR_GPIO_BASE    40          // lane i -> GPIO 40+i -> ADC channel i
#define AUDIO_PIN        47

// How far the reading must move from the baseline to count as a press (12-bit, 0..4095).
#define PRESS_DELTA      300
// 1: reading goes UP when pressed (FSR on the top of the divider, resistor to GND).
// 0: reading goes DOWN when pressed (FSR to GND, resistor to 3.3 V).
#define PRESS_GOES_UP    1

static const uint32_t lane_freq[NUM_LANES] = { 262, 330, 392, 523 };  // C4 E4 G4 C5

static uint pwm_slice;

static void tone_on(uint32_t freq) {
    uint32_t sys = clock_get_hz(clk_sys);
    uint32_t wrap = 65535;
    float div = (float)sys / ((float)freq * (wrap + 1));
    if (div < 1.0f) { div = 1.0f; wrap = sys / freq - 1; }
    pwm_set_clkdiv(pwm_slice, div);
    pwm_set_wrap(pwm_slice, (uint16_t)wrap);
    pwm_set_gpio_level(AUDIO_PIN, (uint16_t)(wrap / 4));   // 25% duty: quieter
    pwm_set_enabled(pwm_slice, true);
}

static void tone_off(void) {
    pwm_set_gpio_level(AUDIO_PIN, 0);
}

static uint16_t read_lane(int lane) {
    adc_select_input(lane);
    return adc_read();
}

static bool is_pressed(uint16_t value, uint16_t baseline) {
#if PRESS_GOES_UP
    return value > baseline + PRESS_DELTA;
#else
    return value + PRESS_DELTA < baseline;
#endif
}

int main(void) {
    stdio_init_all();
    sleep_ms(3000);
    printf("\nFSR test: GP%d..GP%d, audio on GP%d\n",
           FSR_GPIO_BASE, FSR_GPIO_BASE + NUM_LANES - 1, AUDIO_PIN);

    adc_init();
    for (int i = 0; i < NUM_LANES; i++) adc_gpio_init(FSR_GPIO_BASE + i);

    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    pwm_slice = pwm_gpio_to_slice_num(AUDIO_PIN);
    pwm_set_gpio_level(AUDIO_PIN, 0);
    pwm_set_enabled(pwm_slice, true);

    // Calibrate: average 64 samples per lane with nothing pressed.
    printf("Calibrating - keep hands off the tiles...\n");
    sleep_ms(500);
    uint16_t baseline[NUM_LANES];
    for (int i = 0; i < NUM_LANES; i++) {
        uint32_t sum = 0;
        for (int n = 0; n < 64; n++) { sum += read_lane(i); sleep_ms(2); }
        baseline[i] = (uint16_t)(sum / 64);
        printf("  lane %d baseline = %u\n", i, baseline[i]);
    }

    // Ready beep so you know it's live.
    tone_on(880); sleep_ms(150); tone_off();
    printf("Ready. Press a tile. PRESS_DELTA=%d, press goes %s\n",
           PRESS_DELTA, PRESS_GOES_UP ? "UP" : "DOWN");

    uint32_t last_print = 0;
    int playing_lane = -1;

    while (true) {
        uint16_t val[NUM_LANES];
        int pressed_lane = -1;
        for (int i = 0; i < NUM_LANES; i++) {
            val[i] = read_lane(i);
            if (pressed_lane < 0 && is_pressed(val[i], baseline[i])) pressed_lane = i;
        }

        if (pressed_lane != playing_lane) {
            if (pressed_lane >= 0) tone_on(lane_freq[pressed_lane]);
            else                   tone_off();
            playing_lane = pressed_lane;
        }

        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_print >= 200) {
            last_print = now;
            printf("raw: %4u %4u %4u %4u  | %s",
                   val[0], val[1], val[2], val[3],
                   pressed_lane >= 0 ? "PRESSED lane " : "released");
            if (pressed_lane >= 0) printf("%d", pressed_lane);
            printf("\n");
        }
        sleep_ms(10);
    }
}