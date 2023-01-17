/*
  January 16th, 2023
  Lee Leahy

  Sprinkler controller support routines.
*/

void adjustTimeOfDay()
{
  //Ensure that the previousTimestampOffset is non-zero
  //Adjust so that zero is added to time on the next update unless timestampOffset
  //is updated first
  if (virtualCircuitList[VC_SERVER].vcState >= VC_STATE_LINK_ALIVE)
  {
    if (timestampOffset == 1)
      previousTimestampOffset = 2;
    else
      previousTimestampOffset = 1;
    timeOfDay -= previousTimestampOffset;
    previousTimestampOffset = timestampOffset - previousTimestampOffset;
  }

  //Make sure that the correct schedule gets copied
  if ((!scheduleActive) && (!zoneOnDuration))
    scheduleCopied = false;
}

void updateTimeOfDay()
{
  uint32_t currentTime;
  uint32_t offset;
  static uint32_t previousTime;

  //Adjust clocks based upon the updated timestampOffset
  if (!previousTimestampOffset)
    previousTimestampOffset = timestampOffset;
  offset = timestampOffset - previousTimestampOffset;
  previousTimestampOffset = timestampOffset;

  //Update the time
  currentTime = millis();
  timeOfDay += currentTime + offset - previousTime;
  previousTime = currentTime;
  if (timeOfDay >= MILLISECONDS_IN_A_DAY)
  {
    dayOfWeek = (dayOfWeek + 1) % 7;
    timeOfDay -= MILLISECONDS_IN_A_DAY;
    scheduleCopied = false;
  }
}
