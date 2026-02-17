#include "ff.h"
#include <stdio.h>
#include "spi_sd_rp2040.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pico/time.h"


int main() {
  stdio_init_all();

  sleep_ms(5000);
  printf("Checking Disk Status\n");
  DSTATUS status = disk_status(0);
  printf("0x%02X\n", (unsigned)status);
  printf("Running Init\n");
  disk_initialize(0);
  printf("Checking Disk Status\n");
  status = disk_status(0);
  printf("0x%02X\n", (unsigned)status);
  int result;
  printf("Reading 1 Block\n");
  uint8_t block_test_1[512];
  result = disk_read(0, block_test_1, 0, 1);
  if(result == RES_OK){
    printf("Read 1 Block successfully\n");
  }else{
    printf("Read 1 Block failed\n");
  }

  printf("Reading 3 Blocks\n");
  uint8_t block_test_2[512*3];
  result = disk_read(0, block_test_2, 0, 3);
  if(result == RES_OK){
    printf("Read 3 Blocks successfully\n");
  }else{
    printf("Read 3 Blocks failed\n");
  }

  printf("Reading 5 Blocks\n");
  uint8_t block_test_3[512*5];
  result = disk_read(0, block_test_3, 0, 5);
  if(result == RES_OK){
    printf("Read 5 Blocks successfully\n");
  }else{
    printf("Read 5 Blocks failed\n");
  }

  printf("Writing 1 Block\n");
  uint8_t block_test_4[512] = {0};
  result = disk_write(0, block_test_4, 67, 1);
  if(result == RES_OK){
    printf("Write 1 Block successfully\n");
  }else{
    printf("Write 1 Block failed\n");
  }


  printf("Writing 5 Blocks\n");
  uint8_t block_test_5[512*5] = {0};
  result = disk_write(0, block_test_5, 67, 5);
  if(result == RES_OK){
    printf("Write 5 Blocks successfully\n");
  }else{
    printf("Write 5 Blocks failed\n");
  }
}
