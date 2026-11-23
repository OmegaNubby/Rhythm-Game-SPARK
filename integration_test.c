
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "pins.h"
#include "game_state.h"
#include "ui.h"
#include "lcd.h"
#include "audio.h"
#include "chart.h"

#define TEST_UPDATE_INTERVAL_US 100000

static void show_error(const char *message) {
    printf("INTEGRATION TEST ERROR: %s\n", message);

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("TEST ERROR:");
    lcd_set_cursor(1, 0);
    lcd_write_string(message);

    while (true) {
        sleep_ms(1000);
    }
}

static void init_fsr_adc(void) {
    adc_init();

    for (int lane = 0; lane < NUM_LANES; lane++) {
        adc_gpio_init(PIN_FSR_BASE + lane);
    }
}

static uint16_t read_fsr(int lane) {
    adc_select_input(FSR_ADC_CHANNEL(lane));
    return adc_read();
}

static int count_chart_notes(void) {
    chart_note_t note;
    int count = 0;

    while (chart_next_note(&note)) {
        if (note.lane >= NUM_LANES) {
            printf("WARNING: Invalid lane %u in chart\n", note.lane);
        }

        count++;
    }

    return count;
}

int main(void) {
    stdio_init_all();
    sleep_ms(1500);

    printf("\n===============================\n");
    printf(" RHYTHM GAME INTEGRATION TEST\n");
    printf("===============================\n");

    // ---------------------------------------------------------
    // 1. Initialize hardware modules
    // ---------------------------------------------------------

    printf("\n[1] Initializing UI...\n");
    ui_init();

    printf("[2] Initializing audio...\n");
    audio_init();

    printf("[3] Initializing FSR ADC...\n");
    init_fsr_adc();

    // ---------------------------------------------------------
    // 2. Initialize SD card and scan for charts
    // ---------------------------------------------------------

    printf("[4] Initializing chart system...\n");

    if (!chart_init()) {
        show_error("SD INIT FAILED");
    }

    int song_count = chart_count();

    printf("Found %d chart(s)\n", song_count);

    if (song_count <= 0) {
        show_error("NO CHARTS FOUND");
    }

    // ---------------------------------------------------------
    // 3. Select a song using the existing keypad UI
    // ---------------------------------------------------------

    printf("\n[5] Waiting for song selection...\n");

    int song = ui_menu_select_song(song_count);

    if (song < 0 || song >= song_count) {
        show_error("INVALID SONG");
    }

    uint8_t difficulty = ui_menu_select_difficulty();

    chart_info_t info;

    if (!chart_get_info(song, &info)) {
        show_error("CHART INFO FAILED");
    }

    printf("\nSelected chart: %s\n", info.title);
    printf("BPM: %u\n", info.bpm);
    printf("Length: %lu ms\n", (unsigned long)info.length_ms);
    printf("Difficulty selection: %u\n", difficulty);

    // ---------------------------------------------------------
    // 4. Test chart parsing
    // ---------------------------------------------------------

    printf("\n[6] Opening chart...\n");

    if (!chart_open(song)) {
        show_error("CHART OPEN FAILED");
    }

    int note_count = count_chart_notes();

    printf("Parsed %d chart notes\n", note_count);

    if (note_count <= 0) {
        show_error("CHART HAS NO NOTES");
    }

    // Reopen to reset the chart note stream.
    if (!chart_open(song)) {
        show_error("CHART RESET FAILED");
    }

    // ---------------------------------------------------------
    // 5. Load the corresponding WAV file
    // ---------------------------------------------------------

    const char *wav_path = chart_audio_path(song);

    if (wav_path == NULL) {
        show_error("NO AUDIO PATH");
    }

    printf("\n[7] Loading audio: %s\n", wav_path);

    if (!audio_load(wav_path)) {
        show_error("AUDIO LOAD FAILED");
    }

    printf("Audio loaded successfully.\n");

    // ---------------------------------------------------------
    // 6. Start playback and monitor all hardware
    // ---------------------------------------------------------

    printf("\n[8] Starting playback...\n");

    game_state_t test_state = {0};
    test_state.phase = GAME_PLAYING;

    audio_start();

    uint32_t last_update = time_us_32();
    uint32_t last_fsr_print = 0;

    while (!audio_is_finished()) {
        // Keep the PWM audio buffer filled.
        audio_refill();

        uint32_t now = time_us_32();

        // Update UI and sample FSRs approximately 10 times/sec.
        if ((uint32_t)(now - last_update) >=
            TEST_UPDATE_INTERVAL_US) {

            last_update = now;

            uint32_t elapsed_ms = audio_time_ms();

            test_state.song_time_ms = elapsed_ms;

            // Use elapsed seconds as a changing display value.
            // This is test instrumentation, not game scoring.
            test_state.score = elapsed_ms / 1000;

            ui_update(&test_state);

            // Read all four FSR ADC channels.
            uint16_t fsr_values[NUM_LANES];

            for (int lane = 0; lane < NUM_LANES; lane++) {
                fsr_values[lane] = read_fsr(lane);
            }

            printf(
                "Audio: %lu ms | FSR: %u %u %u %u\n",
                (unsigned long)elapsed_ms,
                fsr_values[0],
                fsr_values[1],
                fsr_values[2],
                fsr_values[3]
            );

            last_fsr_print = elapsed_ms;
        }
    }

    // ---------------------------------------------------------
    // 7. Finish and display results
    // ---------------------------------------------------------

    audio_stop();

    test_state.phase = GAME_RESULTS;
    test_state.song_time_ms = audio_time_ms();

    ui_update(&test_state);

    printf("\n===============================\n");
    printf(" INTEGRATION TEST COMPLETE\n");
    printf("===============================\n");
    printf("Chart: %s\n", info.title);
    printf("Notes parsed: %d\n", note_count);
    printf("Playback time: %lu ms\n",
           (unsigned long)test_state.song_time_ms);
    printf("FSR monitoring: completed\n");

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("TEST COMPLETE!");
    lcd_set_cursor(1, 0);
    lcd_write_string("Check Serial");

    while (true) {
        sleep_ms(1000);
    }

    return 0;
}
