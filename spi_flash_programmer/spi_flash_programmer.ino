#include <stdint.h>
#include <stdio.h>

//#include <SPI.h>

// Commands always use other characters than 0-9 a-f A-F
// since the command data is always encoded in HEX
#define COMMAND_HELLO '>'
#define COMMAND_HELP '?'
#define COMMAND_BUFFER_CRC 'h'
#define COMMAND_BUFFER_LOAD 'l'
#define COMMAND_BUFFER_STORE 's'
#define COMMAND_FLASH_READ 'r'
#define COMMAND_FLASH_WRITE 'w'
#define COMMAND_FLASH_ERASE_SECTOR 'k'
#define COMMAND_FLASH_ERASE_ALL 'n'
#define COMMAND_WRITE_PROTECTION_ENABLE 'p'
#define COMMAND_WRITE_PROTECTION_DISABLE 'u'
#define COMMAND_WRITE_PROTECTION_CHECK 'x'
#define COMMAND_STATUS_REGISTER_READ 'y'
#define COMMAND_ID_REGISTER_READ 'i'
#define COMMAND_ERROR '!'
#define COMMAND_SET_CS '*'
#define COMMAND_SET_OUTPUT 'o'

#define WRITE_PROTECTION_NONE 0x00
#define WRITE_PROTECTION_PARTIAL 0x01
#define WRITE_PROTECTION_FULL 0x02
#define WRITE_PROTECTION_UNKNOWN 0x03

#define WRITE_PROTECTION_CONFIGURATION_NONE 0x00
#define WRITE_PROTECTION_CONFIGURATION_PARTIAL 0x01
#define WRITE_PROTECTION_CONFIGURATION_LOCKED 0x02
#define WRITE_PROTECTION_CONFIGURATION_UNKNOWN 0x03

#define VERSION "SPI Flash programmer v1.0"

#define SECTOR_SIZE 4096
#define PAGE_SIZE 256

void dump_buffer(void);
void dump_buffer_crc(void);
int8_t read_into_buffer(void);

void erase_all(void);
void erase_sector(uint32_t address);
void read_page(uint32_t address);
void write_page(uint32_t address);

uint32_t crc_buffer(void);
void wait_for_write_enable(void);

int8_t read_nibble(void);
int8_t read_hex_u8(uint8_t *value);
int8_t read_hex_u16(uint16_t *value);
int8_t read_hex_u32(uint32_t *value);
void write_hex_u8(uint8_t value);
void write_hex_u16(uint16_t value);

void impl_enable_write(void);
void impl_erase_chip(void);
void impl_erase_sector(void);
void impl_read_page(uint32_t address);
void impl_write_page(uint32_t address);
void impl_status_register_read(void);
void impl_write_protection_enable(void);
void impl_write_protection_disable(void);
void impl_write_protection_check(void);
void impl_wait_for_write_enable(void);

uint8_t buffer [PAGE_SIZE];
uint8_t nCsIo;

#define LFE5U_SPIFLASH_CS    23 // PA14 23 
#define PROGRAMN_PIN 94
#define LED_BUSY_PIN 35

void USART1config() {
  USART1->US_WPMR = 0x55534100;   // Unlock the USART Mode register
  USART1->US_MR |= 0x408CE | 0x100;       // Set Mode to CLK0=1, 8_BIT, SPI_MASTER
  USART1->US_BRGR = 21;           // Clock Divider (SCK = 4MHz)

  PIOA->PIO_WPMR = 0x50494F00;    // Unlock PIOA Write Protect Mode Register
  PIOB->PIO_WPMR = 0x50494F00;    // Unlock PIOB Write Protect Mode Register
  PIOA->PIO_ABSR |= (0u << 14);   // CS: Assign A14 I/O to the Peripheral A function
  PIOA->PIO_PDR |= (1u << 14);    // CS: Disable PIO control, enable peripheral control
  PIOA->PIO_ABSR |= (0u << 16);   // SCK: Assign A16 I/O to the Peripheral A function
  PIOA->PIO_PDR |= (1u << 16);    // SCK: Disable PIO control, enable peripheral control
  PIOA->PIO_ABSR |= (0u << 13);   // MOSI: Assign PA13 I/O to the Peripheral A function
  PIOA->PIO_PDR |= (1u << 13);    // MOSI: Disable PIO control, enable peripheral control
  PIOA->PIO_ABSR |= (0u << 12);   // MISO: Assign A12 I/O to the Peripheral A function
  PIOA->PIO_PDR |= (1u << 12);    // MISO: Disable PIO control, enable peripheral control
}

