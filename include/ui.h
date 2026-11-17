#ifndef UI_H
#define UI_H

#include "game_state.h"

// Public UI API functions required by the engine
void ui_init(void);
void ui_update(const game_state_t *st);
int  ui_menu_select_song(int count);
uint8_t ui_menu_select_difficulty(void);

#endif // UI_H