#include "ff.h"
#include "diskio.h"
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "chan_test.h"
#include "custom_test.h"



int main() {
  
stdio_init_all();

    //small delay to 
    sleep_ms(5000);

    run_chan_test();


    return 0;
}
