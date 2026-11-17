#include "keypad.h"
#include "pins.h"
#include "pico/stdlib.h"

// Rows are 4 consecutive input pins, columns are 4 consecutive output pins.
// Better: define these two in pins.h (defaults match your breadboard, GP2-9).
#ifndef KEYPAD_ROW_FIRST_PIN
#define KEYPAD_ROW_FIRST_PIN 2      // rows    = GP2-GP5
#endif
#ifndef KEYPAD_COL_FIRST_PIN
#define KEYPAD_COL_FIRST_PIN 6      // columns = GP6-GP9
#endif

#define DEBOUNCE_MS 20

// Indexed [column][row] (same layout as the old "DCBA#9630852*741" keymap)
static const char keymap[4][4] = {
    {'D', 'C', 'B', 'A'},
    {'#', '9', '6', '3'},
    {'0', '8', '5', '2'},
    {'*', '7', '4', '1'},
};

static char     last_raw = 0;
static char     stable = 0;
static uint32_t last_change_ms = 0;

void keypad_init(void) {
    for (int i = 0; i < 4; i++) {
        uint row = KEYPAD_ROW_FIRST_PIN + i;
        gpio_init(row);
        gpio_set_dir(row, GPIO_IN);
        gpio_pull_down(row);            // idle LOW, pressed key reads HIGH

        uint c = KEYPAD_COL_FIRST_PIN + i;
        gpio_init(c);
        gpio_set_dir(c, GPIO_OUT);
        gpio_put(c, 0);
    }
}

// Drive one column HIGH at a time and see which row goes HIGH.
static char scan(void) {
    for (int c = 0; c < 4; c++) {
        gpio_put(KEYPAD_COL_FIRST_PIN + c, 1);
        sleep_us(30);                   // let the line settle
        for (int r = 0; r < 4; r++) {
            if (gpio_get(KEYPAD_ROW_FIRST_PIN + r)) {
                gpio_put(KEYPAD_COL_FIRST_PIN + c, 0);
                return keymap[c][r];
            }
        }
        gpio_put(KEYPAD_COL_FIRST_PIN + c, 0);
    }
    return 0;
}

char keypad_get_key(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    char raw = scan();

    if (raw != last_raw) {              // reading changed: restart debounce timer
        last_raw = raw;
        last_change_ms = now;
    }
    if (raw != stable && (now - last_change_ms) >= DEBOUNCE_MS) {
        stable = raw;
        if (stable) return stable;      // report presses only, not releases
    }
    return 0;
}