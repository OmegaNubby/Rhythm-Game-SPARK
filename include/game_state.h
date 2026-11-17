// game_state.h - shared types between the engine (P1), renderer, and UI.
// P1 implements game_*; UI person implements ui_*; renderer uses note_view_t.

#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    GAME_MENU,
    GAME_PLAYING,
    GAME_PAUSED,
    GAME_RESULTS
} game_phase_t;

typedef enum {
    JUDGE_NONE,
    JUDGE_GOOD,
    JUDGE_PERFECT,
    JUDGE_MISS
} judgement_t;

typedef struct {
    game_phase_t phase;
    uint32_t     score;            // shown on the 7-seg
    uint16_t     combo;
    uint16_t     max_combo;
    uint16_t     hits;
    uint16_t     misses;
    uint32_t     song_time_ms;     // audio clock at last game_update()
    judgement_t  last_judgement;   // most recent result, for hit feedback
    uint8_t      last_lane;
    uint16_t     judgement_seq;    // increments on every new judgement
} game_state_t;

// What the renderer needs to draw one note.
// y values are in permille of the playfield: 0 = top (spawn),
// 1000 = hit line. Values >1000 mean the note is past the line.
typedef struct {
    uint8_t  lane;          // 0..3
    bool     is_hold;
    bool     holding;       // hold note currently being held
    int16_t  y_head;        // leading edge (closest to hit line)
    int16_t  y_tail;        // trailing edge (equals y_head for taps)
} note_view_t;

#define MAX_NOTE_VIEWS 48

// ---- Implemented by P1 (engine) ----
void game_init(void);
void game_start(int song_index, uint8_t difficulty);   // difficulty 1..3
void game_stop(void);
void game_update(void);                                // call every frame
const game_state_t *game_get_state(void);
int  game_get_note_views(note_view_t *out, int max);   // returns count

// ---- Implemented by the UI person ----
void ui_init(void);
void ui_update(const game_state_t *st);                // refreshes LCD + 7-seg
int  ui_menu_select_song(int count);                   // returns chosen index
uint8_t ui_menu_select_difficulty(void);               // 1..3

#endif // GAME_STATE_H