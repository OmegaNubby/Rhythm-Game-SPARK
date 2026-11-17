// main.c - Person 1: init + top-level loop. Integration only; no feature logic.

#include <stdio.h>
#include "pico/stdlib.h"
#include "pins.h"
#include "game_state.h"
#include "tile_input.h"
#include "chart.h"
#include "audio.h"
#include "render.h"

#define UI_PERIOD_US 100000   // refresh LCD/7-seg at 10 Hz (I2C is slow)

int main(void) {
    stdio_init_all();
    sleep_ms(1500);                       // let USB serial come up
    printf("Rhythm game boot\n");

    tile_input_init();
    audio_init();
    render_init();
    ui_init();
    game_init();

    if (!chart_init()) {
        printf("SD init failed or no songs\n");
    }

    uint32_t last_ui = 0;

    while (true) {
        const game_state_t *s = game_get_state();

        switch (s->phase) {
        case GAME_MENU: {
            int count = chart_count();
            if (count <= 0) {
                printf("No songs on SD card\n");
                sleep_ms(2000);
                chart_init();             // retry (card may have been inserted)
                break;
            }
            int song = ui_menu_select_song(count);
            uint8_t diff = ui_menu_select_difficulty();
            game_start(song, diff);
            if (game_get_state()->phase != GAME_PLAYING) {
                printf("Could not start song %d (check SD card / WAV file)\n", song);
                sleep_ms(2000);
            }
            break;
        }

        case GAME_PLAYING: {
            audio_refill();               // keep the ring buffer full
            tile_input_poll();            // no-op if P2 polls on a timer
            game_update();

            note_view_t views[MAX_NOTE_VIEWS];
            int n = game_get_note_views(views, MAX_NOTE_VIEWS);
            render_frame(s, views, n);

            uint32_t t = time_us_32();
            if (t - last_ui >= UI_PERIOD_US) {
                ui_update(s);
                last_ui = t;
            }
            break;
        }

        case GAME_RESULTS:
            ui_update(s);
            printf("Done. score=%lu max_combo=%u hits=%u misses=%u\n",
                   (unsigned long)s->score, s->max_combo, s->hits, s->misses);
            sleep_ms(5000);               // TODO: wait for keypress instead
            game_stop();                  // back to GAME_MENU
            break;

        case GAME_PAUSED:
        default:
            audio_refill();
            break;
        }
    }
}