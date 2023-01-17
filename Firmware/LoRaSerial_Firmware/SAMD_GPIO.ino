#if defined(ARDUINO_ARCH_SAMD)

void * const samdGpioBaseAddress = (void *)0x41004400;
const uint32_t samdGpioPorts[3] = {0xdbffffff, 0xc0c3ffff, 0};

typedef struct _SAMD_GPIO_REGS
{
  uint32_t DIR;         //0x00
  uint32_t DIRCLR;      //0x04
  uint32_t DIRSET;      //0x08
  uint32_t DIRTGL;      //0x0c
  uint32_t OUT;         //0x10
  uint32_t OUTCLR;      //0x14
  uint32_t OUTSET;      //0x18
  uint32_t OUTTGL;      //0x1c
  uint32_t IN;          //0x20
  uint32_t CTRL;        //0x24
  uint32_t WRCONFIG;    //0x28
  uint32_t res_2c;      //0x2c
  uint8_t PMUX0;        //0x30
  uint8_t PMUX1;        //0x31
  uint8_t PMUX2;        //0x32
  uint8_t PMUX3;        //0x33
  uint8_t PMUX4;        //0x34
  uint8_t PMUX5;        //0x35
  uint8_t PMUX6;        //0x36
  uint8_t PMUX7;        //0x37
  uint8_t PMUX8;        //0x38
  uint8_t PMUX9;        //0x39
  uint8_t PMUX10;       //0x3a
  uint8_t PMUX11;       //0x3b
  uint8_t PMUX12;       //0x3c
  uint8_t PMUX13;       //0x3d
  uint8_t PMUX14;       //0x3e
  uint8_t PMUX15;       //0x3f
  uint8_t PINCFG0;      //0x40
  uint8_t PINCFG1;      //0x41
  uint8_t PINCFG2;      //0x42
  uint8_t PINCFG3;      //0x43
  uint8_t PINCFG4;      //0x44
  uint8_t PINCFG5;      //0x45
  uint8_t PINCFG6;      //0x46
  uint8_t PINCFG7;      //0x47
  uint8_t PINCFG8;      //0x48
  uint8_t PINCFG9;      //0x49
  uint8_t PINCFG10;     //0x4a
  uint8_t PINCFG11;     //0x4b
  uint8_t PINCFG12;     //0x4c
  uint8_t PINCFG13;     //0x4d
  uint8_t PINCFG14;     //0x4e
  uint8_t PINCFG15;     //0x4f
  uint8_t PINCFG16;     //0x50
  uint8_t PINCFG17;     //0x51
  uint8_t PINCFG18;     //0x52
  uint8_t PINCFG19;     //0x53
  uint8_t PINCFG20;     //0x54
  uint8_t PINCFG21;     //0x55
  uint8_t PINCFG22;     //0x56
  uint8_t PINCFG23;     //0x57
  uint8_t PINCFG24;     //0x58
  uint8_t PINCFG25;     //0x59
  uint8_t PINCFG26;     //0x5a
  uint8_t PINCFG27;     //0x5b
  uint8_t PINCFG28;     //0x5c
  uint8_t PINCFG29;     //0x5d
  uint8_t PINCFG30;     //0x5e
  uint8_t PINCFG31;     //0x5f
} SAMD_GPIO_REGS;

