// lcd.c - 16x2 HD44780 character LCD behind a PCF8574 I2C backpack
// (the common "LCM1602" board, address 0x27, or 0x3F for the PCF8574A).
//
// The PCF8574 is just an 8-bit I/O expander. Each byte we send sets its pins:
//   P0 = RS   P1 = RW (kept 0)   P2 = E   P3 = backlight   P4..P7 = D4..D7
// The LCD runs in 4-bit mode, so every character/command takes two nibbles,
// and every nibble needs an E pulse (E high, then E low).
//
// Public API is unchanged: lcd_init, lcd_clear, lcd_set_cursor,
// lcd_write_char, lcd_write_string.

#include "lcd.h"
#include "pins.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdio.h>
#include <stdbool.h>

// Set to 0 once the LCD works to skip the boot-time bus diagnostics.
#define LCD_DEBUG_BUS 1

// Backpack pin mapping. Matches nearly every PCF8574 LCD backpack; if yours
// shows garbage but acknowledges its address, check these against its schematic.
#define BP_RS 0x01
#define BP_RW 0x02
#define BP_EN 0x04
#define BP_BL 0x08

// HD44780 commands
#define LCD_CLEARDISPLAY   0x01
#define LCD_ENTRYMODESET   0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_FUNCTIONSET    0x20
#define LCD_SETDDRAMADDR   0x80

#define LCD_I2C_HZ         (100 * 1000)
#define LCD_I2C_TIMEOUT_US 10000

// Tried in order. LCD_I2C_ADDR from pins.h goes first; the others are the two
// usual backpack addresses, so a wrong pins.h value still gets found.
static const uint8_t addr_candidates[] = { LCD_I2C_ADDR, 0x27, 0x3F };

static uint8_t lcd_addr    = LCD_I2C_ADDR;
static bool    lcd_present = false;
static bool    warned      = false;
static uint8_t backlight   = BP_BL;

// ---------------------------------------------------------------------------
// Low-level I2C
// ---------------------------------------------------------------------------

// Timeout so a missing or stuck LCD can never hang the program.
static void lcd_i2c_write(const uint8_t *buf, size_t len) {
    if (!lcd_present) return;
    int r = i2c_write_timeout_us(LCD_I2C, lcd_addr, buf, len, false, LCD_I2C_TIMEOUT_US);
    if (r < 0 && !warned) {
        printf("lcd: I2C write to 0x%02X failed (%d)\n", lcd_addr, r);
        warned = true;
    }
}

// One nibble: put it on D4..D7, pulse E.
static void lcd_nibble(uint8_t nib, uint8_t rs) {
    uint8_t d = (uint8_t)((nib << 4) | backlight | rs);
    uint8_t buf[2] = { (uint8_t)(d | BP_EN), d };
    lcd_i2c_write(buf, 2);
}

// One full byte as two nibbles, in a single I2C transaction.
static void lcd_byte(uint8_t value, uint8_t rs) {
    uint8_t hi = (uint8_t)((value & 0xF0) | backlight | rs);
    uint8_t lo = (uint8_t)(((value << 4) & 0xF0) | backlight | rs);
    uint8_t buf[4] = { (uint8_t)(hi | BP_EN), hi, (uint8_t)(lo | BP_EN), lo };
    lcd_i2c_write(buf, 4);
}

