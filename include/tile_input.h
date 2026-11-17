#ifndef TILE_INPUT_H
#define TILE_INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include "pins.h"

typedef enum {
    TILE_PRESS,
    TILE_RELEASE
} tile_event_type_t;

typedef struct {
    uint8_t           lane;        // 0..NUM_LANES-1
    tile_event_type_t type;
    uint16_t          pressure;    // 0..4095 raw ADC at event time
    uint32_t          time_ms;     // audio_time_ms() when the event was detected (NOT time since boot)
} tile_event_t;

// Initialize ADC pins and thresholds.
void tile_input_init(void);

// Call every ~1 ms (timer or main loop). Samples FSRs and queues events.
void tile_input_poll(void);

// Pop the oldest event. Returns false if queue empty.
bool tile_input_get_event(tile_event_t *out);

// Current held state of a lane (true while pressed). Used for hold notes.
bool tile_input_is_down(uint8_t lane);

// Tuning: press/release thresholds (with hysteresis), raw ADC units.
void tile_input_set_thresholds(uint8_t lane, uint16_t press, uint16_t release);

#endif // TILE_INPUT_H