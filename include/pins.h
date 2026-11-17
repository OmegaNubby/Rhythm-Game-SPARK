// pins.h - the ONLY file that contains GPIO numbers.
// Target: RP2350B (Proton dev board; same numbers on our custom PCB).
//
// Peripheral buses at a glance:
//   SPI0 : TFT                 (GP18 SCK, GP19 MOSI, GP21 CSn)
//   SPI1 : microSD             (GP28 MISO, GP30 SCK, GP31 MOSI)
//   I2C1 : character LCD       (GP26 SDA, GP27 SCL)
//   ADC  : 4 FSRs              (GP40-43 = ADC0-3)
//   PWM  : audio               (GP44)
//   7-seg: software bit-bang   (GP13-15) - NOT hardware SPI1, because the
//          SD card already uses SPI1 on other pins.

#ifndef PINS_H
#define PINS_H

// ---------------- Keypad (4x4 matrix) ---------------------
#define PIN_KEYPAD_FIRST      2     // GP2..GP9 (rows then cols; see keypad driver)
#define PIN_KEYPAD_LAST       9

// ---------------- 7-segment display (2x 74HC595) ----------
// Bit-banged. Latch is the 595 RCLK pin; pulse it high after shifting 16 bits.
#define PIN_SEG_LATCH         13    // RCLK  (the "CSn" pin)
#define PIN_SEG_CLK           14    // SRCLK
#define PIN_SEG_DATA          15    // SER

// ---------------- Character LCD, AIP31068L (I2C1) ---------
#define LCD_I2C               i2c1
#define PIN_LCD_SDA           26
#define PIN_LCD_SCL           27
#define PIN_LCD_AUX           25    // TODO: backlight or reset? confirm and rename
#define LCD_I2C_ADDR          0x3E  // TODO: confirm with an I2C scan

// ---------------- SPI0: TFT -------------------------------
#define SPI_TFT               spi0
#define PIN_TFT_SCK           18
#define PIN_TFT_MOSI          19    // TX; TFT MISO is not wired
#define PIN_TFT_CS            21    // SPI0 CSn pin, driven as plain GPIO by the driver
#define PIN_TFT_DC            20
#define PIN_TFT_RST           22
// TODO: second TFT? It can share SCK/MOSI/DC/RST and just needs its own CS,
//       e.g. #define PIN_TFT2_CS 17

// ---------------- SPI1: microSD ---------------------------
#define SPI_SD                spi1
#define PIN_SD_MISO           28
#define PIN_SD_CS             29
#define PIN_SD_SCK            30
#define PIN_SD_MOSI           31

// ---------------- Audio (PWM -> RC filter -> jack) --------
#define PIN_AUDIO_PWM         44

// ---------------- FSR tiles (ADC) -------------------------
// RP2350B: ADC inputs are GP40..GP47 = ADC channels 0..7.
// Firmware should use channel numbers; GPIO only needed for init.
#define NUM_LANES             4
#define PIN_FSR_BASE          40    // lanes 0..3 = GP40..GP43
#define FSR_ADC_CHANNEL(lane) (lane)  // ADC ch = GPIO - 40

// ---------------- Debug UART ------------------------------
#define PIN_UART_TX           0
#define PIN_UART_RX           1

#endif // PINS_H