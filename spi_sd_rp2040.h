#ifndef SPI_SD_RP2040
#define SPI_SD_RP2040
#include <stdint.h>
#include "ff.h" //remove this after testing
#include "diskio.h"

//public fxns
void timer_proc();


//temporary public fxn (for test purposes)
uint8_t send_byte(uint8_t byte);

void init_sd_spi();
void sd_select();
void sd_deselect();
uint8_t send_cmd(uint8_t cmd, uint32_t arg);

DSTATUS disk_status (BYTE pdrv);
DSTATUS disk_initialize (BYTE pdrv);
DRESULT disk_read (
  BYTE pdrv,     /* [IN] Physical drive number */
  BYTE* buff,    /* [OUT] Pointer to the read data buffer */
  LBA_t sector,  /* [IN] Start sector number */
  UINT count     /* [IN] Number of sectros to read */
);

#endif