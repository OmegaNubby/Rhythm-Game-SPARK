#include "lcd.h"
#include "pins.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdio.h>
#include <stdbool.h>

// Set to 0 once the LCD works to skip the boot-time bus diagnostics.
#define LCD_DEBUG_BUS 1

// AIP31068L command definitions
#define LCD_CLEARDISPLAY   0x01
#define LCD_RETURNHOME     0x02
#define LCD_ENTRYMODESET   0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_FUNCTIONSET    0x20
#define LCD_SETDDRAMADDR   0x80

// Display control flags
#define LCD_DISPLAYON      0x04
#define LCD_CURSOROFF      0x00
#define LCD_BLINKOFF       0x00

// Function set flags (8-bit interface, 2 lines, 5x8 dots)
#define LCD_FUNCTION_2LINE 0x08
#define LCD_FUNCTION_DL8   0x10

// I2C control byte: Co = 0, RS = 0 for command, RS = 1 for data.
// With Co = 0, every byte after the control byte is treated as that type,
// so a whole string can go out in ONE transaction.
#define CONTROL_BYTE_CMD   0x00
#define CONTROL_BYTE_DATA  0x40

// Stay at 100 kHz: the controller needs ~30-40 us per character.
#define LCD_I2C_HZ         (100 * 1000)
#define LCD_I2C_TIMEOUT_US 10000

static bool lcd_present = false;   // set by lcd_init() once the LCD ACKs
static bool warned = false;

// ---------------------------------------------------------------------------
// Low-level I2C
// ---------------------------------------------------------------------------

// Timeout so a missing or stuck LCD can never hang the whole program.
// Silently does nothing if the LCD was not found at init.
static void lcd_i2c_write(const uint8_t *buf, size_t len) {
    if (!lcd_present) return;
    int r = i2c_write_timeout_us(LCD_I2C, LCD_I2C_ADDR, buf, len, false, LCD_I2C_TIMEOUT_US);
    if (r < 0 && !warned) {
        printf("lcd: I2C write to 0x%02X failed (%d): no ACK or bus stuck\n",
               LCD_I2C_ADDR, r);
        warned = true;
    }
}

static void lcd_send(uint8_t control_byte, uint8_t value) {
    uint8_t buf[2] = { control_byte, value };
    lcd_i2c_write(buf, 2);
    sleep_us(50);   // small delay for command processing
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Both lines should read 1 with pull-ups on and nothing talking.
// A 0 means a short, a swapped wire, or a device holding the line low.
static void lcd_bus_idle_check(void) {
    gpio_init(PIN_LCD_SDA); gpio_set_dir(PIN_LCD_SDA, GPIO_IN); gpio_pull_up(PIN_LCD_SDA);
    gpio_init(PIN_LCD_SCL); gpio_set_dir(PIN_LCD_SCL, GPIO_IN); gpio_pull_up(PIN_LCD_SCL);
    sleep_ms(1);
    printf("lcd: idle SDA=%d SCL=%d (both should be 1)\n",
           gpio_get(PIN_LCD_SDA), gpio_get(PIN_LCD_SCL));
}

static void i2c_scan(void) {
    uint8_t dummy = 0;
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

    sleep_ms(50);                   // let the LCD controller power up

#if LCD_DEBUG_BUS
    i2c_scan();
#endif

    // Probe the LCD itself. If nothing answers, leave lcd_present false so
    // every later call becomes a harmless no-op instead of spamming errors.
    uint8_t probe = CONTROL_BYTE_CMD;
    int r = i2c_write_timeout_us(LCD_I2C, LCD_I2C_ADDR, &probe, 1, false, LCD_I2C_TIMEOUT_US);
    if (r < 0) {
        printf("lcd: no ACK from 0x%02X (%d), LCD disabled\n", LCD_I2C_ADDR, r);
        return;
    }
    lcd_present = true;

    // Function set is sent three times with short waits, as many
    // HD44780-style controllers expect.
    lcd_send(CONTROL_BYTE_CMD, LCD_FUNCTIONSET | LCD_FUNCTION_DL8 | LCD_FUNCTION_2LINE);
    sleep_ms(5);
    lcd_send(CONTROL_BYTE_CMD, LCD_FUNCTIONSET | LCD_FUNCTION_DL8 | LCD_FUNCTION_2LINE);
    sleep_us(150);
    lcd_send(CONTROL_BYTE_CMD, LCD_FUNCTIONSET | LCD_FUNCTION_DL8 | LCD_FUNCTION_2LINE);

    // Display ON, cursor OFF, blink OFF
    lcd_send(CONTROL_BYTE_CMD, LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF);

    lcd_clear();

    // Entry mode: increment cursor, no display shift
    lcd_send(CONTROL_BYTE_CMD, LCD_ENTRYMODESET | 0x02);

    printf("lcd: init done\n");
}

void lcd_clear(void) {
    lcd_send(CONTROL_BYTE_CMD, LCD_CLEARDISPLAY);
    sleep_ms(2);    // clear needs a longer delay
}

void lcd_set_cursor(uint8_t row, uint8_t col) {
    // Row offsets for a 16x2 LCD: 0x00 for row 0, 0x40 for row 1
    static const uint8_t row_offsets[] = { 0x00, 0x40 };
    if (row > 1)  row = 1;
    if (col > 15) col = 15;
    lcd_send(CONTROL_BYTE_CMD, LCD_SETDDRAMADDR | (row_offsets[row] + col));
}

void lcd_write_char(char c) {
    lcd_send(CONTROL_BYTE_DATA, (uint8_t)c);
}

void lcd_write_string(const char *str) {
    // One I2C transaction per string (up to 32 chars) instead of one per
    // character: a 16-char row takes about 1.6 ms instead of about 5 ms.
    uint8_t buf[1 + 32];
    size_t n = 0;
    buf[n++] = CONTROL_BYTE_DATA;
    while (*str && n < sizeof(buf)) {
        buf[n++] = (uint8_t)*str++;
    }
    if (n > 1) {
        lcd_i2c_write(buf, n);
        sleep_us(50);
    }
}