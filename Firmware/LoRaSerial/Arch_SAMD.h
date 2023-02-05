#if defined(ARDUINO_ARCH_SAMD)
#ifndef __SAMD_H__

#include <FlashAsEEPROM_SAMD.h> //Click here to get the library: http://librarymanager/All#FlashStorage_SAMD21 v1.2.1 by Khoi Hoang
#include <WDTZero.h> //https://github.com/javos65/WDTZero
WDTZero myWatchDog;

#define NVM_ERASE_VALUE         0xff
#define NVM_UNIQUE_ID_OFFSET    (EEPROM_EMULATION_SIZE - (MAX_VC * UNIQUE_ID_BYTES))

/*
  Data flow
                   +--------------+
                   |     SAMD     |
                   |              |
    TTL Serial <-->| Serial1      |       +--------------+
                   |          SPI |<----->| SX1276 Radio |<---> Antenna
    USB Serial <-->| Serial       |       +--------------+         ^
                   +--------------+                                |
                                                                   |
                   +--------------+                                |
                   |     SAMD     |                                |
                   |              |                                |
                   |  Sprinkler   |                                |
                   |  Controller  |                                |
                   |              |       +--------------+         V
        Debug      |          SPI |<----->| SX1276 Radio |<---> Antenna
    USB Serial <-->| Serial       |       +--------------+
                   +--------------+

                          +--------------+
                          | SAMD         |
                          |              |
                          |      PB02 A5 |---> rxLED
                          |      PB23 31 |---> txLED
                          |              |
                          |      PA07  9 |---> rssi4LED
                          |      PA06  8 |---> rssi3LED
                          |      PA05 A4 |---> rssi2LED
                          |      PA04 A3 |---> rssi1LED
                          |              |
    USB_D- <------------->| 28 PA24      |
    USB_D+ <------------->| 29 PA25      |
                          |              |
                          |      PA15  5 |---> cs -----> LORA_CS/
                          |      PA17 13 |---> SCLK ---> SPI_SCK
                          |      PA16 11 |---> MOSI ---> SPI_PICO
                          |      PA19 12 |<--- MISO ---> SPI_POCI
                          |              |
                          | 38 PA13      |
                          | 0  PA10      |
                          | 1  PA11      |
                          | 30 PB22      |
                          |              |
                          |      PA09  3 |---> rxen ---> LORA_RXEN
                          |      PA14  2 |---> txen ---> LORA_TXEN
                          |              |
                          |      PA18 10 |<--- dio1 <--- LORA_D1 (Freq Change)
                          |      PA21  7 |<--- dio0 <--- LORA_D0 (TX Done)
                          |              |
                          |      PA20  6 |---> rst ----> LORA_RST/
                          |              |
                          |      PA23 21 |---> SCL
                          |      PA22 20 |---> SDA
                          +--------------+
*/

//Initialize the LoRaSerial board
void samdBeginBoard()
{
  //Use ADC to check resistor divider
  pin_boardID = A2;

  pin_cs = 5;
  pin_dio0 = 7;  //aka A0
  pin_dio1 = 10; //aka A1
  pin_txen = 2;
  pin_rxen = 3;
  pin_rst = 6;
  pin_txLED = 31;
  pin_rxLED = A5;
  pin_rssi1LED = A3;
  pin_rssi2LED = A4;
  pin_rssi3LED = 8;
  pin_rssi4LED = 9;

  pin_trainButton = 4;

  pin_trigger = A0;
  pin_hop_timer = A1;

  //LEDs
  pinMode(pin_rssi1LED, OUTPUT);
  digitalWrite(pin_rssi1LED, LOW);
  pinMode(pin_rssi2LED, OUTPUT);
  digitalWrite(pin_rssi2LED, LOW);
  pinMode(pin_rssi3LED, OUTPUT);
  digitalWrite(pin_rssi3LED, LOW);
  pinMode(pin_rssi4LED, OUTPUT);
  digitalWrite(pin_rssi4LED, LOW);

  pinMode(pin_txLED, OUTPUT);
  digitalWrite(pin_txLED, LOW);
  pinMode(pin_rxLED, OUTPUT);
  digitalWrite(pin_rxLED, LOW);

  //Train button input
  pinMode(pin_trainButton, INPUT_PULLUP);

  //Debug
  pinMode(pin_trigger, OUTPUT);
  digitalWrite(pin_trigger, HIGH);
  pinMode(pin_hop_timer, OUTPUT);
  digitalWrite(pin_hop_timer, LOW);

  //Get average of board ID voltage divider
  int val = 0;
  for (int x = 0 ; x < 8 ; x++)
    val += analogRead(pin_boardID);
  val /= 8;

  //Convert ADC to volts
  float boardID = 3.3 * val / 1024;

  //Use ADC to check board ID resistor divider
  if (boardID > 1.64 * 0.95 && boardID < 1.64 * 1.05)
  {
    radioBand = 915;
    strcpy(platformPrefix, "SAMD21 1W 915MHz");
  }
  else if (boardID > 2.20 * 0.95 && boardID < 2.20 * 1.05)
  {
    radioBand = 433;
    strcpy(platformPrefix, "SAMD21 1W 433MHz");
  }
  else if (boardID > 2.48 * 0.95 && boardID < 2.48 * 1.05)
  {
    radioBand = 868;
    strcpy(platformPrefix, "SAMD21 1W 868MHz");
  }
  else
  {
    strcpy(platformPrefix, "SAMD21 1W");
  }
}

