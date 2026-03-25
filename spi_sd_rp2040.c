#include "ff.h"
#include "diskio.h"
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/aon_timer.h"

// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
// #define SPI_PORT spi0
// #define PIN_MISO 16
// #define PIN_CS   17
// #define PIN_SCK  18
// #define PIN_MOSI 19

#define SPI_PORT spi0
// #define SD_DET_PIN 1
#define PIN_SCK 2
#define PIN_MOSI 3
#define PIN_MISO 4
#define PIN_CS  5

/* MMC/SD command */
#define CMD0	(0)			/* GO_IDLE_STATE */
#define CMD1	(1)			/* SEND_OP_COND (MMC) */
#define	ACMD41 (41)	/* SEND_OP_COND (SDC) */
#define CMD8	(8)			/* SEND_IF_COND */
#define CMD9	(9)			/* SEND_CSD */
#define CMD10	(10)		/* SEND_CID */
#define CMD12	(12)		/* STOP_TRANSMISSION */
#define ACMD13	(13)	/* SD_STATUS (SDC) */
#define CMD16	(16)		/* SET_BLOCKLEN */
#define CMD17	(17)		/* READ_SINGLE_BLOCK */
#define CMD18	(18)		/* READ_MULTIPLE_BLOCK */
#define CMD23	(23)		/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(24)		/* WRITE_BLOCK */
#define CMD25	(25)		/* WRITE_MULTIPLE_BLOCK */
#define CMD32	(32)		/* ERASE_ER_BLK_START */
#define CMD33	(33)		/* ERASE_ER_BLK_END */
#define CMD38	(38)		/* ERASE */
#define CMD55	(55)		/* APP_CMD */
#define CMD58	(58)		/* READ_OCR */


#define slow_baud 400*1000


static volatile DSTATUS SD_Status = STA_NOINIT;	/* Physical drive status */
static volatile UINT Timer1, Timer2;		/* 1kHz decrement timer stopped at zero (disk_timerproc()) */
static BYTE CardType;	/* Card type flags */

static inline uint32_t get_ms(void){

    return to_ms_since_boot(get_absolute_time());

}

/*-----------------------------------------------------------------------*/
/* SPI controls for RP2040                                               */
/*-----------------------------------------------------------------------*/

bool init_sd_spi()
{
  //this struct will set aon timer to jan 1st 1900 00:00:00, as the default time if calendar_time is null
  struct tm calendar_time_def = {
    .tm_sec = 0,
    .tm_min = 0,
    .tm_hour = 0,
    .tm_mday = 1,
    .tm_mon = 0,
    .tm_year = 0,
    .tm_wday = 0,
    .tm_yday = 0,
    .tm_isdst = 0
  };

  // SPI initialisation. This example will use SPI at 1MHz.
  spi_init(SPI_PORT, slow_baud);
  gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
  gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
  gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
  gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
  //internal pull up
  gpio_pull_up(PIN_MISO);
  
  // Chip select is active-low, so we'll initialise it to a driven-high state
  gpio_set_dir(PIN_CS, GPIO_OUT);
  gpio_put(PIN_CS, 1);

  //start aon timer if it isn't started already
  if(!aon_timer_is_running()){
    if(!aon_timer_start_calendar(&calendar_time_def)){
        return false;
    }
    return true;
  }
  
  sleep_ms(10);

}



/*Send a byte*/
//this function will be used in any scenario that requires a byte long transfer
//it can also be used to send garbage over MOSI to cycle the clock for the sd card 
uint8_t send_byte(uint8_t byte){
  
  uint8_t res = 0;

  spi_write_read_blocking(SPI_PORT, &byte, &res, 1);
  while (spi_is_busy(SPI_PORT));
  return res;
}

int wait_sd_ready(uint32_t tm){ // 1 if success and 0 if fail
  
  uint32_t t = get_ms();
  uint8_t res = send_byte(0xFF);
  while(res != 0xFF &&  get_ms() < (t + tm)){
    res = send_byte(0xFF);
  }
  while (spi_is_busy(SPI_PORT));
  return 1;
}

