
#include "ff.h"
#include "diskio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h" 

void write_to_csv_test(){

  printf("Start\n");
  
  FATFS FatFs;
  FRESULT fr;
  FIL fil;
  int mounted = 0;
  int opened = 0;

  fr = f_mount(&FatFs, "", 1);
  if (fr != FR_OK) {
    printf("f_mount failed: %d\n", (int)fr);
    goto done;
  }
  mounted = 1;
  printf("Mounted OK\n");

  fr = f_open(&fil, "data.csv", FA_OPEN_APPEND | FA_WRITE);
  if (fr != FR_OK) {
    printf("f_open failed: %d\n", (int)fr);
    goto done;
  }
  opened = 1;
  printf("Opened data.csv OK\n");

  const char *s = "IRIS actually does stuff\r\n";
  UINT bw = 0;
  fr = f_write(&fil, s, (UINT)strlen(s), &bw);
  if (fr != FR_OK) {
    printf("f_write 1 failed: %d\n", (int)fr);
    goto done;
  }
  s = "Defund the CubeSAT\r\n";
    fr = f_write(&fil, s, (UINT)strlen(s), &bw);
  if (fr != FR_OK) {
    printf("f_write 2 failed: %d\n", (int)fr);
    goto done;
  }
  
  if (bw != (UINT)strlen(s)) {
    printf("f_write short write: wrote %u of %u\n", (unsigned)bw, (unsigned)strlen(s));
    goto done;
  }
  printf("Wrote %u bytes\n", (unsigned)bw);

done:
  if (opened) {
    fr = f_close(&fil);
    printf("f_close: %d\n", (int)fr);
  }
  if (mounted) {
    fr = f_unmount("");
    printf("f_unmount: %d\n", (int)fr);
  }

  printf("End\n");

}

void test_spi_funx(){

  
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

  result = disk_ioctl(0, CTRL_SYNC, NULL);
  if(result == RES_OK){
    printf("CTL_SYNC successfully\n");
  }else{
    printf("CTRL_SYNC failed\n");
  }

}