uint32_t uspi_init()
{
  Serial2.begin(9600);  // pre-initialize USART1
  USART1config();       // setup mode, clock and pins
  //USART1status();

  pinMode(LFE5U_SPIFLASH_CS,OUTPUT);
  digitalWrite(LFE5U_SPIFLASH_CS,HIGH);

  pinMode(PROGRAMN_PIN,OUTPUT);
  digitalWrite(PROGRAMN_PIN,LOW);

  pinMode(LED_BUSY_PIN,OUTPUT);
  digitalWrite(LED_BUSY_PIN,LOW);
}

uint32_t uspi_transfer(uint32_t data)
{
    
    USART1->US_THR = data;
    //SerialUSB.print("  RX: ");
    delayMicroseconds(4);
    while( ! ((USART1->US_CSR  >> 1) & 0x1u) );
    uint8_t response = USART1->US_RHR;
    //delayMicroseconds(1);
    //SerialUSB.print( response );
    return response;
}

void setup()
{
  uspi_init();
  
  nCsIo = LFE5U_SPIFLASH_CS;

  // Use maximum speed with F_CPU / 2
  //SPISettings settingsA(F_CPU / 2, MSBFIRST, SPI_MODE0);
  uint16_t i;

  for (i = 0; i < PAGE_SIZE; i += 4)
  { // Initialize buffer with 0xDEADBEEF
    buffer[i + 0] = 0xDE;
    buffer[i + 1] = 0xAD;
    buffer[i + 2] = 0xBE;
    buffer[i + 3] = 0xEF;
  }

  SerialUSB.begin(115200);

  //SPI.begin(); // Initialize pins
  //SPI.beginTransaction(settingsA);
  pinMode(nCsIo, OUTPUT);
  digitalWrite(nCsIo, HIGH); // disable flash device

  delay(10);
}

void loop()
{
  if(SerialUSB.available())
  {
    uint8_t cmd = SerialUSB.read();
    switch(cmd)
    {
      case '>':
        SerialUSB.println("hello");
        //write_hex_u8(0xFF);
        //impl_jedec_id_read();
        spiflash_init();
        break;
      case '?':
        SerialUSB.println(VERSION);
        SerialUSB.println("  n         : erase chip");
        SerialUSB.println("  kXXXXXXXX : erase 4k sector XXXXXXXX (hex)");
        SerialUSB.println();        delay(1);
        SerialUSB.println("  rXXXXXXXX : read a page XXXXXXXX (hex) to buffer");
        SerialUSB.println("  wXXXXXXXX : write buffer to a page XXXXXXXX (hex)");
        SerialUSB.println();        delay(1);
        SerialUSB.println("  p         : enable write protection");
        SerialUSB.println("  u         : disable write protection");
        SerialUSB.println("  x         : check write protection");
        SerialUSB.println("  y         : read status register");
        SerialUSB.println("  i         : read id register");
        SerialUSB.println();        delay(1);
        SerialUSB.println("  h         : print buffer CRC-32");
        SerialUSB.println("  l         : display the buffer (in hex)");
        SerialUSB.println("  sBBBBBBBB : load the buffer with a page size of data BBBBBBBB...");
        SerialUSB.println();        delay(1);
        SerialUSB.println("  *XX       : set IO XX as CS/SS");
        SerialUSB.println("  oXXYZ     : set IO XX as output, set value Z if Y!=0");
        SerialUSB.println();        delay(1);
        SerialUSB.println("Examples:");
        SerialUSB.println("  r00003700      read data from page 0x3700 into buffer");
        SerialUSB.println("  scafe...3737   load the buffer with a page of data, first byte is 0xca ...");
        break;
    }
  } else {
    delay(100);
  }
}

