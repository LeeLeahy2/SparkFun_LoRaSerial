//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//Local variables - Weather Station
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

static bool rainDetected;
static uint32_t rainSensorLastInterruptTime;
static bool windDetected;
static uint32_t windSensorLastInterruptTime;

static uint32_t lastPoll;

#define POLL_INTERVAL     (10 * 1000)
#define RAIN_TIMEOUT      7
#define WIND_TIMEOUT      7

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
  attachInterrupt(digitalPinToInterrupt(pin_RainSensor), rainSensorIsr, FALLING);
}

void rainSensorIsr()
{
  //Remember the last time the interrupt fired
  if (!rainDetected)
  {
    rainSensorLastInterruptTime = millis();
    rainDetected = true;
  }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

void windSensorBegin()
{
  //Wind sensor - Detect wind switch closure
  //
  //  5V  ____________________                    ______________________
  //  0V                      |||______________|||
  //                         Noise            Noise
  //
  pinMode(pin_WindSensor, INPUT);
  attachInterrupt(digitalPinToInterrupt(pin_WindSensor), windSensorIsr, FALLING);
}

void windSensorIsr()
{
  //Remember the last time the interrupt fired
  if (!windDetected)
  {
    windSensorLastInterruptTime = millis();
    windDetected = true;
  }
}

void weatherStationUpdate()
{
  uint32_t currentTime;
  int entries;
  bool forceUpdate;
  int index;
  uint8_t inputValue;
  static uint32_t lastRainUpdate;
  static uint32_t lastWindUpdate;
  int maxValue;
  int minValue;
  static uint32_t previousRainCount;
  static uint32_t previousWindCount;
  int total;
  double value;

  //Pulse the LED when rain is detected
  rxLED(rainDetected);

  //Pulse the LED when wind is detected
  txLED(windDetected);

  //Clear the rain interrupt after a while
  forceUpdate = false;
  inputValue = digitalRead(pin_RainSensor);
  if (rainDetected && inputValue && ((millis() - rainSensorLastInterruptTime) >= RAIN_TIMEOUT))
  {
    //Update the rain count
    rainCountTotal++;
    rainCount[rainIndex]++;
    rainDetected = false;
  }

  //Clear the wind interrupt after a while
  inputValue = digitalRead(pin_WindSensor);
  if (windDetected && inputValue && ((millis() - windSensorLastInterruptTime) >= WIND_TIMEOUT))
  {
    windCountTotal++;
    windCount[windIndex]++;
    windDetected = false;
  }

  //Update the rain amounts every minute
  currentTime = millis();
  if ((currentTime - lastRainUpdate) >= 60 * 1000)
  {
    lastRainUpdate = currentTime;
    forceUpdate = true;

    //Start the next entry
    entries = sizeof(rainCount) / sizeof(rainCount[0]);
    rainIndex = (rainIndex + 1) % entries;

    //Locate the maximum and minimum values
    maxValue = 0;
    minValue = 255;
    total = 0;
    for (index = 0; index < entries; index++)
    {
      total += rainCount[index];
      if (maxValue < rainCount[index])
        maxValue = rainCount[index];
      if (minValue > rainCount[index])
        minValue = rainCount[index];
    }

    //Compute the maximum rain fall
    value = ((double)maxValue) * 0.010984252;
    maxRainFall = (float)value;

    //Compute the minimum rain fall
    value = ((double)minValue) * 0.010984252;
    minRainFall = (float)value;

    //Compute the average rain fall
    value = ((double)total) * 0.010984252 / ((double)entries);
    aveRainFall = value;

    //Zero the next interval
    rainCount[rainIndex] = 0;
  }

  //Update the wind amounts every second
  if ((currentTime - lastWindUpdate) >= 1000)
  {
    lastWindUpdate = currentTime;

    //Start the next entry
    entries = sizeof(windCount) / sizeof(windCount[0]);
    windIndex = (windIndex + 1) % entries;

    //Locate the maximum and minimum values
    maxValue = 0;
    minValue = 255;
    total = 0;
    for (index = 0; index < entries; index++)
    {
      total += windCount[index];
      if (maxValue < windCount[index])
        maxValue = windCount[index];
      if (minValue > windCount[index])
        minValue = windCount[index];
    }

    //Compute the maximum wind gust
    value = ((double)maxValue) * 1.49129;
    maxWindSpeed = (float)value;

    //Compute the minimum wind gust
    value = ((double)minValue) * 1.49129;
    minWindSpeed = (float)value;

    //Compute the average wind speed
    value = ((double)total) * 1.49129 / ((double)entries);
    aveWindSpeed = value;

    //Zero the next interval
    windCount[windIndex] = 0;
  }

  //Check for rain detected
  if (forceUpdate || (previousRainCount != rainCountTotal))
  {
    //Send a rain status message
    previousRainCount = rainCountTotal;
    sprintf(tempBuffer, "R%08x\r\n", rainCountTotal);
    systemWrite(START_OF_HEADING);        //Start byte
    systemWrite(3 + strlen(tempBuffer));  //Length
    systemWrite(PC_RAIN_STATUS);          //Destination
    systemWrite(myVc);                    //Source
    systemPrint(tempBuffer);
  }

  //Check for wind detected
  if (forceUpdate || (previousWindCount != windCountTotal))
  {
    //Send a wind status message
    previousWindCount = windCountTotal;
    sprintf(tempBuffer, "W%08x\r\n", windCountTotal);
    systemWrite(START_OF_HEADING);        //Start byte
    systemWrite(3 + strlen(tempBuffer));  //Length
    systemWrite(PC_WIND_STATUS);          //Destination
    systemWrite(myVc);                    //Source
    systemPrint(tempBuffer);
  }
}

void displayRainFall()
{
  systemPrint("Rain (in/hr) Max: ");
  systemPrint(maxRainFall, 4);
  systemPrint(", Ave: ");
  systemPrint(aveRainFall, 4);
  systemPrint(", Min: ");
  systemPrintln(minRainFall, 4);
}

void displayWindSpeed()
{
  systemPrint("Wind Speed (mi/hr) Max: ");
  systemPrint(maxWindSpeed, 2);
  systemPrint(", Ave: ");
  systemPrint(aveWindSpeed, 2);
  systemPrint(", Min: ");
  systemPrintln(minWindSpeed, 2);
}
