#ifndef KEYPAD_H
#define KEYPAD_H
 
// Basic polling 4x4 keypad. Call keypad_get_key() once per main-loop tick.
// Returns the key ('0'-'9', 'A'-'D', '*', '#') once per press, or 0 if none.
void keypad_init(void);
char keypad_get_key(void);
 
#endif
 