void loop2()
{
  uint32_t address;
  uint8_t tmp8;
  uint16_t tmp16;

  // Wait for command
  //while(SerialUSB.available() == 0) {
  //  ; // Do nothing
  //}

  //if( ! SerialUSB.available() ) return;

  int cmd = SerialUSB.read();
  switch(cmd) {
  case COMMAND_HELLO:
    SerialUSB.print(COMMAND_HELLO); // Echo OK
    SerialUSB.println(VERSION);
    SerialUSB.print(COMMAND_HELLO); // Echo 2nd OK
    break;

  case COMMAND_FLASH_ERASE_ALL:
    erase_all();
    SerialUSB.print(COMMAND_FLASH_ERASE_ALL); // Echo OK
    break;

  case COMMAND_FLASH_ERASE_SECTOR:
    if (!read_hex_u32(&address)) {
      SerialUSB.print(COMMAND_ERROR); // Echo Error
      break;
    }

    erase_sector(address);
    SerialUSB.print(COMMAND_FLASH_ERASE_SECTOR); // Echo OK
    break;

  case COMMAND_FLASH_READ:
    if (!read_hex_u32(&address)) {
      SerialUSB.print(COMMAND_ERROR); // Echo Error
      break;
    }

    read_page(address);
    SerialUSB.print(COMMAND_FLASH_READ); // Echo OK
    dump_buffer_crc();
    break;

  case COMMAND_FLASH_WRITE:
    if (!read_hex_u32(&address)) {
      SerialUSB.print(COMMAND_ERROR); // Echo Error
      break;
    }

    write_page(address);
    SerialUSB.print(COMMAND_FLASH_WRITE); // Echo OK
    break;

  case COMMAND_BUFFER_LOAD:
    SerialUSB.print(COMMAND_BUFFER_LOAD); // Echo OK
    dump_buffer();
    SerialUSB.println();
    break;

  case COMMAND_BUFFER_CRC:
    SerialUSB.print(COMMAND_BUFFER_CRC); // Echo OK
    dump_buffer_crc();
    SerialUSB.println();
    break;

  case COMMAND_BUFFER_STORE:
    if (!read_into_buffer()) {
      SerialUSB.print(COMMAND_ERROR); // Echo Error
      break;
    }

    SerialUSB.print(COMMAND_BUFFER_STORE); // Echo OK
    dump_buffer_crc();
    break;

  case COMMAND_WRITE_PROTECTION_CHECK:
    SerialUSB.print(COMMAND_WRITE_PROTECTION_CHECK); // Echo OK
    impl_write_protection_check();
    break;

  case COMMAND_WRITE_PROTECTION_ENABLE:
    SerialUSB.print(COMMAND_WRITE_PROTECTION_ENABLE); // Echo OK
    impl_write_protection_enable();
    break;

  case COMMAND_WRITE_PROTECTION_DISABLE:
    SerialUSB.print(COMMAND_WRITE_PROTECTION_DISABLE); // Echo OK
    impl_write_protection_disable();
    break;

  case COMMAND_STATUS_REGISTER_READ:
    SerialUSB.print(COMMAND_STATUS_REGISTER_READ); // Echo OK
    impl_status_register_read();
    break;

  case COMMAND_ID_REGISTER_READ:
    SerialUSB.print(COMMAND_ID_REGISTER_READ); // Echo OK
    impl_jedec_id_read();
    break;

  case COMMAND_SET_CS:
    if(!read_hex_u8(&tmp8)) {
      SerialUSB.print(COMMAND_ERROR); // Echo Error
      break;
    }
    if (tmp8 != nCsIo) {
      if (nCsIo != SS)
        pinMode(nCsIo, INPUT);
      nCsIo=tmp8;
      pinMode(nCsIo, OUTPUT);
      digitalWrite(nCsIo, HIGH); // disable flash device
    }

    SerialUSB.print(COMMAND_SET_CS); // Echo OK
    break;

  case COMMAND_SET_OUTPUT:
    if(!read_hex_u16(&tmp16)) {
      SerialUSB.print(COMMAND_ERROR); // Echo Error
      break;
    }
    pinMode(tmp16>>8, OUTPUT);
    if (tmp16 & 0xf0) {
      if (tmp16 & 0xf) {
        digitalWrite(tmp16>>8, HIGH);
      }
      else {
        digitalWrite(tmp16>>8, LOW);
      }
    }

    SerialUSB.print(COMMAND_SET_OUTPUT); // Echo OK
    break;

  case COMMAND_HELP:
    SerialUSB.println(VERSION);
    SerialUSB.println("  n         : erase chip");
    SerialUSB.println("  kXXXXXXXX : erase 4k sector XXXXXXXX (hex)");
    SerialUSB.println();
    SerialUSB.println("  rXXXXXXXX : read a page XXXXXXXX (hex) to buffer");
    SerialUSB.println("  wXXXXXXXX : write buffer to a page XXXXXXXX (hex)");
    SerialUSB.println();
    SerialUSB.println("  p         : enable write protection");
    SerialUSB.println("  u         : disable write protection");
    SerialUSB.println("  x         : check write protection");
    SerialUSB.println("  y         : read status register");
    SerialUSB.println("  i         : read id register");
    SerialUSB.println();
    SerialUSB.println("  h         : print buffer CRC-32");
    SerialUSB.println("  l         : display the buffer (in hex)");
    SerialUSB.println("  sBBBBBBBB : load the buffer with a page size of data BBBBBBBB...");
    SerialUSB.println();
    SerialUSB.println("  *XX       : set IO XX as CS/SS");
    SerialUSB.println("  oXXYZ     : set IO XX as output, set value Z if Y!=0");
    SerialUSB.println();
    SerialUSB.println("Examples:");
    SerialUSB.println("  r00003700      read data from page 0x3700 into buffer");
    SerialUSB.println("  scafe...3737   load the buffer with a page of data, first byte is 0xca ...");
    break;
  }

  //SerialUSB.flush();
} 

