//PWM audio streamed from SD, sample counter = game clock
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "audio.h"
#include "ff.h"              // FatFs (e.g., carlk3/no-OS-FatFS-SD-SPI-RPi-Pico)

#define AUDIO_PIN    44
#define SAMPLE_RATE  22050
#define WAV_HEADER   44               // assumes canonical 44-byte header
#define RING_SIZE    8192             // power of 2
#define RING_MASK    (RING_SIZE - 1)

static uint8_t ring[RING_SIZE];
static volatile uint32_t rd = 0, wr = 0;          // free-running indices
static volatile uint32_t sample_counter = 0;      // master clock
static volatile bool playing = false;

static uint slice;
static FIL wav;

// Fires once per sample (PWM wrap)
static void pwm_isr(void) {
    pwm_clear_irq(slice);
    if (playing && rd != wr) {
        pwm_set_gpio_level(AUDIO_PIN, ring[rd & RING_MASK]);
        rd++;
        sample_counter++;
    } else {
        pwm_set_gpio_level(AUDIO_PIN, 128);        // silence (midpoint)
        if (playing) { /* underrun: count as time passing so game stays synced */
            sample_counter++;
        }
    }
}

void audio_init(void) {
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(AUDIO_PIN);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, 255);                               // 8-bit
    float div = (float)clock_get_hz(clk_sys) / (256.0f * SAMPLE_RATE);
    pwm_config_set_clkdiv(&cfg, div);                             // wrap rate = 22.05 kHz
    pwm_init(slice, &cfg, true);

    pwm_clear_irq(slice);
    pwm_set_irq_enabled(slice, true);
    irq_set_exclusive_handler(PWM_DEFAULT_IRQ_NUM(), pwm_isr);
    irq_set_enabled(PWM_DEFAULT_IRQ_NUM(), true);
}

bool audio_load(const char *path) {
    playing = false;
    rd = wr = 0;
    sample_counter = 0;
    if (f_open(&wav, path, FA_READ) != FR_OK) return false;
    f_lseek(&wav, WAV_HEADER);
    audio_refill();                 // prefill buffer before starting
    return true;
}

void audio_start(void) { playing = true; }

// Call this often from the main loop
void audio_refill(void) {
    uint8_t tmp[512];
    while ((RING_SIZE - (wr - rd)) >= sizeof(tmp)) {
        UINT got;
        if (f_read(&wav, tmp, sizeof(tmp), &got) != FR_OK || got == 0) break;
        for (UINT i = 0; i < got; i++) ring[(wr + i) & RING_MASK] = tmp[i];
        wr += got;
    }
}

// What the game uses for note timing
uint32_t audio_time_ms(void) {
    return (uint64_t)sample_counter * 1000 / SAMPLE_RATE;
}


/*
main stuff
f_mount(&fs, "", 1);
audio_init();
audio_load("song1.wav");
audio_start();

while (true) {
    audio_refill();                      // keep the ring buffer full
    uint32_t t = audio_time_ms();        // compare against chart note timestamps
    // game_update(t);  (Person 3's engine calls this)
}
*/





