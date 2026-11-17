// pins.h - the ONLY file that contains GPIO numbers.
// Target: RP2350B (Proton dev board and our custom PCB share this map).
// TODO(P1): verify against the Proton schematic that none of these pins
//           are used by onboard parts (buttons, debugger UART, etc.).

#ifndef PINS_H
#define PINS_H

// ---------------- Keypad (4x4 matrix, P1) ----------------
#define PIN_KEYPAD_FIRST      2     // GP2..GP9 (rows then cols; see keypad driver)
#define PIN_KEYPAD_LAST       9

// ---------------- 7-segment display (2x 74HC595, P1) ------
#define PIN_SEG_DATA          17
#define PIN_SEG_CLK           18
#define PIN_SEG_LATCH         19

// ---------------- Character LCD, AIP31068L (I2C1, P1) -----
#define LCD_I2C               i2c1
#define PIN_LCD_SDA           30
#define PIN_LCD_SCL           31
#define LCD_I2C_ADDR          0x3E  // TODO(P1): confirm with an I2C scan

// ---------------- SPI0: TFT1 + microSD (shared bus) -------
#define SPI_BUS0              spi0
#define PIN_SPI0_SCK          34
#define PIN_SPI0_MOSI         35
#define PIN_SPI0_MISO         32    // SD card only; TFT MISO not wired
#define PIN_SD_CS             33
#define PIN_TFT1_CS           20
#define PIN_TFT1_DC           21

// ---------------- SPI1: TFT2 ------------------------------
#define SPI_BUS1              spi1
#define PIN_SPI1_SCK          10
#define PIN_SPI1_MOSI         11
#define PIN_TFT2_CS           13
#define PIN_TFT2_DC           12

#define PIN_TFT_RST           22    // shared reset for both TFTs

// ---------------- Audio (PWM -> RC filter -> jack, P4) ----
#define PIN_AUDIO_PWM         36

// ---------------- FSR tiles (ADC, P2) ---------------------
// RP2350B: ADC inputs are GP40..GP47 = ADC channels 0..7.
// Firmware should use channel numbers; GPIO only needed for init.
#define NUM_LANES             4
#define PIN_FSR_BASE          40    // lanes 0..3 = GP40..GP43
#define FSR_ADC_CHANNEL(lane) (lane)  // ADC ch = GPIO - 40

// ---------------- Debug UART ------------------------------
#define PIN_UART_TX           0
#define PIN_UART_RX           1

#endif // PINS_H