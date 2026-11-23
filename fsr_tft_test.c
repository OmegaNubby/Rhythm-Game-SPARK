#include <stdio.h>
#include "pico/stdlib.h"

#include "pins.h"
#include "tile_input.h"
#include "game_state.h"
#include "include/render.h"
#include "audio.h"

// Test-only replacement for audio_time_ms().
// The actual game will use the real function from audio.c.
uint32_t audio_time_ms(void)
{
    return to_ms_since_boot(get_absolute_time());
}

int main(void)
{
    stdio_init_all();
    sleep_ms(1500);

    printf("FSR + TFT test starting\n");

    tile_input_init();
    render_init();

    game_state_t test_state = {0};
    test_state.phase = GAME_PLAYING;

    note_view_t notes[NUM_LANES];

    uint32_t last_render = 0;

    while (true) {
        // Poll sensors frequently.
        tile_input_poll();

        // Print press/release events for debugging.
        tile_event_t event;

        while (tile_input_get_event(&event)) {
            printf("Lane %u: %s, pressure=%u, time=%lu ms\n",
                   event.lane,
                   event.type == TILE_PRESS ? "PRESS" : "RELEASE",
                   event.pressure,
                   (unsigned long)event.time_ms);
        }

        // Refresh the TFT around every 33 ms.
        uint32_t now = to_ms_since_boot(get_absolute_time());

        if (now - last_render >= 33) {
            int count = 0;

            for (uint8_t lane = 0; lane < NUM_LANES; lane++) {
                if (tile_input_is_down(lane)) {
                    notes[count++] = (note_view_t) {
                        .lane = lane,
                        .is_hold = true,
                        .holding = true,
                        .y_head = 220,
                        .y_tail = 120
                    };
                }
            }

            render_frame(&test_state, notes, count);

            last_render = now;
        }

        sleep_ms(1);
    }
}
