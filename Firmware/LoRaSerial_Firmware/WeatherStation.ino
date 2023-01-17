#include "Weather_Station_Protocol.h"
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//Local variables - Weather Station
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

static bool rainDetected;
static uint32_t rainSensorLastInterruptTime;

static uint32_t lastPoll;

#define POLL_INTERVAL     (10 * 1000)
#define RAIN_TIMEOUT      7

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

  //Pulse the LED when rain is detected
  blinkRainLed(true);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

void weatherStationUpdate()
{
  uint32_t currentTime;
  int entries;
  bool forceUpdate;
  int index;
  uint8_t inputValue;
  static uint32_t lastRainUpdate;
  int maxValue;
  int minValue;
  static uint32_t previousRainCount;
  int total;
  double value;

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

  //Check for rain detected
  if (forceUpdate || (previousRainCount != rainCountTotal))
  {
    //Send a rain status message
    previousRainCount = rainCountTotal;
    sprintf(tempBuffer, "R%08x\r\n", rainCountTotal);
    systemWrite(START_OF_VC_SERIAL);      //Start byte
    systemWrite(3 + strlen(tempBuffer));  //Length
    systemWrite(PC_RAIN_STATUS);          //Destination
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

