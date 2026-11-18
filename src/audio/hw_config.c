#include "hw_config.h"

static spi_t spis[] = {{
    .hw_inst   = spi1,
    .miso_gpio = 28,
    .mosi_gpio = 31,
    .sck_gpio  = 30,
    .baud_rate = 12500 * 1000      // drop to 1 MHz if it won't mount
}};

static sd_card_t sd_cards[] = {{
    .pcName = "0:",
    .spi = &spis[0],
    .ss_gpio = 29,                 // CS
    .use_card_detect = false
}};

size_t sd_get_num(void) { return count_of(sd_cards); }
sd_card_t *sd_get_by_num(size_t num) { return (num < sd_get_num()) ? &sd_cards[num] : NULL; }
size_t spi_get_num(void) { return count_of(spis); }
spi_t *spi_get_by_num(size_t num) { return (num < spi_get_num()) ? &spis[num] : NULL; }