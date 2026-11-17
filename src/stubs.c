// stubs.c - WEAK fake implementations so the engine runs with no hardware.
// Each real module (tile_input.c, chart.c, audio.c, render.c, ui.c) defines
// the same functions normally; the linker prefers those over these weak ones,
// so stubs disappear automatically as teammates finish their parts.
//
// The stub tile input "auto-plays" the stub chart with slightly early/late
// presses, so you can watch score/combo/holds in the serial log.

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "game_state.h"
#include "tile_input.h"
#include "chart.h"
#include "audio.h"
#include "render.h"

#define WEAK __attribute__((weak))

// ------------------------------------------------------------- stub chart
static const chart_note_t stub_chart[] = {
    {1000, 0, 0}, {1500, 1, 0}, {2000, 2, 0}, {2500, 3, 0},
    {3000, 0, 800},                       // hold note
    {3500, 2, 0}, {4500, 1, 0}, {5000, 3, 0}, {5500, 0, 0}, {6000, 1, 0},
};
#define STUB_N ((int)(sizeof(stub_chart) / sizeof(stub_chart[0])))
#define STUB_END_MS 8000
static int stub_idx;

WEAK bool        chart_init(void) { return true; }
WEAK int         chart_count(void) { return 1; }
WEAK const char *chart_audio_path(int i) { (void)i; return "stub.wav"; }
WEAK bool chart_get_info(int i, chart_info_t *o) {
    (void)i;
    memset(o, 0, sizeof(*o));
    strcpy(o->title, "Stub Song");
    o->difficulty = 2; o->bpm = 120; o->length_ms = STUB_END_MS;
    return true;
}
WEAK bool chart_open(int i) { (void)i; stub_idx = 0; return true; }
WEAK bool chart_next_note(chart_note_t *o) {
    if (stub_idx >= STUB_N) return false;
    *o = stub_chart[stub_idx++];
    return true;
}

// ------------------------------------------------------------- stub audio
static bool     stub_playing;
static uint32_t stub_t0;

WEAK void audio_init(void) {}
WEAK bool audio_load(const char *p) { (void)p; stub_playing = false; return true; }
WEAK void audio_start(void) { stub_t0 = to_ms_since_boot(get_absolute_time()); stub_playing = true; }
WEAK void audio_stop(void)  { stub_playing = false; }
WEAK void audio_refill(void) {}
WEAK uint32_t audio_time_ms(void) {
    return stub_playing ? to_ms_since_boot(get_absolute_time()) - stub_t0 : 0;
}
WEAK bool audio_is_finished(void) { return audio_time_ms() > STUB_END_MS; }

// -------------------------------------------------- stub tiles (auto-play)
static bool stub_emitted[2 * 16];
static bool stub_down[NUM_LANES];
static int  stub_gen = -1;       // restarts when a new song starts

static uint32_t stub_event_time(int e) {
    int i = e / 2;
    if (e % 2 == 0)                           // press: -20, 0, +20 ms jitter
        return stub_chart[i].time_ms + ((i % 3) - 1) * 20;
    return stub_chart[i].time_ms + (stub_chart[i].duration_ms ? stub_chart[i].duration_ms : 80);
}

WEAK void tile_input_init(void) {}
WEAK void tile_input_poll(void) {}
WEAK void tile_input_set_thresholds(uint8_t l, uint16_t p, uint16_t r) { (void)l; (void)p; (void)r; }
WEAK bool tile_input_is_down(uint8_t lane) { return lane < NUM_LANES && stub_down[lane]; }

WEAK bool tile_input_get_event(tile_event_t *out) {
    uint32_t now = audio_time_ms();
    if (now < 50) {                           // new song: reset the script
        memset(stub_emitted, 0, sizeof(stub_emitted));
        memset(stub_down, 0, sizeof(stub_down));
    }
    int best = -1;
    uint32_t best_t = 0xFFFFFFFF;
    for (int e = 0; e < 2 * STUB_N; e++) {
        uint32_t t = stub_event_time(e);
        if (!stub_emitted[e] && t <= now && t < best_t) { best = e; best_t = t; }
    }
    if (best < 0) return false;

    int i = best / 2;
    stub_emitted[best] = true;
    out->lane = stub_chart[i].lane;
    out->type = (best % 2 == 0) ? TILE_PRESS : TILE_RELEASE;
    out->pressure = 2000;
    out->time_ms = best_t;
    stub_down[out->lane] = (out->type == TILE_PRESS);
    return true;
}

// ------------------------------------------------- stub render + UI (serial)
WEAK void render_init(void) {}
WEAK void render_frame(const game_state_t *s, const note_view_t *n, int c) {
    static uint32_t last;
    if (s->song_time_ms - last >= 500) {
        last = s->song_time_ms;
        printf("t=%lu notes=%d score=%lu combo=%u hit=%u miss=%u\n",
               (unsigned long)s->song_time_ms, c, (unsigned long)s->score,
               s->combo, s->hits, s->misses);
    }
    (void)n;
}
WEAK void    ui_init(void) {}
WEAK void    ui_update(const game_state_t *s) { (void)s; }
WEAK int     ui_menu_select_song(int count) { (void)count; return 0; }
WEAK uint8_t ui_menu_select_difficulty(void) { return 2; }