#include "ui.h"
#include "keypad.h"
#include "lcd.h"
#include "seg7.h"
#include <stdio.h>

// Local tracking for menu selections
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

// Interactive menu loop called by main.c during GAME_MENU
int ui_menu_select_song(int count) {
    char lcd_buffer[32];
    bool confirmed = false;

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("Select Song & Diff");
    
    while (!confirmed) {
        char key = keypad_get_key();

        if (key != 0) {
            if (key == '2' || key == 'B') {
                selected_song_index = (selected_song_index + 1) % count;
            } else if (key == '1' || key == 'A') {
                selected_song_index = (selected_song_index - 1 + count) % count;
            } else if (key >= '1' && key <= '3') {
                // If pressing 1, 2, 3 directly select difficulty or song depending on workflow
                // Let's map 1-3 to difficulty for quick selection:
                selected_difficulty = key - '0';
            } else if (key == '#' || key == 'D') {
                confirmed = true; // Confirm selection
            }
        }

        // Update LCD display in real-time while in menu
        lcd_set_cursor(0, 0);
        snprintf(lcd_buffer, sizeof(lcd_buffer), "Song: %d / %d     ", selected_song_index + 1, count);
        lcd_write_string(lcd_buffer);

        lcd_set_cursor(1, 0);
        snprintf(lcd_buffer, sizeof(lcd_buffer), "Diff: %d (Press #)", selected_difficulty);
        lcd_write_string(lcd_buffer);

        sleep_ms(50); // Small delay to prevent CPU hammering during polling
    }

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("Loading Game...");
    return selected_song_index;
}

uint8_t ui_menu_select_difficulty(void) {
    // Difficulty was already captured and confirmed in ui_menu_select_song
    if (selected_difficulty < 1) selected_difficulty = 1;
    if (selected_difficulty > 3) selected_difficulty = 3;
    return selected_difficulty;
}

void ui_update(const game_state_t *st) {
    char lcd_buffer[32];

    switch (st->phase) {
        case GAME_PLAYING:
            // Update 7-segment display with real-time score
            seg7_update(st->score);

            // Update LCD with live score & combo
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

        case GAME_MENU:
        default:
            break;
    }
}