#ifndef SPI_SD_RP2040
#define SPI_SD_RP2040
#include <stdint.h>

//temporary public fxn (for test purposes)
uint8_t send_byte(uint8_t byte);

void init_sd_spi();
void sd_select();
void sd_deselect();

#endif