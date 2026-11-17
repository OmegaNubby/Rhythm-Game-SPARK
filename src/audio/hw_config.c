#include "hw_config.c"

static spi_t spis[] = {{
    .hw_inst   = spi0,
    .sck_gpio  = 18,
    .mosi_gpio = 19,
    .miso_gpio = 16,
    .baud_rate = 12500 * 1000      // 12.5 MHz; drop to 1 MHz if it won't mount
}};

static sd_spi_if_t spi_if = {
    .spi = &spis[0],
    .ss_gpio = 20                  // CS
};

static sd_card_t sd_cards[] = {{
    .type = SD_IF_SPI,
    .spi_if_p = &spi_if
}};

size_t sd_get_num(void) { return count_of(sd_cards); }
sd_card_t *sd_get_by_num(size_t n) { return (n < sd_get_num()) ? &sd_cards[n] : NULL; }
size_t spi_get_num(void) { return count_of(spis); }
spi_t *spi_get_by_num(size_t n) { return (n < spi_get_num()) ? &spis[n] : NULL; }