void read_page(uint32_t address)
{
  // Send read command
  digitalWrite(nCsIo, LOW);
  impl_read_page(address);

  // Release chip, signal end transfer
  digitalWrite(nCsIo, HIGH);
} 

void write_page(uint32_t address)
{
  digitalWrite(nCsIo, LOW);
  impl_enable_write();
  digitalWrite(nCsIo, HIGH);
  //delay(10);
  delayMicroseconds(1);

  digitalWrite(nCsIo, LOW);
  impl_write_page(address);
  digitalWrite(nCsIo, HIGH);
  //delay(1); // Wait for 1 ms

  impl_wait_for_write_enable();
}

void erase_all()
{
  digitalWrite(nCsIo, LOW);
  impl_enable_write();
  digitalWrite(nCsIo, HIGH);
  delay(10); // Wait for 10 ms

  digitalWrite(nCsIo, LOW);
  impl_erase_chip();
  digitalWrite(nCsIo, HIGH);
  delay(1); // Wait for 1 ms

  impl_wait_for_write_enable();
}

void erase_sector(uint32_t address)
{
  digitalWrite(nCsIo, LOW);
  impl_enable_write();
  digitalWrite(nCsIo, HIGH);
  delay(10);

  digitalWrite(nCsIo, LOW);
  impl_erase_sector(address);
  digitalWrite(nCsIo, HIGH);

  impl_wait_for_write_enable();
}

void dump_buffer(void)
{
  uint16_t counter;

  for(counter = 0; counter < PAGE_SIZE; counter++) {
    write_hex_u8(buffer[counter]);
  }
}

void dump_buffer_crc(void)
{
  uint32_t crc = crc_buffer();
  write_hex_u16((crc >> 16) & 0xFFFF);
  write_hex_u16(crc & 0xFFFF);
}

int8_t read_into_buffer(void)
{
  uint16_t counter;
  uint8_t tmp;

  for(counter = 0; counter < PAGE_SIZE; counter++) {
    if (!read_hex_u8(&tmp)) {
      return 0;
    }

    buffer[counter] = (uint8_t) tmp;
  }

  return 1;
}

int8_t read_nibble(void)
{
  int16_t c;

  do {
    c = SerialUSB.read();
  } while(c == -1);

  if (c >= '0' && c <= '9') {
    return (c - '0') + 0;
  } else if (c >= 'a' && c <= 'f') {
    return (c - 'a') + 10;
  } else if (c >= 'A' && c <= 'F') {
    return (c - 'A') + 10;
  } else {
    return -1;
  }
}

