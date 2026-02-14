#include "ff.h"
#include "diskio.h"
#include <stdbool.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/time.h"

// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19



/* MMC/SD command */
#define CMD0	(0)			/* GO_IDLE_STATE */
#define CMD1	(1)			/* SEND_OP_COND (MMC) */
#define	ACMD41	(0x80+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(8)			/* SEND_IF_COND */
#define CMD9	(9)			/* SEND_CSD */
#define CMD10	(10)		/* SEND_CID */
#define CMD12	(12)		/* STOP_TRANSMISSION */
#define ACMD13	(0x80+13)	/* SD_STATUS (SDC) */
#define CMD16	(16)		/* SET_BLOCKLEN */
#define CMD17	(17)		/* READ_SINGLE_BLOCK */
#define CMD18	(18)		/* READ_MULTIPLE_BLOCK */
#define CMD23	(23)		/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(0x80+23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(24)		/* WRITE_BLOCK */
#define CMD25	(25)		/* WRITE_MULTIPLE_BLOCK */
#define CMD32	(32)		/* ERASE_ER_BLK_START */
#define CMD33	(33)		/* ERASE_ER_BLK_END */
#define CMD38	(38)		/* ERASE */
#define CMD55	(55)		/* APP_CMD */
#define CMD58	(58)		/* READ_OCR */


#define slow_baud 400*1000


static volatile DSTATUS Stat = STA_NOINIT;	/* Physical drive status */
static volatile UINT Timer1, Timer2;		/* 1kHz decrement timer stopped at zero (disk_timerproc()) */

static BYTE CardType;	/* Card type flags */



/*-----------------------------------------------------------------------*/
/* SPI controls for RP2040                                               */
/*-----------------------------------------------------------------------*/

void init_sd_spi(void)
{
  // SPI initialisation. This example will use SPI at 1MHz.
  spi_init(SPI_PORT, slow_baud);
  gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
  gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
  gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
  gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
  
  // Chip select is active-low, so we'll initialise it to a driven-high state
  gpio_set_dir(PIN_CS, GPIO_OUT);
  gpio_put(PIN_CS, 1);
  
  sleep_ms(10);

}

/*Send a byte*/
//this function will be used in any scenario that requires a byte long transfer
//it can also be used to send garbage over MOSI to cycle the clock for the sd card 
uint8_t send_byte(uint8_t byte){
  
  uint8_t res = 0;

  spi_write_read_blocking(SPI_PORT, &byte, &res, 1);

  return res;
}

/*Select SD Card*/

void sd_select(){
  gpio_put(PIN_CS, 0);
}

void sd_deselect(){
  gpio_put(PIN_CS, 1);
}

