// PWM audio streamed from SD; the sample counter is the game clock.
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "audio.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

// FatFs timestamp hook. We only read the SD, so a fixed date is fine.
DWORD get_fattime(void) {
    return ((DWORD)(2026 - 1980) << 25)
         | ((DWORD)11 << 21)
         | ((DWORD)18 << 16);
}

#define AUDIO_PIN    47
#define RING_SIZE    8192
#define RING_MASK    (RING_SIZE - 1)
#define READ_SIZE    512

static uint8_t ring[RING_SIZE];
static volatile uint32_t rd = 0, wr = 0;
static volatile uint32_t sample_counter = 0;
static volatile bool playing = false;

static uint32_t sample_rate = 22050;
static uint16_t channels;
static uint16_t frame_size;
static uint32_t data_remaining;

static uint slice;
static uint sample_alarm;
static uint32_t sample_period_us;
static uint32_t sample_period_remainder;
static uint32_t sample_period_accumulator;
static absolute_time_t next_sample_time;
static FIL wav;
static bool file_open = false;
static bool file_done = false;

static uint16_t read_u16le(const uint8_t *bytes) {
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static uint32_t read_u32le(const uint8_t *bytes) {
    return (uint32_t)bytes[0]
         | ((uint32_t)bytes[1] << 8)
         | ((uint32_t)bytes[2] << 16)
         | ((uint32_t)bytes[3] << 24);
}

static bool read_exact(void *buffer, UINT size) {
    UINT got = 0;
    return f_read(&wav, buffer, size, &got) == FR_OK && got == size;
}

static bool supported_sample_rate(uint32_t rate) {
    return rate == 16000 || rate == 44100 || rate == 48000;
}

static bool read_wav_header(void) {
    uint8_t header[12];
    if (!read_exact(header, sizeof header) ||
        memcmp(header, "RIFF", 4) != 0 ||
        memcmp(header + 8, "WAVE", 4) != 0) {
        return false;
    }

    FSIZE_t riff_end = (FSIZE_t)read_u32le(header + 4) + 8;
    if (riff_end < sizeof header || riff_end > f_size(&wav)) {
        return false;
    }

    bool found_fmt = false;
    while (riff_end - f_tell(&wav) >= 8) {
        uint8_t chunk[8];
        if (!read_exact(chunk, sizeof chunk)) {
            return false;
        }

        uint32_t chunk_size = read_u32le(chunk + 4);
        FSIZE_t chunk_start = f_tell(&wav);
        FSIZE_t chunk_end = chunk_start + (FSIZE_t)chunk_size + (chunk_size & 1u);
        if (chunk_end < chunk_start || chunk_end > riff_end) {
            return false;
        }

        if (memcmp(chunk, "fmt ", 4) == 0) {
            uint8_t fmt[16];
            if (chunk_size < sizeof fmt || !read_exact(fmt, sizeof fmt)) {
                return false;
            }

            uint16_t format = read_u16le(fmt);
            channels = read_u16le(fmt + 2);
            sample_rate = read_u32le(fmt + 4);
            uint16_t block_align = read_u16le(fmt + 12);
            uint16_t bits_per_sample = read_u16le(fmt + 14);

            if (format != 1 || bits_per_sample != 16 ||
                (channels != 1 && channels != 2) ||
                block_align != channels * 2 ||
                !supported_sample_rate(sample_rate)) {
                return false;
            }

            frame_size = block_align;
            found_fmt = true;
            if (f_lseek(&wav, chunk_end) != FR_OK) {
                return false;
            }
        } else if (memcmp(chunk, "data", 4) == 0) {
            if (!found_fmt || chunk_size % frame_size != 0) {
                return false;
            }
            data_remaining = chunk_size;
            return true;
        } else if (f_lseek(&wav, chunk_end) != FR_OK) {
            return false;
        }
    }

    return false;
}

static void close_wav(void) {
    if (file_open) {
        FRESULT result = f_close(&wav);
        file_open = false;
        if (result != FR_OK) {
            printf("Audio: WAV close failed (FRESULT=%d)\n", (int)result);
        }
    }
}

static void schedule_next_sample(uint alarm_num) {
    uint32_t interval = sample_period_us;
    sample_period_accumulator += sample_period_remainder;
    if (sample_period_accumulator >= sample_rate) {
        sample_period_accumulator -= sample_rate;
        interval++;
    }

    while (true) {
        next_sample_time = delayed_by_us(next_sample_time, interval);
        if (!hardware_alarm_set_target(alarm_num, next_sample_time)) {
            return;
        }
        next_sample_time = get_absolute_time();
    }
}

// Fires once per sample, independently of the faster PWM carrier.
static void sample_alarm_callback(uint alarm_num) {
    if (playing && rd != wr) {
        pwm_set_gpio_level(AUDIO_PIN, ring[rd & RING_MASK]);
        rd++;
        sample_counter++;
    } else {
        pwm_set_gpio_level(AUDIO_PIN, 128);
        if (playing) {
            sample_counter++;
        }
    }

    if (playing) {
        schedule_next_sample(alarm_num);
    }
}

void audio_init(void) {
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(AUDIO_PIN);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, 255);
    pwm_config_set_clkdiv(&cfg, 2.0f);
    pwm_init(slice, &cfg, true);

    sample_alarm = hardware_alarm_claim_unused(true);
    hardware_alarm_set_callback(sample_alarm, sample_alarm_callback);
}

