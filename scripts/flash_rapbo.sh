#!/bin/sh
~/spi_flash_programmer_client.py -d /dev/ttyACM0 --io 0x29 --value 0x0 set-output
~/spi_flash_programmer_client.py -d /dev/ttyACM0  -l -1 --pad 0xFF write -f `ls *.bit`
~/spi_flash_programmer_client.py -d /dev/ttyACM0 --io 0x29 --value 0x1 set-output
