#include "ff.h"
#include <stdio.h>
#include "spi_sd_rp2040.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pico/time.h"


int main() {
  stdio_init_all();

  init_sd_spi();
  sleep_ms(5000);

  printf("Sending CMD0\n");
  send_cmd(0x00, 0x00000000);
}
