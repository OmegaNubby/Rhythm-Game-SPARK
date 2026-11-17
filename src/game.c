// game.c - Person 1: game engine.
//
// Rules:
//  * The only clock is audio_time_ms(). tile events must carry timestamps
//    on that same clock (see tile_input.h).
//  * No hardware access here: tiles, chart, audio come through headers.
//
// Flow each frame (game_update):
//   1. consume tile events      -> judge presses / releases
//   2. spawn upcoming notes     -> chart_next_note() within LEAD_MS
//   3. expire notes             -> misses, hold completion/break
//   4. detect end of song       -> GAME_RESULTS

#include "game_state.h"
#include "tile_input.h"
#include "chart.h"
#include "audio.h"
#include <string.h>

#define LEAD_MS              1500   // note spawns this long before its hit time
#define MAX_ACTIVE           MAX_NOTE_VIEWS
#define PERFECT_BASE_MS      40     // widened at lower difficulty
#define GOOD_BASE_MS         80
#define HOLD_RELEASE_TOL_MS  120    // may release this early and still count
#define HOLD_BONUS           200
#define SCORE_PERFECT        300
#define SCORE_GOOD           100

typedef enum { SLOT_FREE, SLOT_PENDING, SLOT_HOLDING } slot_state_t;

typedef struct {
    slot_state_t state;
    chart_note_t note;
} slot_t;

static slot_t        slots[MAX_ACTIVE];
static game_state_t  st;
static chart_note_t  lookahead;
static bool          have_lookahead;
static uint32_t      perfect_ms = PERFECT_BASE_MS;
static uint32_t      good_ms    = GOOD_BASE_MS;

// ---------------------------------------------------------------- helpers

static inline uint32_t note_end(const chart_note_t *n) {
    return n->time_ms + n->duration_ms;
}

static void set_judgement(judgement_t j, uint8_t lane) {
    st.last_judgement = j;
    st.last_lane = lane;
    st.judgement_seq++;
}

static void register_hit(bool perfect, uint8_t lane) {
    uint32_t base = perfect ? SCORE_PERFECT : SCORE_GOOD;
    uint32_t mult = 1 + st.combo / 20;          // x2 at 20 combo ... x4 max
    if (mult > 4) mult = 4;
    st.score += base * mult;
    st.combo++;
    if (st.combo > st.max_combo) st.max_combo = st.combo;
    st.hits++;
    set_judgement(perfect ? JUDGE_PERFECT : JUDGE_GOOD, lane);
}

static void register_miss(uint8_t lane) {
    st.combo = 0;
    st.misses++;
    set_judgement(JUDGE_MISS, lane);
}

static void clear_slots(void) {
    memset(slots, 0, sizeof(slots));
}

static int find_free_slot(void) {
    for (int i = 0; i < MAX_ACTIVE; i++)
        if (slots[i].state == SLOT_FREE) return i;
    return -1;
}

static bool any_active(void) {
    for (int i = 0; i < MAX_ACTIVE; i++)
        if (slots[i].state != SLOT_FREE) return true;
    return false;
}

// ----------------------------------------------------------- event handling

static void handle_press(const tile_event_t *ev) {
    int best = -1;
    int32_t best_err = 0x7FFFFFFF;

    for (int i = 0; i < MAX_ACTIVE; i++) {
        if (slots[i].state != SLOT_PENDING || slots[i].note.lane != ev->lane)
            continue;
        int32_t err = (int32_t)ev->time_ms - (int32_t)slots[i].note.time_ms;
        if (err < 0) err = -err;
        if (err <= (int32_t)good_ms && err < best_err) {
            best = i;
            best_err = err;
        }
    }
    if (best < 0) return;   // stray tap: no penalty

    register_hit(best_err <= (int32_t)perfect_ms, ev->lane);
    if (slots[best].note.duration_ms > 0)
        slots[best].state = SLOT_HOLDING;
    else
        slots[best].state = SLOT_FREE;
}

static void handle_release(const tile_event_t *ev) {
    for (int i = 0; i < MAX_ACTIVE; i++) {
        if (slots[i].state != SLOT_HOLDING || slots[i].note.lane != ev->lane)
            continue;
        if (ev->time_ms + HOLD_RELEASE_TOL_MS >= note_end(&slots[i].note)) {
            st.score += HOLD_BONUS;              // held to the end
        } else {
            register_miss(ev->lane);             // let go too early
        }
        slots[i].state = SLOT_FREE;
    }
}