static void lcd_cmd(uint8_t c) {
    lcd_byte(c, 0);
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Both lines should read 1 with pull-ups on and nothing talking.
static void lcd_bus_idle_check(void) {
    gpio_init(PIN_LCD_SDA); gpio_set_dir(PIN_LCD_SDA, GPIO_IN); gpio_pull_up(PIN_LCD_SDA);
    gpio_init(PIN_LCD_SCL); gpio_set_dir(PIN_LCD_SCL, GPIO_IN); gpio_pull_up(PIN_LCD_SCL);
    sleep_ms(1);
    printf("lcd: idle SDA=%d SCL=%d (both should be 1)\n",
           gpio_get(PIN_LCD_SDA), gpio_get(PIN_LCD_SCL));
}

static void i2c_scan(void) {
    uint8_t dummy = BP_BL;          // backlight bit on, so the scan doesn't blink it
    int found = 0;
    printf("lcd: I2C scan on SDA=%d SCL=%d\n", PIN_LCD_SDA, PIN_LCD_SCL);
    for (uint8_t a = 0x08; a < 0x78; a++) {
        if (i2c_write_timeout_us(LCD_I2C, a, &dummy, 1, false, LCD_I2C_TIMEOUT_US) >= 0) {
            printf("lcd:   ACK at 0x%02X\n", a);
            found++;
        }
    }
    if (!found) printf("lcd:   no devices found\n");
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void lcd_init(void) {
#if LCD_DEBUG_BUS
    lcd_bus_idle_check();           // runs before the I2C block takes over the pins
#endif

    i2c_init(LCD_I2C, LCD_I2C_HZ);
    gpio_set_function(PIN_LCD_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_LCD_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_LCD_SDA);
    gpio_pull_up(PIN_LCD_SCL);

    sleep_ms(50);                   // HD44780 needs >40 ms after power-up

#if LCD_DEBUG_BUS
    i2c_scan();
#endif

    // Find the backpack. Only mark the LCD present once something ACKs.
    uint8_t probe = backlight;
    lcd_present = false;
    for (unsigned i = 0; i < sizeof addr_candidates; i++) {
        int r = i2c_write_timeout_us(LCD_I2C, addr_candidates[i], &probe, 1, false,
                                     LCD_I2C_TIMEOUT_US);
        if (r >= 0) {
            lcd_addr = addr_candidates[i];
            lcd_present = true;
            break;
        }
    }
    if (!lcd_present) {
        printf("lcd: nothing ACKed at 0x%02X, 0x27 or 0x3F: LCD disabled "
               "(check power, wiring, pull-ups)\n", LCD_I2C_ADDR);
        return;
    }
    if (lcd_addr != LCD_I2C_ADDR)
        printf("lcd: found at 0x%02X (pins.h says 0x%02X, update it)\n",
               lcd_addr, LCD_I2C_ADDR);

    // HD44780 reset-by-instruction: force 8-bit mode three times, then
    // switch to 4-bit mode.
    lcd_nibble(0x03, 0);
    sleep_ms(5);
    lcd_nibble(0x03, 0);
    sleep_ms(1);
    lcd_nibble(0x03, 0);
    sleep_ms(1);
    lcd_nibble(0x02, 0);            // now in 4-bit mode
    sleep_ms(1);

    lcd_cmd(LCD_FUNCTIONSET | 0x08);                    // 4-bit, 2 lines, 5x8 font
    lcd_cmd(LCD_DISPLAYCONTROL);                        // display off
    lcd_clear();
    lcd_cmd(LCD_ENTRYMODESET | 0x02);                   // increment, no shift
    lcd_cmd(LCD_DISPLAYCONTROL | 0x04);                 // display on, no cursor

    printf("lcd: init done (backpack at 0x%02X)\n", lcd_addr);
}

void lcd_clear(void) {
    lcd_cmd(LCD_CLEARDISPLAY);
    sleep_ms(2);                    // clear needs about 1.6 ms
}

void lcd_set_cursor(uint8_t row, uint8_t col) {
    static const uint8_t row_offsets[] = { 0x00, 0x40 };
    if (row > 1)  row = 1;
    if (col > 15) col = 15;
    lcd_cmd(LCD_SETDDRAMADDR | (row_offsets[row] + col));
}

void lcd_write_char(char c) {
    lcd_byte((uint8_t)c, BP_RS);
}

void lcd_write_string(const char *str) {
    // Up to 32 chars in one I2C transaction (4 bytes per character).
    uint8_t buf[4 * 32];
    size_t n = 0;
    while (*str && n + 4 <= sizeof(buf)) {
        uint8_t v  = (uint8_t)*str++;
        uint8_t hi = (uint8_t)((v & 0xF0) | backlight | BP_RS);
        uint8_t lo = (uint8_t)(((v << 4) & 0xF0) | backlight | BP_RS);
        buf[n++] = hi | BP_EN;
        buf[n++] = hi;
        buf[n++] = lo | BP_EN;
        buf[n++] = lo;
    }
    if (n) lcd_i2c_write(buf, n);
}