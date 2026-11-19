#include "render.h"
#include "pins.h"
#include "pico/stdlib.h"
#include <stdio.h>

#define RENDER_ROWS 10
#define RENDER_REFRESH_MS 100

static uint32_t last_render_ms;
static uint32_t last_phase;
static uint16_t last_judgement_seq;

static int clamp_row(int16_t y) {
    int row;
    if (y <= 0) row = RENDER_ROWS - 1;
    else if (y >= 1000) row = 0;
    else row = RENDER_ROWS - 1 - ((y * RENDER_ROWS) / 1000);
    return row;
}

void render_init(void) {
    last_render_ms = 0;
    last_phase = 0;
    last_judgement_seq = 0;
    printf("Render: serial console renderer ready\n");
}

void render_frame(const game_state_t *st,
                  const note_view_t *notes,
                  int count) {
    if (st == NULL) return;

    uint32_t now = st->song_time_ms;
    if (st->phase == last_phase &&
        now - last_render_ms < RENDER_REFRESH_MS &&
        st->judgement_seq == last_judgement_seq) {
        return;
    }

    last_phase = (uint32_t)st->phase;
    last_render_ms = now;
    last_judgement_seq = st->judgement_seq;

    char grid[RENDER_ROWS][NUM_LANES];
    for (int row = 0; row < RENDER_ROWS; row++) {
        for (int lane = 0; lane < NUM_LANES; lane++) {
            grid[row][lane] = '.';
        }
    }

    if (notes != NULL) {
        for (int i = 0; i < count; i++) {
            if (notes[i].lane >= NUM_LANES) continue;

            int head_row = clamp_row(notes[i].y_head);
            int tail_row = clamp_row(notes[i].y_tail);
            if (tail_row < head_row) {
                int temp = head_row;
                head_row = tail_row;
                tail_row = temp;
            }

            grid[head_row][notes[i].lane] =
                notes[i].holding ? '=' : (notes[i].is_hold ? 'H' : 'O');

            if (notes[i].is_hold) {
                for (int row = head_row + 1; row <= tail_row &&
                     row < RENDER_ROWS; row++) {
                    grid[row][notes[i].lane] = '|';
                }
            }
        }
    }

    printf("\nT=%lu ms Score=%lu Combo=%u Notes=%d\n",
           (unsigned long)now,
           (unsigned long)st->score,
           (unsigned)st->combo,
           count);

    for (int row = RENDER_ROWS - 1; row >= 0; row--) {
        printf("|");
        for (int lane = 0; lane < NUM_LANES; lane++) {
            printf(" %c ", grid[row][lane]);
        }
        printf("|\n");
        if (row == RENDER_ROWS - 1) {
            printf("+--------------------+\n");
        }
    }

    printf("  L0  L1  L2  L3\n");

    if (st->judgement_seq != 0) {
        const char *judgement = " ";
        switch (st->last_judgement) {
            case JUDGE_PERFECT: judgement = "PERFECT"; break;
            case JUDGE_GOOD: judgement = "GOOD"; break;
            case JUDGE_MISS: judgement = "MISS"; break;
            default: judgement = ""; break;
        }
        printf("Judgement: %s lane=%u offset=%dms\n",
               judgement, (unsigned)st->last_lane,
               (int)st->last_offset_ms);
    }
}
