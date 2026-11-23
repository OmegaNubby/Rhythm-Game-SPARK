// Standalone UI test for the Proton board. No SD card, audio, tiles or game engine needed.
// Lives at the project root (next to platformio.ini), built only by [env:ui_test].
// Fakes a game_state_t so every UI screen can be checked by hand.

#include "ui.h"
#include "game_state.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

typedef struct { judgement_t j; int16_t offset; } fake_event_t;

// 15 events = 3 cycles of these 5.
// Expected at the end: score 2400, hits 12, misses 3, max combo 4, average offset +2 ms.
static const fake_event_t EVENTS[5] = {
    {JUDGE_PERFECT, -12},
    {JUDGE_GOOD,    +55},
    {JUDGE_PERFECT,  +8},
    {JUDGE_MISS,      0},
    {JUDGE_GOOD,    -40},
};
#define TOTAL_EVENTS 15
#define EVENT_GAP_MS 600

static void apply_event(game_state_t *s, const fake_event_t *e) {
    if (e->j == JUDGE_MISS) {
        s->combo = 0;
        s->misses++;
    } else {
        s->score += (e->j == JUDGE_PERFECT) ? 300 : 100;
        s->combo++;
        if (s->combo > s->max_combo) s->max_combo = s->combo;
        s->hits++;
        s->last_offset_ms = e->offset;
    }
    s->last_judgement = e->j;
    s->judgement_seq++;
}

int main(void) {
    stdio_init_all();
    sleep_ms(3000);                       // time to open the serial monitor
    printf("\nUI test starting\n");

    ui_init();
    game_state_t fake;

    while (true) {
        // ---- Stage 1: real menu, real keypad ----
        printf("TEST 1: menu. Press 1/2/3, then D to confirm\n");
        int song = ui_menu_select_song(3);
        uint8_t diff = ui_menu_select_difficulty();
        printf("  chosen song index=%d difficulty=%u\n", song, (unsigned)diff);

        // ---- Stage 2: fake game in progress (about 10 s) ----
        printf("TEST 2: fake play\n");
        memset(&fake, 0, sizeof fake);
        fake.phase = GAME_PLAYING;

        uint32_t t0 = to_ms_since_boot(get_absolute_time());
        int next_event = 0;
        while (true) {
            uint32_t t = to_ms_since_boot(get_absolute_time()) - t0;
            if (next_event < TOTAL_EVENTS && t >= (uint32_t)(next_event + 1) * EVENT_GAP_MS) {
                apply_event(&fake, &EVENTS[next_event % 5]);
                next_event++;
            }
            ui_update(&fake);
            if (next_event >= TOTAL_EVENTS && t > (TOTAL_EVENTS + 1) * EVENT_GAP_MS) break;
            sleep_ms(1);
        }

        // ---- Stage 3: results (8 s, rotating pages) ----
        printf("TEST 3: results\n");
        fake.phase = GAME_RESULTS;
        t0 = to_ms_since_boot(get_absolute_time());
        while (to_ms_since_boot(get_absolute_time()) - t0 < 8000) {
            ui_update(&fake);
            sleep_ms(1);
        }

        // Back to the menu and repeat
        fake.phase = GAME_MENU;
        ui_update(&fake);
    }
}