const char * const samdGpioAMux[32][8] =
{//  A          B         C          D          E       F       G           H
 //  0          1         2          3          4       5       6           7
  {"EXTINT0",  "?",      "?",       "SERCOM1", "TCC2", "?",    "?",        "?"},        // PA0
  {"EXTINT1",  "?",      "?",       "SERCOM1", "TCC2", "?",    "?",        "?"},        // PA1
  {"EXTINT2",  "ANALOG", "?",       "?",       "?",    "?",    "?",        "?"},        // PA2
  {"EXTINT3",  "ANALOG", "?",       "?",       "?",    "?",    "?",        "?"},        // PA3
  {"EXTINT4",  "ANALOG", "?",       "SERCOM0", "TCC0", "?",    "?",        "?"},        // PA4
  {"EXTINT5",  "ANALOG", "?",       "SERCOM0", "TCC0", "?",    "?",        "?"},        // PA5
  {"EXTINT6",  "ANALOG", "?",       "SERCOM0", "TCC1", "?",    "?",        "?"},        // PA6
  {"EXTINT7",  "ANALOG", "?",       "SERCOM0", "TCC1", "?",    "I2S/SD0",  "?"},        // PA7
  {"NMI",      "ANALOG", "SERCOM0", "SERCOM2", "TCC0", "TCC1", "I2S/SD1",  "?"},        // PA8
  {"EXTINT9",  "ANALOG", "SERCOM0", "SERCOM2", "TCC0", "TCC1", "I2S/MCK0", "?"},        // PA9
  {"EXTINT10", "ANALOG", "SERCOM0", "SERCOM2", "TCC1", "TCC0", "I2S/SCK0", "GCLK_IO4"}, //PA10
  {"EXTINT11", "ANALOG", "SERCOM0", "SERCOM2", "TCC1", "TCC0", "I2S/FS0",  "GCLK_IO5"}, //PA11
  {"EXTINT12", "?",      "SERCOM2", "SERCOM4", "TCC2", "TCC0", "?",        "AC/CMP0"},  //PA12
  {"EXTINT13", "?",      "SERCOM2", "SERCOM4", "TCC2", "TCC0", "?",        "AC/CMP1"},  //PA13
  {"EXTINT14", "?",      "SERCOM2", "SERCOM4", "TC3",  "TCC0", "?",        "GCLK_IO0"}, //PA14
  {"EXTINT15", "?",      "SERCOM2", "SERCOM4", "TC3",  "TCC0", "?",        "GCLK_IO1"}, //PA15
  {"EXTINT0",  "ANALOG", "SERCOM1", "SERCOM3", "TCC2", "TCC0", "?",        "GCLK_IO2"}, //PA16
  {"EXTINT1",  "ANALOG", "SERCOM1", "SERCOM3", "TCC2", "TCC0", "?",        "GCLK_IO3"}, //PA17
  {"EXTINT2",  "ANALOG", "SERCOM1", "SERCOM3", "TC3",  "TCC0", "?",        "AC/CMP0"},  //PA18
  {"EXTINT3",  "ANALOG", "SERCOM1", "SERCOM3", "TC3",  "TCC0", "?",        "AC/CMP1"},  //PA19
  {"EXTINT4",  "ANALOG", "SERCOM5", "SERCOM3", "TC7",  "TCC0", "I2S/SCK0", "GCLK_IO4"}, //PA20
  {"EXTINT5",  "ANALOG", "SERCOM5", "SERCOM3", "TC7",  "TCC0", "I2S/FS0",  "GCLK_IO5"}, //PA21
  {"EXTINT6",  "ANALOG", "SERCOM3", "SERCOM5", "TC4",  "TCC0", "USB/SOF",  "GCLK_IO6"}, //PA22
  {"EXTINT7",  "ANALOG", "SERCOM3", "SERCOM5", "TC4",  "TCC0", "?",        "GCLK_IO7"}, //PA23
  {"EXTINT12", "?",      "SERCOM3", "SERCOM5", "TC5",  "TCC1", "USB/DM",   "?"},        //PA24
  {"EXTINT13", "?",      "SERCOM3", "SERCOM5", "TC5",  "TCC1", "USB/DP",   "?"},        //PA25
  {"?",        "?",      "?",       "?",       "?",    "?",    "?",        "?"},        //PA26
  {"EXTINT15", "?",      "?",       "?",       "?",    "?",    "?",        "GCLK_IO0"}, //PA27
  {"EXTINT8",  "?",      "?",       "?",       "?",    "?",    "?",        "GCLK_IO0"}, //PA28
  {"?",        "?",      "?",       "?",       "?",    "?",    "?",        "?"},        //PA29
  {"EXTINT10", "?",      "?",       "SERCOM1", "TCC1", "?",    "SWCLK",    "GCLK_IO0"}, //PA30
  {"EXTINT11", "?",      "?",       "SERCOM1", "TCC1", "?",    "SWDIO",    "?"},        //PA31
};

