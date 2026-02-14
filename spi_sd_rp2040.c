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
  //pull cs low to activate
  gpio_put(PIN_CS, 0);
  //send dummy byte to give sd card clock cycles for processing
  send_byte(0xFF); 
}

void sd_deselect(){
  //pull cs high to deactivate
  gpio_put(PIN_CS, 1);
  //send dummy byte to give sd card clock cycles for processing
  send_byte(0xFF);
}



/*Send a Command*/
/*Each "Command" is 48 bits with a breakdown as follows
  Start bits [47-46]: '01'
  Command number [45-40]: A 6 bit command number to indicate the meaning of following data
  Argument [39-8]: Any data needed to execute indicated command (a lot of the time, this data is NULL)
  Cyclic Redundancy Check (CRC) [7-1]: Used by the SD to verify the integrity of a command it recieves (often ignored by the SD)
  Stop bit [0]: Indicates the end of transmission 

*/

uint8_t send_cmd(uint8_t cmd, uint32_t arg){
  //deselect and select to cleanly end any previous actions
  sd_deselect();
  sd_select();  
  //Send start bits and cmd
  send_byte(0x40 | cmd);
  //send argument in 8 bit increments by shifting the desired part
  //to the first 8 bits and "extracting" the first 8 bits
  send_byte((uint8_t)((arg >> 24) & 0xFF));
  send_byte((uint8_t)((arg >> 16) & 0xFF));
  send_byte((uint8_t)((arg >> 8) & 0xFF));
  send_byte((uint8_t)((arg >> 0) & 0xFF));

  //Send 8 bit CRC if command requires it (only required for SPI init w/ CMD0 and CMD8) 
  //Otherwise send garbage and the stop bit
  if(cmd == CMD0){
    send_byte(0x95);
  }else if(cmd == CMD8){
    send_byte(0x87);
  }else{
    send_byte(0xFF);
  }

  //toggle the clock and wait for a command response
  //requires the MOSI line to be high
  //will exit on error if not gotten in 10 tries
  int n = 10;
  uint8_t res = send_byte(0xFF);
  n--;
  while (res == 0xFF && n > 0){ //a response will have a leading 0 
    res = send_byte(0xFF);
    n--;
  } 
  return res;
}

