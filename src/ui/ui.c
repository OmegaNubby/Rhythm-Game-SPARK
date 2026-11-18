#include "ui.h"
#include "keypad.h"
#include "lcd.h"
#include "seg7.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

// 1 = show timing offset (needs `last_offset_ms` in game_state_t and game.c's handle_press()).
// Set to 0 to show only PERFECT / GOOD / MISS.
#define UI_HAVE_OFFSET 1

#define HUD_REFRESH_MS   100    // score/combo redraw rate while playing
#define RESULTS_PAGE_MS  2000   // results screen rotates its second line
#define MENU_POLL_MS     10

// ---- Song table (key '1' -> index 0, and so on) ----
static const char *song_titles[] = {
    "Twinkle Twinkle",   // Key '1' (Easy)
    "Never Gonna Give",  // Key '2' (Medium)
    "Flurry"             // Key '3' (Hard)
};
static const char *mode_names[] = {"Easy", "Medium", "Hard"};
#define NUM_SONGS ((int)(sizeof(song_titles) / sizeof(song_titles[0])))

// ---- State ----
static int      selected_song_index = 0;
static bool     start_pending = false;    // menu returned a song, game hasn't started yet
static int      prev_phase = -1;
static unsigned last_seq = 0;             // last judgement_seq we displayed
static uint32_t last_hud_ms = 0;
static int32_t  offset_sum = 0;           // for average timing offset
static int      offset_count = 0;
static uint32_t last_score_shown = 0xFFFFFFFFu;   // 7-seg cache

// ---- 7-seg helper: only shift data out when the score actually changed ----
static void show_score(uint32_t score) {
    if (score == last_score_shown) return;
    last_score_shown = score;
    seg7_update(score);
}

// ---- LCD helpers: full 16-char rows, and skip the I2C write if nothing changed ----
static char shown[2][17];

static void lcd_row(uint8_t row, const char *text) {
    char buf[17];
    snprintf(buf, sizeof buf, "%-16.16s", text);   // pad/truncate to 16 columns
    if (strcmp(buf, shown[row]) == 0) return;      // same text: no I2C traffic
    strcpy(shown[row], buf);
    lcd_set_cursor(row, 0);
    lcd_write_string(buf);
}

static void lcd_wipe(void) {
    lcd_clear();
    shown[0][0] = '\0';
    shown[1][0] = '\0';
}

static const char *judge_name(judgement_t j) {
    switch (j) {
        case JUDGE_PERFECT: return "PERFECT";
        case JUDGE_GOOD:    return "GOOD";
        case JUDGE_MISS:    return "MISS";
        default:            return "";
    }
}

static void draw_menu(void) {
    char line[24];
    lcd_row(0, song_titles[selected_song_index]);
    snprintf(line, sizeof line, "Mode:%-6s #:OK", mode_names[selected_song_index]);
    lcd_row(1, line);
}

// ---- Public API ----
void ui_init(void) {
    keypad_init();
    lcd_init();
    seg7_init();

    selected_song_index = 0;
    start_pending = false;
    prev_phase = -1;
    last_score_shown = 0xFFFFFFFFu;
    shown[0][0] = '\0';
    shown[1][0] = '\0';
}

// Interactive menu loop called by main.c during GAME_MENU
int ui_menu_select_song(int count) {
    char line[24];
    int limit = (count < NUM_SONGS) ? count : NUM_SONGS;
    if (limit < 1) limit = 1;

    // If we handed back a song last time but the game never reached PLAYING,
    // game_start() failed (chart or audio file could not be opened).
    if (start_pending) {
        start_pending = false;
        lcd_wipe();
        lcd_row(0, "Song failed");
        lcd_row(1, "Check SD card");
        sleep_ms(1500);
    }

    lcd_wipe();
    snprintf(line, sizeof line, "Select Song 1-%d", limit);
    lcd_row(0, line);
    lcd_row(1, "# to confirm");
    sleep_ms(1200);                         // brief instructional flash

    draw_menu();

    bool confirmed = false;
    while (!confirmed) {
        char key = keypad_get_key();

        if (key >= '1' && key < '1' + limit) {
            int idx = key - '1';
            if (idx != selected_song_index) {
                selected_song_index = idx;
                draw_menu();                // redraw only when the choice changes
            }
        } else if (key == '#' || key == 'D') {
            confirmed = true;
        }
        sleep_ms(MENU_POLL_MS);
    }

    lcd_row(0, "Loading Game...");
    lcd_row(1, "");
    start_pending = true;                   // cleared in ui_update() once PLAYING is seen
    return selected_song_index;
}

uint8_t ui_menu_select_difficulty(void) {
    // Difficulty (1, 2 or 3) is tied directly to the selected song index
    return (uint8_t)(selected_song_index + 1);
}

void ui_update(const game_state_t *st) {
    char line[24];
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // ---- Phase change bookkeeping ----
    if ((int)st->phase != prev_phase) {
        prev_phase = (int)st->phase;
        if (st->phase == GAME_PLAYING) {
            start_pending = false;          // game started fine
            offset_sum = 0;
            offset_count = 0;
            last_seq = st->judgement_seq;
            last_hud_ms = 0;                // force an immediate first draw
            last_score_shown = 0xFFFFFFFFu; // force 7-seg to redraw (shows 0)
            lcd_row(1, "");                 // wipe leftover judgement text
        }
    }

    switch (st->phase) {
        case GAME_PLAYING:
            show_score(st->score);

            // New hit/miss since last call? Show it right away.
            if (st->judgement_seq != last_seq) {
                last_seq = st->judgement_seq;
#if UI_HAVE_OFFSET
                if (st->last_judgement != JUDGE_MISS) {
                    offset_sum += st->last_offset_ms;
                    offset_count++;
                    snprintf(line, sizeof line, "%-7s %+4dms",
                             judge_name(st->last_judgement), (int)st->last_offset_ms);
                } else {
                    snprintf(line, sizeof line, "%s", judge_name(st->last_judgement));
                }
#else
                snprintf(line, sizeof line, "%s", judge_name(st->last_judgement));
#endif
                lcd_row(1, line);
            }

            // Score and combo: throttled so I2C writes don't steal time from audio
            if (now - last_hud_ms >= HUD_REFRESH_MS) {
                last_hud_ms = now;
                snprintf(line, sizeof line, "S:%06lu C:%-4u",
                         (unsigned long)st->score, (unsigned)st->combo);
                lcd_row(0, line);
            }
            break;

        case GAME_RESULTS: {
            show_score(st->score);

            snprintf(line, sizeof line, "Final:%lu", (unsigned long)st->score);
            lcd_row(0, line);

            unsigned total = (unsigned)st->hits + (unsigned)st->misses;
            unsigned acc = total ? ((unsigned)st->hits * 100u) / total : 0;
#if UI_HAVE_OFFSET
            const int pages = 3;
#else
            const int pages = 2;
#endif
            int page = (int)((now / RESULTS_PAGE_MS) % pages);

            if (page == 0) {
                snprintf(line, sizeof line, "Hits:%u Miss:%u",
                         (unsigned)st->hits, (unsigned)st->misses);
            } else if (page == 1) {
                snprintf(line, sizeof line, "Acc:%u%% Cmb:%u",
                         acc, (unsigned)st->max_combo);
            } else {
#if UI_HAVE_OFFSET
                int avg = offset_count ? (int)(offset_sum / offset_count) : 0;
                snprintf(line, sizeof line, "Avg:%+dms", avg);
#else
                line[0] = '\0';
#endif
            }
            lcd_row(1, line);
            break;
        }

        case GAME_MENU:
        default:
            break;
    }
}