const char * const samdGpioBMux[32][8] =
{//  A          B         C          D          E       F       G           H
 //  0          1         2          3          4       5       6           7
  {"EXTINT0",  "ANALOG", "?",       "SERCOM5", "TC7",  "?",    "?",        "?"},        // PB0
  {"EXTINT1",  "ANALOG", "?",       "SERCOM5", "TC7",  "?",    "?",        "?"},        // PB1
  {"EXTINT2",  "ANALOG", "?",       "SERCOM5", "TC6",  "?",    "?",        "?"},        // PB2
  {"EXTINT3",  "ANALOG", "?",       "SERCOM5", "TC6",  "?",    "?",        "?"},        // PB3
  {"EXTINT4",  "ANALOG", "?",       "?",       "?",    "?",    "?",        "?"},        // PB4
  {"EXTINT5",  "ANALOG", "?",       "?",       "?",    "?",    "?",        "?"},        // PB5
  {"EXTINT6",  "ANALOG", "?",       "?",       "?",    "?",    "?",        "?"},        // PB6
  {"EXTINT7",  "ANALOG", "?",       "?",       "?",    "?",    "?",        "?"},        // PB7
  {"EXTINT8",  "ANALOG", "?",       "SERCOM4", "TC4",  "?",    "?",        "?"},        // PB8
  {"EXTINT9",  "ANALOG", "?",       "SERCOM4", "TC4",  "?",    "?",        "?"},        // PB9
  {"EXTINT10", "?",      "?",       "SERCOM4", "TC5",  "TCC0", "I2S/MCK1", "GCLK_IO4"}, //PB10
  {"EXTINT11", "?",      "?",       "SERCOM4", "TC5",  "TCC0", "I2S/SCK1", "GCLK_IO5"}, //PB11
  {"EXTINT12", "ANALOG", "SERCOM4", "?",       "TC4",  "TCC0", "I2S/FS1",  "GCLK_IO6"}, //PB12
  {"EXTINT13", "ANALOG", "SERCOM4", "?",       "TC4",  "TCC0", "?",        "GCLK_IO7"}, //PB13
  {"EXTINT14", "ANALOG", "SERCOM4", "?",       "TC5",  "?",    "?",        "GCLK_IO0"}, //PB14
  {"EXTINT15", "ANALOG", "SERCOM4", "?",       "TC5",  "?",    "?",        "GCLK_IO1"}, //PB15
  {"EXTINT0",  "?",      "SERCOM5", "?",       "TC6",  "TCC0", "I2S/SD0",  "GCLK_IO2"}, //PB16
  {"EXTINT1",  "?",      "SERCOM5", "?",       "TC6",  "TCC0", "I2S/SD1",  "GCLK_IO3"}, //PB17
  {"?",        "?",      "?",       "?",       "?",    "?",    "?",        "?"},        //PB18
  {"?",        "?",      "?",       "?",       "?",    "?",    "?",        "?"},        //PB19
  {"?",        "?",      "?",       "?",       "?",    "?",    "?",        "?"},        //PB20
  {"?",        "?",      "?",       "?",       "?",    "?",    "?",        "?"},        //PB21
  {"EXTINT6",  "?",      "?",       "SERCOM5", "TC7",  "?",    "?",        "GCLK_IO0"}, //PB22
  {"EXTINT7",  "?",      "?",       "SERCOM5", "TC7",  "?",    "?",        "GCLK_IO1"}, //PB23
  {"?",        "?",      "?",       "?",       "?",    "?",    "?",        "?"},        //PB24
  {"?",        "?",      "?",       "?",       "?",    "?",    "?",        "?"},        //PB25
  {"?",        "?",      "?",       "?",       "?",    "?",    "?",        "?"},        //PB26
  {"?",        "?",      "?",       "?",       "?",    "?",    "?",        "?"},        //PB27
  {"?",        "?",      "?",       "?",       "?",    "?",    "?",        "?"},        //PB28
  {"?",        "?",      "?",       "?",       "?",    "?",    "?",        "?"},        //PB29
  {"EXTINT14", "?",      "?",       "SERCOM5", "TCC0", "TCC1", "?",        "?"},        //PB30
  {"EXTINT15", "?",      "?",       "SERCOM5", "TCC0", "TCC1", "?",        "?"},        //PB31
};

const int samdGpioAPin[32] =
{
    1,  2,  3,  4,  9, 10, 11, 12,
   13, 14, 15, 16, 21, 22, 23, 24,
   25, 26, 27, 28, 29, 30, 31, 32,
   33, 34,  0, 39, 41,  0, 45, 46
};

const int samdGpioBPin[32] =
{
    0,  0,  0,  0,  0,  0,  0,  0,
    7,  8, 19, 20,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0, 37, 38,
    0,  0,  0,  0,  0,  0,  0,  0
};

