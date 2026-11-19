#include "lcd.h"
#include "pins.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdio.h>

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

// Function set flags (2 line, 5x8 dots)
#define LCD_FUNCTION_2LINE 0x08
#define LCD_FUNCTION_DL8   0x10 // 8-bit mode for AIP31068L I2C

// I2C control byte for AIP31068L (Co = 0, RS = 0 for command, RS = 1 for data)
#define CONTROL_BYTE_CMD   0x00
#define CONTROL_BYTE_DATA  0x40

#define LCD_I2C_TIMEOUT_US 5000

static bool lcd_err_reported = false;   // only log the first failure (no serial flood)
static bool lcd_available = true;     // skip further writes if I2C NACKs/times out

static void lcd_scan_bus(void) {
    printf("LCD: scanning I2C bus...\n");
    int found = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        uint8_t dummy = 0;
        int ret = i2c_write_timeout_us(LCD_I2C, addr, &dummy, 1, false, 1000);
        if (ret >= 0) {
            printf("LCD: found device at 0x%02X\n", addr);
            found++;
        }
    }
    if (found == 0) {
        printf("LCD: no I2C devices found on bus\n");
    }
}

static void lcd_send(uint8_t control_byte, uint8_t value) {
    if (!lcd_available) {
        return;
    }

    uint8_t buf[2];
    buf[0] = control_byte;
    buf[1] = value;

    // Timeout version: a missing or miswired LCD can no longer freeze the program.
    int r = i2c_write_timeout_us(LCD_I2C, LCD_I2C_ADDR, buf, 2, false, LCD_I2C_TIMEOUT_US);
    if (r != 2) {
        lcd_available = false;
        if (!lcd_err_reported) {
            lcd_err_reported = true;
            printf("LCD: I2C write FAILED (code %d) to addr 0x%02X - check wiring, address, pins.h\n",
                   r, LCD_I2C_ADDR);
        }
        return;
    }
    sleep_us(50); // Small delay for command processing
}

void lcd_init(void) {
    lcd_err_reported = false;
    lcd_available = true;

    printf("LCD: init, I2C addr 0x%02X, SDA=%d SCL=%d\n",
           LCD_I2C_ADDR, (int)PIN_LCD_SDA, (int)PIN_LCD_SCL);

#ifdef PIN_LCD_AUX
    gpio_init(PIN_LCD_AUX);
    gpio_set_dir(PIN_LCD_AUX, GPIO_OUT);
    gpio_put(PIN_LCD_AUX, 1);
#endif

    // Initialize I2C pins according to pins.h
    i2c_init(LCD_I2C, 100 * 1000); // 100 kHz standard speed
    gpio_set_function(PIN_LCD_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_LCD_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_LCD_SDA);
    gpio_pull_up(PIN_LCD_SCL);

    lcd_scan_bus();

    // Give the LCD controller time to power up
    sleep_ms(50);

    // Initialize AIP31068L in 8-bit interface mode over I2C
    lcd_send(CONTROL_BYTE_CMD, LCD_FUNCTIONSET | LCD_FUNCTION_DL8 | LCD_FUNCTION_2LINE);
    sleep_ms(5);

    // Display control: Display ON, Cursor OFF, Blink OFF
    lcd_send(CONTROL_BYTE_CMD, LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF);

    // Clear display
    lcd_clear();

    // Entry mode set: Increment cursor, no display shift
    lcd_send(CONTROL_BYTE_CMD, LCD_ENTRYMODESET | 0x02);

    printf("LCD: init finished%s\n", lcd_err_reported ? " (with I2C errors)" : " (no I2C errors)");
}

void lcd_clear(void) {
    lcd_send(CONTROL_BYTE_CMD, LCD_CLEARDISPLAY);
    sleep_ms(2); // Clear command requires a longer delay
}

void lcd_set_cursor(uint8_t row, uint8_t col) {
    // Row offsets for standard 16x2 character LCDs (0x00 for row 0, 0x40 for row 1)
    uint8_t row_offsets[] = {0x00, 0x40};
    if (row > 1) row = 1;
    lcd_send(CONTROL_BYTE_CMD, LCD_SETDDRAMADDR | (row_offsets[row] + col));
}

void lcd_write_char(char c) {
    lcd_send(CONTROL_BYTE_DATA, (uint8_t)c);
}

void lcd_write_string(const char *str) {
    while (*str) {
        lcd_write_char(*str++);
    }
}