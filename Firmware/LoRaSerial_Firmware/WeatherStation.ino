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
    windCount++;
  }
}

void weatherStationUpdate()
{
  uint32_t count;
  uint32_t currentTime;
  int entries;
  int index;
  uint8_t inputValue;
  static uint32_t lastRainUpdate;
  static uint32_t lastWindCount;
  int maxValue;
  int minValue;
  int total;
  double value;

  //Pulse the LED when rain is detected
  rxLED(rainDetected);

  //Pulse the LED when wind is detected
  txLED(windDetected);

  //Clear the rain interrupt after a while
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
    windDetected = false;
//systemPrintln(windCount);
}

  //Update the rain amounts every minute
  currentTime = millis();
  if ((currentTime - lastRainUpdate) >= 60 * 1000)
  {
    lastRainUpdate = currentTime;

    //Start the next entry
    entries = sizeof(rainCount) / sizeof(rainCount[0]);
    rainIndex = (rainIndex + 1) % entries;
    rainCount[rainIndex] = 0;

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
displayRainFall();
  }

  //Compute the wind speed on a regular basis
  currentTime = millis();
  if ((currentTime - lastPoll) >= POLL_INTERVAL)
  {
    count = windCount;
    windSpeedKmPerHr = (2.4 * 1000. * (windCount - lastWindCount)) / (currentTime - lastPoll);
    windSpeedMiPerHr = windSpeedKmPerHr * 1.49129;
    lastPoll = currentTime;
    lastWindCount = count;
systemPrint("Wind speed: ");
systemPrint((float)windSpeedMiPerHr, 2);
systemPrint(" mi/hr (");
systemPrint((float)windSpeedKmPerHr, 2);
systemPrintln(" km/hr)");
  }
}

void displayRainFall()
{
  systemPrint("Rain (in/hr) Max: ");
  systemPrint(maxRainFall, 2);
  systemPrint(", Ave: ");
  systemPrint(aveRainFall, 2);
  systemPrint(", Min: ");
  systemPrintln(minRainFall, 2);
}