void * samdGpioValidatePortLetter(char portLetter)
{
  void * gpioRegs;

  //Section 22.7, GPIO registers
  portLetter = toupper(portLetter);
  if ((portLetter < 'A') || (portLetter > 'C'))
  {
    systemPrintln("ERROR - Invalid port letter, needs to be in the range A - C");
    return NULL;
  }

  //Get the base address of the GPIO port register set
  gpioRegs = samdGpioBaseAddress + ((portLetter - 'A') << 7);
  return gpioRegs;
}

uint32_t samdGpioValidatePin(char portLetter, int pinNumber)
{
  uint32_t pinMask;

  //Determine if this is a valid pin number
  if ((pinNumber >= 0) && (pinNumber < 32))
  {
    pinMask = 1 << pinNumber;
    if (samdGpioPorts[portLetter - 'A'] & pinMask)
      return pinMask;
    systemPrint("ERROR - pin ");
    systemPrint(pinNumber);
    systemPrint(" is not supported by port ");
    systemWrite(portLetter);
    systemPrintln();
  }

  //Invalid pin number specified
  systemPrintln("ERROR - Invalid port number, needs to be in the range 0 - 32");
  return 0;
}

int samdGpioDirGet(char portLetter, int pinNumber)
{
  SAMD_GPIO_REGS * gpioRegs;
  uint32_t pinMask;

  //Validate the portLetter
  gpioRegs = (SAMD_GPIO_REGS *)samdGpioValidatePortLetter(portLetter);
  if (!gpioRegs)
    return 0;

  //Validate the pinNumber
  pinMask = samdGpioValidatePin(portLetter, pinNumber);
  if (!pinMask)
    return 0;

  //Section 22.7, DIR register
  return (gpioRegs->DIR & pinMask) ? 1 : 0;
}

int samdGpioDvrstrGet(char portLetter, int pinNumber)
{
  uint8_t * config;
  SAMD_GPIO_REGS * gpioRegs;
  uint32_t pinMask;

  //Validate the portLetter
  gpioRegs = (SAMD_GPIO_REGS *)samdGpioValidatePortLetter(portLetter);
  if (!gpioRegs)
    return 0;

  //Validate the pinNumber
  pinMask = samdGpioValidatePin(portLetter, pinNumber);
  if (!pinMask)
    return 0;

  //Section 22.7, PINCFG register
  config = &gpioRegs->PINCFG0;
  return (config[pinNumber] & 0x40) ? 1 : 0;
}

int samdGpioInGet(char portLetter, int pinNumber)
{
  SAMD_GPIO_REGS * gpioRegs;
  uint32_t pinMask;

  //Validate the portLetter
  gpioRegs = (SAMD_GPIO_REGS *)samdGpioValidatePortLetter(portLetter);
  if (!gpioRegs)
    return 0;

  //Validate the pinNumber
  pinMask = samdGpioValidatePin(portLetter, pinNumber);
  if (!pinMask)
    return 0;

  //Section 22.7, IN register
  return (gpioRegs->IN & pinMask) ? 1 : 0;
}

int samdGpioInenGet(char portLetter, int pinNumber)
{
  uint8_t * config;
  SAMD_GPIO_REGS * gpioRegs;
  uint32_t pinMask;

  //Validate the portLetter
  gpioRegs = (SAMD_GPIO_REGS *)samdGpioValidatePortLetter(portLetter);
  if (!gpioRegs)
    return 0;

  //Validate the pinNumber
  pinMask = samdGpioValidatePin(portLetter, pinNumber);
  if (!pinMask)
    return 0;

  //Section 22.7, PINCFG register
  config = &gpioRegs->PINCFG0;
  return (config[pinNumber] & 2) ? 1 : 0;
}

int samdGpioMuxGet(char portLetter, int pinNumber)
{
  uint8_t * config;
  SAMD_GPIO_REGS * gpioRegs;
  uint32_t pinMask;
  uint8_t * pmux;

  //Validate the portLetter
  gpioRegs = (SAMD_GPIO_REGS *)samdGpioValidatePortLetter(portLetter);
  if (!gpioRegs)
    return 0;

  //Validate the pinNumber
  pinMask = samdGpioValidatePin(portLetter, pinNumber);
  if (!pinMask)
    return 0;

  //Section 22.7, PINCFG register
  config = &gpioRegs->PINCFG0;
  if (config[pinNumber] & 1)
  {
    //Get the MUX value
    pmux = &gpioRegs->PMUX0;
    return (pmux[pinNumber >> 1] >> ((pinNumber & 1) ? 4 : 0)) & 0xf;
  }
  return 0;
}

