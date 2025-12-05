#include "hw_config.h"
#include "sd_card.h"
#include "my_spi.h"

// SPI Configuration
static spi_t spi = {
    .hw_inst = spi0,
    .miso_gpio = 4,        // Change to your actual MISO pin
    .mosi_gpio = 3,        // Change to your actual MOSI pin
    .sck_gpio = 2,         // Change to your actual SCK pin
    .baud_rate = 12 * 1000 * 1000,  // 12 MHz
    .spi_mode = 0,         // SPI Mode 0
    .no_miso_gpio_pull_up = false,
    .set_drive_strength = false,
    .use_static_dma_channels = false
};

// SPI Interface for SD Card
static sd_spi_if_t spi_if = {
    .spi = &spi,
    .ss_gpio = 5,          // Chip Select - Change to your actual CS pin
    .set_drive_strength = false
};

// SD Card Configuration
static sd_card_t sd_card = {
    .type = SD_IF_SPI,
    .spi_if_p = &spi_if,
    .use_card_detect = false,
    .card_detect_gpio = 0,
    .card_detected_true = 0,
    .card_detect_use_pull = false,
    .card_detect_pull_hi = false
};

// Required by the library
size_t sd_get_num() { 
    return 1;
}

sd_card_t *sd_get_by_num(size_t num) {
    if (num == 0) {
        return &sd_card;
    }
    return NULL;
}
