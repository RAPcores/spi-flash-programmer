python spi_flash_programmer_client.py -d COM5 --io 0x5e --value 0x0 set-output
python spi_flash_programmer_client.py -d COM5  -l -1 --pad 0xFF write -f ulticore.bit
python spi_flash_programmer_client.py -d COM5 --io 0x5e --value 0x1 set-output