int8_t read_hex_u16(uint16_t *value)
{
  int8_t i, tmp;
  uint16_t result = 0;

  for (i = 0; i < 4; i++) {
    tmp = read_nibble();
    if (tmp == -1) {
      return 0;
    }

    result <<= 4;
    result |= ((uint8_t) tmp) & 0x0F;
  }

  (*value) = result;

  return 1;
}

int8_t read_hex_u8(uint8_t *value)
{
  int8_t i, tmp;
  uint8_t result = 0;

  for (i = 0; i < 2; i++) {
    tmp = read_nibble();
    if (tmp == -1) {
      return 0;
    }

    result <<= 4;
    result |= ((uint8_t) tmp) & 0x0F;
  }

  (*value) = result;

  return 1;
}

int8_t read_hex_u32(uint32_t *value)
{
  int8_t i, tmp;
  uint32_t result = 0;

  for (i = 0; i < 8; i++) {
    tmp = read_nibble();
    if (tmp == -1) {
      return 0;
    }

    result <<= 4;
    result |= ((uint32_t) tmp) & 0x0F;
  }

  (*value) = result;

  return 1;
}

void write_nibble(uint8_t value)
{
  if (value < 10) {
    SerialUSB.write(value + '0' - 0);
  } else {
    SerialUSB.write(value + 'A' - 10);
  }
}

void write_hex_u8(uint8_t value)
{
    uint8_t i;

    for (i = 0; i < 2; i++) {
      write_nibble((uint8_t) ((value >> 4) & 0x0F));
      value <<= 4;
    }
}

void write_hex_u16(uint16_t value)
{
    uint8_t i;

    for (i = 0; i < 4; i++) {
      write_nibble((uint8_t) ((value >> 12) & 0x0F));
      value <<= 4;
    }
}

