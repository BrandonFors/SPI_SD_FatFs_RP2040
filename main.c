#include "ff.h"
#include <stdio.h>
#include "spi_sd_rp2040.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pico/time.h"


int main() {
  stdio_init_all();

  sleep_ms(5000);

  printf("Running Init\n");
  disk_initialize(0);
  
}