int samdGpioMuxenGet(char portLetter, int pinNumber)
{
  uint8_t * config;
  SAMD_GPIO_REGS * gpioRegs;
  uint32_t pinMask;
  uint8_t * pmux;

  //Validate the portLetter
  gpioRegs = (SAMD_GPIO_REGS *)samdGpioValidatePortLetter(portLetter);
  if (!gpioRegs)
    return 0;

  //Validate the pinNumber
  pinMask = samdGpioValidatePin(portLetter, pinNumber);
  if (!pinMask)
    return 0;

  //Section 22.7, PINCFG register
  config = &gpioRegs->PINCFG0;
  return config[pinNumber] & 1;
}

int samdGpioOutGet(char portLetter, int pinNumber)
{
  SAMD_GPIO_REGS * gpioRegs;
  uint32_t pinMask;

  //Validate the portLetter
  gpioRegs = (SAMD_GPIO_REGS *)samdGpioValidatePortLetter(portLetter);
  if (!gpioRegs)
    return 0;

  //Validate the pinNumber
  pinMask = samdGpioValidatePin(portLetter, pinNumber);
  if (!pinMask)
    return 0;

  //Section 22.7, OUT register
  return (gpioRegs->OUT & pinMask) ? 1 : 0;
}

int samdGpioPullenGet(char portLetter, int pinNumber)
{
  uint8_t * config;
  SAMD_GPIO_REGS * gpioRegs;
  uint32_t pinMask;

  //Validate the portLetter
  gpioRegs = (SAMD_GPIO_REGS *)samdGpioValidatePortLetter(portLetter);
  if (!gpioRegs)
    return 0;

  //Validate the pinNumber
  pinMask = samdGpioValidatePin(portLetter, pinNumber);
  if (!pinMask)
    return 0;

  //Section 22.7, PINCFG register
  config = &gpioRegs->PINCFG0;
  return (config[pinNumber] & 4) ? 1 : 0;
}

void samdGpioPortConfig(char portLetter)
{
  SAMD_GPIO_REGS * gpioRegs;
  int pinNumber;
  int physicalPin;

  //Validate the portLetter
  gpioRegs = (SAMD_GPIO_REGS *)samdGpioValidatePortLetter(portLetter);
  if (!gpioRegs)
    return;

  //Verify that this port has pins
  if (!samdGpioPorts[portLetter - 'A'])
    return;

  //Display the port name
  for (pinNumber = 0; pinNumber < 32; pinNumber++)
  {
    //Display the pin configuration
    if (samdGpioPorts[portLetter - 'A'] & (1 << pinNumber))
    {
      systemPrint("Port ");
      if (pinNumber <= 9)
        systemWrite(' ');
      systemWrite(portLetter);
      systemPrint(pinNumber);
      systemPrint(" Pin ");
      if (portLetter == 'A')
        physicalPin = samdGpioAPin[pinNumber];
      else
        physicalPin = samdGpioBPin[pinNumber];
      if (physicalPin < 10)
        systemWrite(' ');
      systemPrint(physicalPin);
      if (samdGpioMuxenGet(portLetter, pinNumber))
      {
        systemPrint(" MUX:");
        if (portLetter == 'A')
          systemPrint(samdGpioAMux[pinNumber][samdGpioMuxGet(portLetter, pinNumber)]);
        else
          systemPrint(samdGpioBMux[pinNumber][samdGpioMuxGet(portLetter, pinNumber)]);
        systemWrite(',');
      }
      else
        systemPrint(" GPIO");
      systemPrint(samdGpioDirGet(portLetter, pinNumber) ? " 1(Output)" : "  0(Input)");
      systemPrint(", INEN:");
      systemPrint(samdGpioInenGet(portLetter, pinNumber));
      systemPrint(", PULLEN:");
      systemPrint(samdGpioPullenGet(portLetter, pinNumber));
      systemPrint(", OUT:");
      systemPrint(samdGpioOutGet(portLetter, pinNumber));
      systemPrint(", IN:");
      systemPrint(samdGpioInGet(portLetter, pinNumber));
      systemPrint(", DRVSTR:");
      systemPrintln(samdGpioDvrstrGet(portLetter, pinNumber));
    }
  }
}

#endif  //ARDUINO_ARCH_SAMD