bool audio_load(const char *path) {
    playing = false;
    hardware_alarm_cancel(sample_alarm);
    close_wav();
    rd = wr = 0;
    file_done = false;
    sample_counter = 0;
    data_remaining = 0;

    FRESULT result = f_open(&wav, path, FA_READ);
    if (result != FR_OK) {
        printf("Audio: failed to open WAV (FRESULT=%d)\n", (int)result);
        return false;
    }
    file_open = true;

    if (!read_wav_header()) {
        printf("Audio: invalid or unsupported WAV file: %s\n", path);
        close_wav();
        return false;
    }

    audio_refill();
    return true;
}

void audio_start(void) {
    hardware_alarm_cancel(sample_alarm);
    sample_period_us = 1000000u / sample_rate;
    sample_period_remainder = 1000000u % sample_rate;
    sample_period_accumulator = 0;
    next_sample_time = get_absolute_time();
    playing = true;
    schedule_next_sample(sample_alarm);
}

// Call this often from the main loop.
void audio_refill(void) {
    uint8_t tmp[READ_SIZE];

    while (!file_done && (RING_SIZE - (wr - rd)) >= 128 &&
           data_remaining >= frame_size) {
        UINT request = sizeof tmp;
        if (request > data_remaining) {
            request = data_remaining;
        }
        request -= request % frame_size;

        UINT got = 0;
        FRESULT result = f_read(&wav, tmp, request, &got);
        if (got > data_remaining) {
            got = data_remaining;
        }
        data_remaining -= got;

        UINT complete_bytes = got - (got % frame_size);
        for (UINT i = 0; i < complete_bytes; i += frame_size) {
            uint16_t left_bits = read_u16le(tmp + i);
            int32_t mono = (left_bits & 0x8000u)
                ? (int32_t)left_bits - 65536
                : left_bits;

            if (channels == 2) {
                uint16_t right_bits = read_u16le(tmp + i + 2);
                int32_t right = (right_bits & 0x8000u)
                    ? (int32_t)right_bits - 65536
                    : right_bits;
                mono = (mono + right) / 2;
            }

            ring[wr & RING_MASK] = (uint8_t)((mono + 32768) >> 8);
            wr++;
        }

        if (result != FR_OK || got != request || complete_bytes != got) {
            if (result != FR_OK) {
                printf("Audio: WAV read failed (FRESULT=%d)\n", (int)result);
            } else {
                printf("Audio: unexpected end of WAV data\n");
            }
            file_done = true;
            break;
        }
    }

    if (data_remaining == 0) {
        file_done = true;
    }
}

// What the game uses for note timing.
uint32_t audio_time_ms(void) {
    return (uint64_t)sample_counter * 1000 / sample_rate;
}

bool audio_is_finished(void) {
    return file_done && rd == wr;
}

void audio_stop(void) {
    playing = false;
    hardware_alarm_cancel(sample_alarm);
    pwm_set_gpio_level(AUDIO_PIN, 128);
    close_wav();
}