// Via http://excamera.com/sphinx/article-crc.html
static const uint32_t crc_table[16] = {
  0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
  0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
  0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
  0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

uint32_t crc_update(uint32_t crc, uint8_t data)
{
  uint8_t tbl_idx;

  tbl_idx = crc ^ (data >> (0 * 4));
  crc = crc_table[tbl_idx & 0x0f] ^ (crc >> 4);

  tbl_idx = crc ^ (data >> (1 * 4));
  crc = crc_table[tbl_idx & 0x0f] ^ (crc >> 4);

  return crc;
}

uint32_t crc_buffer(void)
{
  uint16_t i;
  uint32_t crc = ~0L;

  for(i = 0; i < PAGE_SIZE; i++) {
    crc = crc_update(crc, buffer[i]);
  }

  crc = ~crc;

  return crc;
}

// ---------------------------------------------------------------------------
// Chip implementation specific code
// ---------------------------------------------------------------------------

// SPI opcodes
#define WREN         0x06
#define WRDI         0x04
#define RDSR         0x05
#define RDSR2        0x35
#define RDSR3        0x15
#define WRSR         0x01
#define WRSR2        0x31
#define WRSR3        0x11
#define READ         0x03
#define WRITE        0x02
#define SECTOR_ERASE 0x20
#define CHIP_ERASE   0xC7
#define JEDECIDR     0x9F

#define WPS          0x040000
#define CP           0x000400
#define SRP          0x000180
#define SRP1         0x000100
#define SRP0         0x000080
#define BP           0x00001C

void impl_enable_write(void)
{
  uspi_transfer(WREN); // write enable
}

void impl_erase_chip(void)
{
  uspi_transfer(CHIP_ERASE);
}

void impl_erase_sector(uint32_t address)
{
  uspi_transfer(SECTOR_ERASE);            // sector erase instruction
  uspi_transfer((address & 0x0FF0) >> 4); // bits 23 to 16
  uspi_transfer((address & 0x000F) << 4); // bits 15 to 8
  uspi_transfer(0);                       // bits 7 to 0
}

void impl_read_page(uint32_t address)
{
  uint16_t counter;

  uspi_transfer(READ);                  // read instruction
  //uspi_transfer((address >> 16) & 0xFF); // bits 23 to 16
  uspi_transfer((address >> 8) & 0xFF); // bits 23 to 16
  uspi_transfer(address & 0xFF);        // bits 15 to 8
  uspi_transfer(0);                     // bits 7 to 0

  // Transfer a dummy page to read data
  for(counter = 0; counter < PAGE_SIZE; counter++) {
    buffer[counter] = uspi_transfer(0xff);
  }
}

void impl_write_page(uint32_t address)
{
  uint16_t counter;

  uspi_transfer(WRITE);                 // write instruction
  //uspi_transfer((address >> 16) & 0xFF); // bits 23 to 16
  uspi_transfer((address >> 8) & 0xFF); // bits 23 to 16
  uspi_transfer(address & 0xFF);        // bits 15 to 8
  uspi_transfer(0);                     // bits 7 to 0

  for (counter = 0; counter < PAGE_SIZE; counter++) {
    uspi_transfer(buffer[counter]);
  }
}

void impl_wait_for_write_enable(void)
{
  uint8_t statreg = 0x1;

  while((statreg & 0x1) == 0x1) {
    // Wait for the chip
    digitalWrite(nCsIo, LOW);
    uspi_transfer(RDSR);
    statreg = uspi_transfer(RDSR);
    digitalWrite(nCsIo, HIGH);
  }
}

void impl_write_protection_check(void)
{
  uint32_t statusRegister;

  // Read status register 1
  digitalWrite(nCsIo, LOW);
  uspi_transfer(RDSR);
  statusRegister = ((uint32_t) uspi_transfer(RDSR));
  digitalWrite(nCsIo, HIGH);

  // Read status register 2
  digitalWrite(nCsIo, LOW);
  uspi_transfer(RDSR2);
  statusRegister |= ((uint32_t) uspi_transfer(RDSR2)) << 8;
  digitalWrite(nCsIo, HIGH);

  // Read status register 3
  digitalWrite(nCsIo, LOW);
  uspi_transfer(RDSR3);
  statusRegister |= ((uint32_t) uspi_transfer(RDSR3)) << 16;
  digitalWrite(nCsIo, HIGH);

  if (statusRegister & SRP1) {
    write_hex_u8(WRITE_PROTECTION_CONFIGURATION_LOCKED);
  } else {
    write_hex_u8(WRITE_PROTECTION_CONFIGURATION_NONE);
  }

  if (statusRegister & WPS) {
    write_hex_u8(WRITE_PROTECTION_PARTIAL);
    return;
  }

  // Complement protect
  if (statusRegister & CP) {
    // Protection is inverted
    if ((statusRegister & BP) == BP) {
      write_hex_u8(WRITE_PROTECTION_NONE);
      return;
    }

    write_hex_u8((statusRegister & BP)
        ? WRITE_PROTECTION_PARTIAL : WRITE_PROTECTION_FULL);
  } else {
    // Protection is not inverted
    if ((statusRegister & BP) == BP) {
      write_hex_u8(WRITE_PROTECTION_FULL);
      return;
    }

    write_hex_u8((statusRegister & BP)
        ? WRITE_PROTECTION_PARTIAL : WRITE_PROTECTION_NONE);
  }
}

void impl_write_protection_disable(void)
{
  uint8_t statusRegister;
  uint8_t statusRegister2;

  // Read status register 1
  digitalWrite(nCsIo, LOW);
  uspi_transfer(RDSR);
  statusRegister = uspi_transfer(RDSR);
  digitalWrite(nCsIo, HIGH);

  // Read status register 2
  digitalWrite(nCsIo, LOW);
  uspi_transfer(RDSR2);
  statusRegister2 = uspi_transfer(RDSR2);
  digitalWrite(nCsIo, HIGH);

  // Set chip as writable
  digitalWrite(nCsIo, LOW);
  uspi_transfer(WREN); // Write enable
  digitalWrite(nCsIo, HIGH);
  delay(10);

  digitalWrite(nCsIo, LOW);
  uspi_transfer(WRSR);                         // Write register instruction
  uspi_transfer(statusRegister & ~BP);         // Force SR1 to XXX000XX
  digitalWrite(nCsIo, HIGH);

  // Set chip as writable
  digitalWrite(nCsIo, LOW);
  uspi_transfer(WREN); // Write enable
  digitalWrite(nCsIo, HIGH);
  delay(10);

  digitalWrite(nCsIo, LOW);
  uspi_transfer(WRSR2);                        // Write register 2 instruction
  uspi_transfer(statusRegister2 & ~(CP >> 8)); // Force SR2 to X0XXXXXX
  digitalWrite(nCsIo, HIGH);
  delay(1);

  impl_wait_for_write_enable();
}

void impl_write_protection_enable(void)
{
  uint8_t statusRegister;
  uint8_t statusRegister2;

  // Read status register 1
  digitalWrite(nCsIo, LOW);
  uspi_transfer(RDSR);
  statusRegister = uspi_transfer(RDSR);
  digitalWrite(nCsIo, HIGH);

  // Read status register 2
  digitalWrite(nCsIo, LOW);
  uspi_transfer(RDSR2);
  statusRegister2 = uspi_transfer(RDSR2);
  digitalWrite(nCsIo, HIGH);

  // Set chip as writable
  digitalWrite(nCsIo, LOW);
  uspi_transfer(WREN); // Write enable
  digitalWrite(nCsIo, HIGH);
  delay(10);

  digitalWrite(nCsIo, LOW);
  uspi_transfer(WRSR);                         // Write register instruction
  uspi_transfer(statusRegister | BP);          // Force SR1 to XXX111XX
  digitalWrite(nCsIo, HIGH);

  // Set chip as writable
  digitalWrite(nCsIo, LOW);
  uspi_transfer(WREN); // Write enable
  digitalWrite(nCsIo, HIGH);
  delay(10);

  digitalWrite(nCsIo, LOW);
  uspi_transfer(WRSR2);                        // Write register 2 instruction
  uspi_transfer(statusRegister2 & ~(CP >> 8)); // Force SR2 to X0XXXXXX
  digitalWrite(nCsIo, HIGH);
  delay(1);

  impl_wait_for_write_enable();
}

void impl_status_register_read(void)
{
  uint8_t statusRegister;
  uint8_t statusRegister2;
  uint8_t statusRegister3;

  // Read status register 1
  digitalWrite(nCsIo, LOW);
  uspi_transfer(RDSR);
  statusRegister = uspi_transfer(RDSR);
  digitalWrite(nCsIo, HIGH);

  // Read status register 2
  digitalWrite(nCsIo, LOW);
  uspi_transfer(RDSR2);
  statusRegister2 = uspi_transfer(RDSR2);
  digitalWrite(nCsIo, HIGH);

  // Read status register 3
  digitalWrite(nCsIo, LOW);
  uspi_transfer(RDSR3);
  statusRegister3 = uspi_transfer(RDSR3);
  digitalWrite(nCsIo, HIGH);

  // Send status register length
  write_hex_u8(0x03);

  // Write register content
  write_hex_u8(statusRegister);
  write_hex_u8(statusRegister2);
  write_hex_u8(statusRegister3);
}

void impl_jedec_id_read(void)
{
  digitalWrite(nCsIo, LOW);
  delayMicroseconds(1);
  uspi_transfer(JEDECIDR);
  write_hex_u8(0x03);
  write_hex_u8(uspi_transfer(0x0));
  write_hex_u8(uspi_transfer(0x0));
  write_hex_u8(uspi_transfer(0x0));
  digitalWrite(nCsIo, HIGH);
}

void spiflash_init()
{
  //pinMode(SPIFLASH_CS,OUTPUT);
  //digitalWrite(SPIFLASH_CS,HIGH);
  //SPI.begin(SPIFLASH_CS);
  digitalWrite(nCsIo,LOW);
  // MFG ID
  SerialUSB.print(PSTR("SPIFLASH_MFG_ID: ")); 
  SerialUSB.print( uspi_transfer(0x9F) );
  SerialUSB.print(" "); 
  SerialUSB.print( uspi_transfer(0x00) );
  SerialUSB.print(" "); 
  SerialUSB.print( uspi_transfer(0x00) );
  SerialUSB.print(" "); 
  SerialUSB.println( uspi_transfer(0x00) );

  digitalWrite(nCsIo,HIGH);
}
