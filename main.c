#include "ff.h"
#include <stdio.h>
#include "spi_sd_rp2040.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pico/time.h"


int main() {
  stdio_init_all();

  init_sd_spi();
  
  while(1){
    printf("Sending Byte\n");
    sd_select();
    sleep_ms(10);
    send_byte(0xAA);
    sleep_ms(10);
    sd_deselect();
    sleep_ms(1000);
  }

}
