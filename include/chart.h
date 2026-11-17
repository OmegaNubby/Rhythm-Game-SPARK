#ifndef CHART_H
#define CHART_H

#include <stdint.h>
#include <stdbool.h>

#define CHART_TITLE_MAX 16   // fits the 16x2 LCD

typedef struct {
    uint32_t time_ms;       // when the note should be hit (song time)
    uint8_t  lane;          // 0..3
    uint16_t duration_ms;   // 0 = tap, >0 = hold note
} chart_note_t;

typedef struct {
    char     title[CHART_TITLE_MAX + 1];
    uint8_t  difficulty;    // 1..3
    uint16_t bpm;
    uint32_t length_ms;
} chart_info_t;

// Mount the SD card and scan for songs. Returns false if no card.
bool chart_init(void);

// Number of songs found (target: ~3).
int  chart_count(void);

// Metadata for the menu. index in 0..chart_count()-1.
bool chart_get_info(int index, chart_info_t *out);

// Path of the song's WAV file, to pass to audio_load().
const char *chart_audio_path(int index);

// Open a song's chart and reset the note stream.
bool chart_open(int index);

// Next note in time order. Returns false at end of chart.
bool chart_next_note(chart_note_t *out);

#endif // CHART_H