/*Select SD Card*/


void sd_deselect(){
  //pull cs high to deactivate
  gpio_put(PIN_CS, 1);
  //send dummy byte to give sd card clock cycles for processing
  send_byte(0xFF);
  //makes sure final clock cycle finishes before returning
  while (spi_is_busy(SPI_PORT));
}

int sd_select(){
  //pull cs low to activate
  gpio_put(PIN_CS, 0);
  //send dummy byte to give sd card clock cycles for processing
  send_byte(0xFF); 
  //makes sure final clock cycle finishes before returning
  while (spi_is_busy(SPI_PORT));
  if(wait_sd_ready(500)) return 1;

  sd_deselect();
  return 0;
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
  
  //deselect and select to cleanly end any previous actions if CMD12 is not sent
  //we don't want to reassert CS to interrupt a transaction
  if(cmd != CMD12){
    sd_deselect();
    sd_select();
  }
  
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

  // discard garbage byte that comes after sending CMD12
  if (cmd == CMD12) send_byte(0xFF);

  //toggle the clock and wait for a command response
  //requires the MOSI line to be high
  //will exit on error if not gotten in 10 tries
  int n = 10;
  uint8_t res = send_byte(0xFF);
  n--;
  //while tries are left and the leading bit is 1
  //leading 0 indicates a response
  while ((res & 0x80) && n > 0){ //a response will have a leading 0 
    res = send_byte(0xFF);
    n--;
  }
  while (spi_is_busy(SPI_PORT));
  return res;
}



//writes a data packet: see -> https://elm-chan.org/docs/mmc/mmc_e.html
uint8_t write_block( // returns 1 for success and 0 for fail
  const uint8_t *buff,
  uint8_t token
){
  //need to check to see if SD is ready to recieve with included timeout
  if(!wait_sd_ready(500)) return 0;

  send_byte(token);
  //send data if the token was not a stop transmission token
  if(token != 0xFD){
    spi_write_blocking(SPI_PORT, buff, 512); //writes 512 bytes (fixed size standard) 
    //send dummy crc
    send_byte(0xFF);
    send_byte(0xFF);
    //get response byte
    uint8_t resp = send_byte(0xFF);
    if ((resp & 0x1F) != 0x05) return 0;
  }
  
  return 1;
}

//reads a data packed: see -> https://elm-chan.org/docs/mmc/mmc_e.html
uint8_t read_block( // returns 1 for success and 0 for fail
  uint8_t *buff,
  int len
){
  //add har dware timer timeout functionality
	//read until start token squired
  const uint32_t timeout = 200;
  uint32_t t = get_ms();

  uint8_t token = send_byte(0xFF);
  while(token == 0xFF && get_ms() < t+timeout){
    token = send_byte(0xFF);
  }
  if(token != 0xFE) return 0;
  //read bytes
  spi_read_blocking(SPI_PORT, 0xFF, buff, len);
  // get rid of crc
  send_byte(0xFF);
  send_byte(0xFF); 

  return 1;
}



