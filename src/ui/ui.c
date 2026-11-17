#include "ui.h"
#include "keypad.h"
#include "lcd.h"
#include "seg7.h"
#include <stdio.h>

// Internal UI selection trackers
static int selected_song_index = 0;
static uint8_t selected_difficulty = 1; // 1..3

void ui_init(void) {
    // Initialize hardware peripherals
    keypad_init();
    lcd_init();
    seg7_init();

    selected_song_index = 0;
    selected_difficulty = 1;
}

void ui_update(const game_state_t *st) {
    char lcd_buffer[32];
    char key = keypad_get_key(); // Non-blocking or scanned key from keypad.c

    switch (st->phase) {
        case GAME_MENU:
            // Handle menu navigation via keypad input
            if (key == '2') {
                selected_song_index++; // Cycle through songs
            } else if (key >= '1' && key <= '3') {
                selected_difficulty = key - '0'; // Set difficulty 1..3
            }

            lcd_set_cursor(0, 0);
            snprintf(lcd_buffer, sizeof(lcd_buffer), "Song: %d   Diff:%d", selected_song_index + 1, selected_difficulty);
            lcd_write_string(lcd_buffer);
            lcd_set_cursor(1, 0);
            lcd_write_string("Press Start/Key ");
            break;

        case GAME_PLAYING:
            // Update 7-segment displays with the real-time score from game_state
            seg7_update(st->score);

            // Update LCD with live gameplay info (score & combo)
            lcd_set_cursor(0, 0);
            snprintf(lcd_buffer, sizeof(lcd_buffer), "Score: %lu", (unsigned long)st->score);
            lcd_write_string(lcd_buffer);

            lcd_set_cursor(1, 0);
            snprintf(lcd_buffer, sizeof(lcd_buffer), "Combo: %u (Max:%u)", st->combo, st->max_combo);
            lcd_write_string(lcd_buffer);
            break;

        case GAME_PAUSED:
            lcd_set_cursor(0, 0);
            lcd_write_string("   -- PAUSED -- ");
            lcd_set_cursor(1, 0);
            lcd_write_string("Waiting to resume");
            break;

        case GAME_RESULTS:
            // Final score on 7-segment
            seg7_update(st->score);

            // Display results summary on LCD
            lcd_set_cursor(0, 0);
            snprintf(lcd_buffer, sizeof(lcd_buffer), "Final: %lu", (unsigned long)st->score);
            lcd_write_string(lcd_buffer);

            lcd_set_cursor(1, 0);
            snprintf(lcd_buffer, sizeof(lcd_buffer), "Hits:%u Miss:%u", st->hits, st->misses);
            lcd_write_string(lcd_buffer);
            break;
    }
}

int ui_menu_select_song(int count) {
    if (count > 0) {
        selected_song_index = selected_song_index % count;
    }
    return selected_song_index;
}

uint8_t ui_menu_select_difficulty(void) {
    if (selected_difficulty < 1) selected_difficulty = 1;
    if (selected_difficulty > 3) selected_difficulty = 3;
    return selected_difficulty;
}