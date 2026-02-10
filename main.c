#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"





int main()
{
    stdio_init_all();



    // For more examples of SPI use see https://github.com/raspberrypi/pico-examples/tree/master/spi

    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}