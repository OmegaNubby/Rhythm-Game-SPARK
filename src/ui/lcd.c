#include "lcd.h"
#include "pins.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"

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

static void lcd_send(uint8_t control_byte, uint8_t value) {
    uint8_t buf[2];
    buf[0] = control_byte;
    buf[1] = value;
    i2c_write_blocking(LCD_I2C, LCD_I2C_ADDR, buf, 2, false);
    sleep_us(50); // Small delay for command processing
}

void lcd_init(void) {
    // Initialize I2C1 pins according to pins.h
    i2c_init(LCD_I2C, 100 * 1000); // 100 kHz standard speed
    gpio_set_function(PIN_LCD_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_LCD_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_LCD_SDA);
    gpio_pull_up(PIN_LCD_SCL);

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