//disk initialize function
DSTATUS disk_initialize (BYTE pdrv){
  // A flowchart for the initialization process: https://www.dejazzer.com/ee379/lecture_notes/lec12_sd_card.pdf
  //will hold data for init CMDS that have an ocr component in their response
  uint8_t ocr[4];
  uint8_t sd_type;
  uint32_t t;
  const uint32_t timeout = 1000; /* Initialization timeout = 1 sec */
  if (pdrv) return STA_NOINIT;
  init_sd_spi();

  if(SD_Status & STA_NODISK) return SD_Status;

  //80 clock cycles to prep 
  for(uint8_t i = 0; i<10; i++){
    send_byte(0xFF);
  }
  //initialize sd_type to be 0
  sd_type = 0; 

  //set number of tries for CMD0 to 3
  uint8_t n = 3;

  //attempt to send the first command n times
  while(send_cmd(CMD0, 0) != 0x01 && n-- > 0);
  //if CMD0 failed, return STA_NOINIT according Chan FatFS
  if(n <= 0){
    SD_Status = STA_NOINIT;
    return SD_Status;
  }

  //try CMD8 and check to see if the response is valid with no illegal flags
  if(send_cmd(CMD8, 0x1AA) == 0x01){
    t = get_ms();
    spi_read_blocking(SPI_PORT, 0xFF, ocr, 4);
    if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
      //the card is of type SDCv2+ in this case	
      //leading command of initialization using ACMD	
      send_cmd(CMD55,0);
      //ACMD command with high capacity bit set 
      uint8_t adcm_res = send_cmd(ACMD41, (1UL << 30));
      while(get_ms() < t + timeout && adcm_res && 0x01){ //while the card is still idle, send CMD55 and ACMD41 again
        send_cmd(CMD55,0);
        adcm_res = send_cmd(ACMD41, (1UL << 30));
      }
      //request to read ocr
      send_cmd(CMD58, 0);
      //read in ocr
      spi_read_blocking(SPI_PORT, 0xFF, ocr, 4);
      //if the high capacity bit is set, we have an SDCv2 card with HC or XC which means we need to address by block of 521 bytes
      //otherwise we have a standard SDCv2
      sd_type = (ocr[0] & 0x40) ? (CT_SDC2 | CT_BLOCK) : CT_SDC2;

    }

  //try  CMD58 and check if CMD58 had an okay response
  }else if(get_ms() < t + timeout && send_cmd(CMD58, 0) == 0x01){
    //card is of type SDCv1
    spi_read_blocking(SPI_PORT, 0xFF, ocr, 4);
    //check if card supports 2.7-3.3v
    //different ocr for SDCv1
    if(ocr[3]){
      //notifies SD of ACMD
      send_cmd(CMD55,0);
      uint8_t adcm_res = send_cmd(ACMD41, (1UL << 30));
      while(get_ms() < t + timeout && adcm_res && 0x01){ //while the card is still idle, send CMD55 and ACMD41 again
        send_cmd(CMD55,0);
        adcm_res = send_cmd(ACMD41, (1UL << 30));
      }
      sd_type = CT_SDC1;
    }
  }

  if(sd_type){ //success
    CardType = sd_type;
    SD_Status &= ~STA_NOINIT; // clear STA_NOINIT flag
  }else{ // Init Failed
    SD_Status = STA_NOINIT;
  }

  return SD_Status;

}


DSTATUS disk_status (BYTE pdrv){ //pdrv should only ever be 0

	if (pdrv) return STA_NOINIT;	// checks if drv is 0

	return SD_Status;	//returns stored status
}

DRESULT disk_write (
  BYTE pdrv,        // Physical drive num -> always 0
  const BYTE* buff, // pointer to a byte buffer of size count
  LBA_t sector,     // start sector number in LBA
  UINT count        // size of byte buffer
){
  DWORD sect = (DWORD)sector;

	if (pdrv || !count) return RES_PARERR;		/* Check parameter */
	if (SD_Status & STA_NOINIT) return RES_NOTRDY;	/* Check drive status */
	if (SD_Status & STA_PROTECT) return RES_WRPRT;	/* Check write protect */
  
  if (!(CardType & CT_BLOCK)) sector *= 512;	// Converts from LBA to BA for non block addressed cards


  if(count == 1){
    if(send_cmd(CMD24, sector) == 0x00){
      send_byte(0xFF);
      if(write_block(buff, 0xFE)){// check to see if data was accepted
        count = 0;
      } 
    }
  }else{
    if(send_cmd(CMD25, sector) == 0x00){
      send_byte(0xFF);
      uint8_t res;
      while(count){
        if(!write_block(buff, 0xFC)) break;
        buff += 512;
        count--;
        wait_sd_ready(500);
      }
      send_byte(0xFD);
    }
  }

  sd_deselect();

  return count ? RES_ERROR : RES_OK;

}


