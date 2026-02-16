#ifndef SPI_SD_RP2040
#define SPI_SD_RP2040
#include <stdint.h>
#include "ff.h" //remove this after testing
#include "diskio.h"

//temporary public fxn (for test purposes)
uint8_t send_byte(uint8_t byte);

void init_sd_spi();
void sd_select();
void sd_deselect();
uint8_t send_cmd(uint8_t cmd, uint32_t arg);

DSTATUS disk_initialize (BYTE pdrv);

#endif