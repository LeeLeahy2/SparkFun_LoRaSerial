//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//Global variables - Rain Sensor
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

uint32_t rainSensorTicks;
uint32_t rainSensorPreviousTicks;

//Pulse recording
static char rainSensorTempBuffer[32];
static uint8_t rainSensorTempBufferOffset;
static uint32_t rainSensorLastInterruptTime;
static uint32_t rainSensorPulseStartMillis;
static uint32_t rainSensorPulseEndMillis;

//Last pulse data
static uint32_t rainSensorPulseDeltaTime;
static char rainSensorPulseBuffer[32];
static uint32_t rainSensorPulseStartTime;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

void rainSensorBegin()
{
  //Rain sensor - Detect changes in the level of the rain sensor switch
  //
  //  5V  ____________________                    ______________________
  //  0V                      |||______________|||
  //                         Noise            Noise
  //
  pinMode(pin_RainSensor, INPUT);
  attachInterrupt(digitalPinToInterrupt(pin_RainSensor), rainSensorIsr, CHANGE);
}

void rainSensorIsr()
{
  uint32_t currentTime;
  static bool rainDetected;
  bool inputValue;

  //Debounce the noise from the rain sensor reed switch
  currentTime = millis();
  if (!rainSensorLastInterruptTime)
    //Record the start time of the pulse
    rainSensorPulseStartMillis = currentTime;
  else
  {
    //Record the end time of the pulse
    rainSensorPulseEndMillis = currentTime;

    //Determine if the middle of the pulse has passed
    if ((millis() - rainSensorLastInterruptTime) >= 2)
    {
      //Indicate rain detected
      rainCount++;

      //Show the switch closed time
      rainSensorTempBuffer[rainSensorTempBufferOffset++] = '_';
      rainSensorTempBuffer[rainSensorTempBufferOffset++] = '_';
      rainSensorTempBuffer[rainSensorTempBufferOffset++] = '.';
      rainSensorTempBuffer[rainSensorTempBufferOffset++] = '.';
      rainSensorTempBuffer[rainSensorTempBufferOffset++] = '.';
      rainSensorTempBuffer[rainSensorTempBufferOffset++] = '_';
      rainSensorTempBuffer[rainSensorTempBufferOffset++] = '_';
    }
  }
  rainSensorLastInterruptTime = currentTime;

  //Get the reed switch value
  inputValue = digitalRead(pin_RainSensor);
  rainDetected = !inputValue;

  //Record this interrupt
  rainSensorTempBuffer[rainSensorTempBufferOffset++] = '0' + inputValue;
  rainSensorTempBuffer[rainSensorTempBufferOffset] = 0;

  //Indicate the pulse
  rxLED(rainDetected);
}

void rainSensorPulsePrint()
{
  systemPrint("Rain sensor pulse @ ");
  systemPrintln(rainSensorPulseStartTime);

  //Display the pulse duration in milliseconds
  if (rainSensorPulseDeltaTime < 100)
    systemWrite(' ');
  if (rainSensorPulseDeltaTime < 10)
    systemWrite(' ');
  systemPrint(rainSensorPulseDeltaTime);
  systemPrint(" mSec");

  //Display the idle time
  systemPrint("  ____");
  for (int i = 0; i < strlen(rainSensorPulseBuffer); i++)
    systemWrite(' ');
  systemPrintln("____");

  //Display the pulse with the noise
  systemPrint("              ");
  systemPrintln(rainSensorPulseBuffer);
}

void rainSensorUpdate()
{
  //Draw the pulse if requested
  if (rainSensorLastInterruptTime && ((millis() - rainSensorLastInterruptTime) >= 200))
  {
    //Save the last pulse data
    rainSensorPulseStartTime = rainSensorPulseStartMillis;
    rainSensorPulseDeltaTime = rainSensorPulseEndMillis - rainSensorPulseStartMillis;
    strcpy(rainSensorPulseBuffer, rainSensorTempBuffer);

rainSensorPulsePrint();

    //Initialize pulse recording
    rainSensorLastInterruptTime = 0;
    rainSensorTempBufferOffset = 0;
  }
}
