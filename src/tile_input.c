#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "pins.h"
#include "tile_input.h"
#include "audio.h"

#define EVENT_QUEUE_SIZE 16

// Default thresholds. We can tune these after seeing your actual ADC readings.
#define DEFAULT_PRESS_THRESHOLD   1000
#define DEFAULT_RELEASE_THRESHOLD 700

static uint16_t press_threshold[NUM_LANES];
static uint16_t release_threshold[NUM_LANES];

static bool lane_down[NUM_LANES];

static tile_event_t event_queue[EVENT_QUEUE_SIZE];
static volatile uint8_t queue_head = 0;
static volatile uint8_t queue_tail = 0;

static void queue_event(uint8_t lane, tile_event_type_t type,
                        uint16_t pressure)
{
    uint8_t next = (queue_head + 1) % EVENT_QUEUE_SIZE;

    // Queue full: drop the newest event.
    if (next == queue_tail) {
        return;
    }

    event_queue[queue_head].lane = lane;
    event_queue[queue_head].type = type;
    event_queue[queue_head].pressure = pressure;
    event_queue[queue_head].time_ms = audio_time_ms();

    queue_head = next;
}

void tile_input_init(void)
{
    adc_init();

    for (uint8_t lane = 0; lane < NUM_LANES; lane++) {
        adc_gpio_init(PIN_FSR_BASE + lane);

        press_threshold[lane] = DEFAULT_PRESS_THRESHOLD;
        release_threshold[lane] = DEFAULT_RELEASE_THRESHOLD;

        lane_down[lane] = false;
    }

    queue_head = 0;
    queue_tail = 0;
}

void tile_input_poll(void)
{
    for (uint8_t lane = 0; lane < NUM_LANES; lane++) {
        adc_select_input(FSR_ADC_CHANNEL(lane));

        uint16_t pressure = adc_read();

        if (!lane_down[lane] &&
            pressure >= press_threshold[lane]) {

            lane_down[lane] = true;
            queue_event(lane, TILE_PRESS, pressure);

        } else if (lane_down[lane] &&
                   pressure <= release_threshold[lane]) {

            lane_down[lane] = false;
            queue_event(lane, TILE_RELEASE, pressure);
        }
    }
}

bool tile_input_get_event(tile_event_t *out)
{
    if (out == NULL || queue_tail == queue_head) {
        return false;
    }

    *out = event_queue[queue_tail];
    queue_tail = (queue_tail + 1) % EVENT_QUEUE_SIZE;

    return true;
}

bool tile_input_is_down(uint8_t lane)
{
    if (lane >= NUM_LANES) {
        return false;
    }

    return lane_down[lane];
}

void tile_input_set_thresholds(uint8_t lane, uint16_t press, uint16_t release)
{
    if (lane >= NUM_LANES || release > press) {
        return;
    }
    press_threshold[lane] = press;
    release_threshold[lane] = release;
}
