#include "ui.h"
#include "keypad.h"
#include "lcd.h"
#include "seg7.h"
#include <stdio.h>

// Local tracking for song & mode selection
static int selected_song_index = 0;
// Preset modes/difficulties tied directly to the song keys:
// 1 -> Twinkle Twinkle (Easy)
// 2 -> Never Gonna Give You Up (Medium)
// 3 -> Flurry (Hard)

static const char *song_titles[] = {
    "Twinkle Twinkle", // Key '1'
    "Never Gonna Give",// Key '2'
    "Flurry"           // Key '3'
};

static const char *mode_names[] = {
    "Easy",   // Mode for Song 0
    "Medium", // Mode for Song 1
    "Hard"    // Mode for Song 2
};

void ui_init(void) {
    keypad_init();
    lcd_init();
    seg7_init();

    selected_song_index = 0; // Default to Song 1
}

// Interactive menu loop called by main.c during GAME_MENU
int ui_menu_select_song(int count) {
    char lcd_buffer[32]; //for formatting LCD output
    bool confirmed = false;

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("Select Song (1-3)");
    lcd_set_cursor(1, 0);
    lcd_write_string("Press #: Confirm");
    sleep_ms(1200); // Brief instructional flash
    
    while (!confirmed) {
        char key = keypad_get_key();

        if (key != 0) {
            if (key == '1') {
                selected_song_index = 0; // Twinkle Twinkle (Easy)
            } else if (key == '2') {
                selected_song_index = 1; // Never Gonna Give (Medium)
            } else if (key == '3') {
                selected_song_index = 2; // Flurry (Hard)
            } else if (key == '#' || key == 'D') {
                confirmed = true; // Confirm selection
            }
        }

        // --- Update LCD display in real-time ---
        // Line 0: Show selected song title
        lcd_set_cursor(0, 0);
        snprintf(lcd_buffer, sizeof(lcd_buffer), "%-16s", song_titles[selected_song_index]);
        lcd_write_string(lcd_buffer);

        // Line 1: Show the locked mode for this song and confirmation hint
        lcd_set_cursor(1, 0);
        snprintf(lcd_buffer, sizeof(lcd_buffer), "Mode:%s (Press#)", mode_names[selected_song_index]);
        lcd_write_string(lcd_buffer);

        sleep_ms(50); // Small delay to prevent CPU hammering during polling
    }

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("Loading Game...");
    return selected_song_index;
}

uint8_t ui_menu_select_difficulty(void) {
    // Return the difficulty level (1, 2, or 3) tied directly to the selected song index
    return (uint8_t)(selected_song_index + 1);
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