//Initialize the USB serial port and the UART
void samdBeginSerial(uint16_t serialSpeed)
{
  if (settings.usbSerialWait)
    //Wait for serial to come online for debug printing
    while (!Serial);
}

//Initialize the watch dog timer
void samdBeginWDT()
{
  myWatchDog.setup(WDT_HARDCYCLE2S);  // Initialize WDT with 2s timeout
  petTimeout = 1800;
}

//Initilaize the EEPROM controller or simulation
void samdEepromBegin()
{
  online.eeprom = true;
}

//Write any remaining data to EEPROM
void samdEepromCommit()
{
  EEPROM.commit();
}

//Perform the necessary action to "pet" the watch dog timer
void samdPetWDT()
{
  //This takes 4-5ms to complete
  myWatchDog.clear();
}

//Initialize the radio module
Module * samdRadio()
{
  return new Module(pin_cs, pin_dio0, pin_rst, pin_dio1);
}

//Determine if serial input data is available
bool samdSerialAvailable()
{
  return (Serial.available());
}

//Ensure that all serial output data has been sent over USB and via the UART
void samdSerialFlush()
{
  Serial.flush();
}

//Read in the serial input data
uint8_t samdSerialRead()
{
  byte incoming = 0;
  if (Serial.available())
    incoming = Serial.read();
  return (incoming);
}

//Provide the serial output data to the USB layer or the UART TX FIFO
void samdSerialWrite(uint8_t value)
{
  Serial.write(value);
}

//Reset the CPU
void samdSystemReset()
{
  NVIC_SystemReset();
}

//Get the CPU's unique ID value
void samdUniqueID(uint8_t * unique128_BitID)
{
  uint32_t id[UNIQUE_ID_BYTES / 4];

  //Read the CPU's unique ID value
  id[0] = *(uint32_t *)0x0080a00c;
  id[1] = *(uint32_t *)0x0080a040;
  id[2] = *(uint32_t *)0x0080a044;
  id[3] = *(uint32_t *)0x0080a048;

  memcpy(unique128_BitID, id, UNIQUE_ID_BYTES);
}

//Provide the hardware abstraction layer (HAL) interface
const ARCH_TABLE arch = {
  samdBeginBoard,             //beginBoard
  samdBeginSerial,            //beginSerial
  samdBeginWDT,               //beginWDT
  samdEepromBegin,            //eepromBegin
  samdEepromCommit,           //eepromCommit
  samdPetWDT,                 //petWDT
  samdRadio,                  //radio
  samdSerialAvailable,        //serialAvailable
  samdSerialFlush,            //serialFlush
  samdSerialRead,             //serialRead
  samdSerialWrite,            //serialWrite
  samdSystemReset,            //systemReset
  samdUniqueID,               //uniqueID
};

#endif  //__SAMD_H__
#endif  //ARDUINO_ARCH_SAMD