#ifndef SEG7_H
#define SEG7_H

#include <stdint.h>

// Starts the background refresh (multiplexing runs on a 1 kHz timer, so the
// display stays steady no matter what the game loop is doing).
void seg7_init(void);

// Set the score to show (clamped to 9999). Cheap: just stores the value.
void seg7_update(uint32_t score);

// TEST ONLY: freeze the display on one raw 16-bit word (select << 8 | segments).
void seg7_raw(uint16_t word);

// Leave raw mode and go back to showing the score.
void seg7_resume(void);

#endif // SEG7_H