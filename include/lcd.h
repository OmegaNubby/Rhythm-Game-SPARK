#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include <stdbool.h>

void lcd_init(void);
void lcd_clear(void);
void lcd_set_cursor(uint8_t row, uint8_t col);
void lcd_write_string(const char *str);
void lcd_write_char(char c);

#endif // LCD_H