# SD-SPI HAL for ChanFATFS library

## Overview
This repo contains a HAL to enbale use of ChanFATFS with the rp2040 and rp2350. It has only been tested with rp2040 boards but should work fine with rp2350.

All unique source code is in spi_sd_rp2040.c

Currently the HAL only supports a single sd card formatted with FAT32. EXFAT support comming soon.

Feedback welcome

## Brief Setup Instructions

Make sure you have an SD card formated w/ FAT32

Modify the SPI pin macros in `spi_sd_rp2040.c` to match the pins for your board.

Run the `main.c` with `run_chan_test()` *(this will wipe your card)* and observe the output over the serial console to verify the uC writes/reads the SD card properly. 

ChanFATFS:
https://www.elm-chan.org/fsw/ff/

About SD-SPI:
https://www.dejazzer.com/ee379/lecture_notes/lec12_sd_card.pdf

Official SD Docs:
https://www.sdcard.org/downloads/pls/

Refer to section 7 here:
https://academy.cba.mit.edu/classes/networking_communications/SD/SD.pdf