DRESULT disk_read (
  BYTE pdrv,     /* [IN] Physical drive number */
  BYTE* buff,    /* [OUT] Pointer to the read data buffer */
  LBA_t sector,  /* [IN] Start sector number */
  UINT count     /* [IN] Number of sectros to read */
){

  if (pdrv || !count) return RES_PARERR;		//check for valid parameters
	if (SD_Status & STA_NOINIT) return RES_NOTRDY;	//Check to see if drive is ready

  if (!(CardType & CT_BLOCK)) sector *= 512;	// Converts from LBA to BA for non block addressed cards
  
  //decide if we need to send the cmd for multi block or single block read
  if(count == 1){
    if(send_cmd(CMD17, sector) == 0x00 && read_block(buff, 512)){
      count = 0;
    }
  }else{
    if(send_cmd(CMD18, sector) == 0x00){
      while(count){
        if(!read_block(buff, 512)) break;
        buff += 512;
        count--;
      }
    }

    send_cmd(CMD12, 0);
  }
  sd_deselect();
  return count ? RES_ERROR : RES_OK;
}


DRESULT disk_ioctl (
  BYTE pdrv,     /* [IN] Drive number */
  BYTE cmd,      /* [IN] Control command code */
  void* buff     /* [I/O] Parameter and data buffer */
){

  if(pdrv) return RES_PARERR;

  if(SD_Status & STA_NOINIT) return RES_NOTRDY;

  // will hold 16 bit csd data 
  uint8_t csd[16];
  uint8_t exp;
  DRESULT res;
  uint32_t c_size;

  res = RES_ERROR;

  switch(cmd){
    case (CTRL_SYNC):
      //gives clock cycles to SD card to make sure the last operation is complete
      if(sd_select()) res = RES_OK;
      sd_deselect();
      break;
    
    // here we want to get the total amount of 512 bit sectors that 
    case(GET_SECTOR_COUNT):
      //send cmd that will request CSD and read in CSD
      if(send_cmd(CMD9,0) == 0x00 && read_block(csd, 16)){
        //SDCv2
        if((csd[0] >> 6) == 1){
          c_size = csd[9] + ((uint16_t) csd[8] << 8) + ((uint32_t)(csd[7] & 0x3F) << 16) + 1;
          *(LBA_t*)buff = c_size << 10;
        //SDCv1
        }else{
          //the following calculation is a simplification of the equation given by SDv1 Specifications constructing storage size
          //construct c_size (12 bits) from the csd byte array
          c_size = (csd[8] >> 6) + ((uint16_t)csd[7] << 2) + ((uint32_t)(csd[6] & 0x03)  << 10) + 1;
          exp = (csd[10] >> 7) + ((csd[9] & 0x03) << 1) + (csd[5] >> 4) - 7;
          *(LBA_t*)buff = c_size << exp;
        }
        res = RES_OK;
      }
      break;  
      //SD cards manage their blocks internally
    case(GET_BLOCK_SIZE):
      *(DWORD*)buff = 1;
      res = RES_OK;
      break;
    default:
      res = RES_ERROR;


  }

  return res;


}

#if !FF_FS_NORTC && !FF_FS_READONLY
DWORD get_fattime (void)
{
    struct tm calendar_time = {0};
	/* Get local time */
	if (!aon_timer_get_time_calendar(&calendar_time)) return 0;

	/* Pack date and time into a DWORD variable */
	return	  ((DWORD)(calendar_time.tm_year - 80) << 25) // tm_year is stored as yrs since 1900, we need yrs since 1980
			| ((DWORD)(calendar_time.tm_mon + 1) << 21) // shift range from 0-11 to 1-12
			| ((DWORD)calendar_time.tm_mday << 16)
			| ((DWORD)calendar_time.tm_hour << 11)
			| ((DWORD)calendar_time.tm_min << 5)
			| ((DWORD)calendar_time.tm_sec >> 1); // secons is stored as sec/2
}
#endif




void timer_proc(){
  uint8_t n;
  n = Timer1;
  if(n) n--;
  n = Timer2;
  if(n) n--;
}
