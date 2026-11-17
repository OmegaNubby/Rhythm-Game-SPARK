// render.h - interface between the engine (P1) and the TFT renderer.
// Renderer owner implements; engine/main call. Stub exists in stubs.c.

#ifndef RENDER_H
#define RENDER_H

#include "game_state.h"

void render_init(void);

// Draw one frame. Redraw only what changed where possible.
void render_frame(const game_state_t *st, const note_view_t *notes, int count);

#endif // RENDER_H