// ----------------------------------------------------------------- public

void game_init(void) {
    memset(&st, 0, sizeof(st));
    st.phase = GAME_MENU;
    clear_slots();
    have_lookahead = false;
}

void game_start(int song_index, uint8_t difficulty) {
    if (difficulty < 1) difficulty = 1;
    if (difficulty > 3) difficulty = 3;

    memset(&st, 0, sizeof(st));
    clear_slots();

    // Harder difficulty = tighter windows.
    perfect_ms = PERFECT_BASE_MS + (3 - difficulty) * 10;
    good_ms    = GOOD_BASE_MS    + (3 - difficulty) * 20;

    if (!chart_open(song_index) ||
        !audio_load(chart_audio_path(song_index))) {
        st.phase = GAME_MENU;       // could not start; back to menu
        return;
    }

    tile_event_t junk;
    while (tile_input_get_event(&junk)) { }   // drop stale presses

    have_lookahead = chart_next_note(&lookahead);
    audio_start();
    st.phase = GAME_PLAYING;
}

void game_stop(void) {
    audio_stop();
    clear_slots();
    have_lookahead = false;
    st.phase = GAME_MENU;
}

void game_update(void) {
    if (st.phase != GAME_PLAYING) return;

    uint32_t now = audio_time_ms();
    st.song_time_ms = now;

    // 1. tile events
    tile_event_t ev;
    while (tile_input_get_event(&ev)) {
        if (ev.lane >= NUM_LANES) continue;
        if (ev.type == TILE_PRESS) handle_press(&ev);
        else                       handle_release(&ev);
    }

    // 2. spawn upcoming notes
    while (have_lookahead && lookahead.time_ms <= now + LEAD_MS) {
        int s = find_free_slot();
        if (s < 0) break;                       // retry next frame
        slots[s].state = SLOT_PENDING;
        slots[s].note  = lookahead;
        have_lookahead = chart_next_note(&lookahead);
    }

    // 3. expire notes
    for (int i = 0; i < MAX_ACTIVE; i++) {
        if (slots[i].state == SLOT_PENDING) {
            if (now > slots[i].note.time_ms + good_ms) {
                register_miss(slots[i].note.lane);
                slots[i].state = SLOT_FREE;
            }
        } else if (slots[i].state == SLOT_HOLDING) {
            uint32_t end = note_end(&slots[i].note);
            if (now >= end) {
                st.score += HOLD_BONUS;          // reached the end held
                slots[i].state = SLOT_FREE;
            } else if (!tile_input_is_down(slots[i].note.lane) &&
                       now + HOLD_RELEASE_TOL_MS < end) {
                register_miss(slots[i].note.lane);   // backup: missed release
                slots[i].state = SLOT_FREE;
            }
        }
    }

    // 4. end of song
    if (!have_lookahead && !any_active() && audio_is_finished()) {
        audio_stop();
        st.phase = GAME_RESULTS;
    }
}

const game_state_t *game_get_state(void) {
    return &st;
}

int game_get_note_views(note_view_t *out, int max) {
    uint32_t now = st.song_time_ms;
    int n = 0;

    for (int i = 0; i < MAX_ACTIVE && n < max; i++) {
        if (slots[i].state == SLOT_FREE) continue;

        const chart_note_t *c = &slots[i].note;
        int32_t head = ((int32_t)now - (int32_t)(c->time_ms - LEAD_MS)) * 1000 / LEAD_MS;
        int32_t tail = head;
        bool is_hold = c->duration_ms > 0;

        if (is_hold) {
            tail = ((int32_t)now - (int32_t)(note_end(c) - LEAD_MS)) * 1000 / LEAD_MS;
            // tail is the trailing (upper) edge; head the leading (lower) edge
            if (slots[i].state == SLOT_HOLDING && head > 1000) head = 1000;
        }

        out[n].lane    = c->lane;
        out[n].is_hold = is_hold;
        out[n].holding = (slots[i].state == SLOT_HOLDING);
        out[n].y_head  = (int16_t)head;
        out[n].y_tail  = (int16_t)tail;
        n++;
    }
    return n;
}