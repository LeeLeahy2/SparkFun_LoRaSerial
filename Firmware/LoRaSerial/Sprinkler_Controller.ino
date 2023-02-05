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
  {
    scheduleReady = false;
    scheduleCopied = false;
  }
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

/*
        Day
    Sun  |  Mon
  1 -----       -------------------------------------------------- scheduleCopied
  0      \_____/:
                :
  1           __:
  0 _________/  \_________________________________________________ waterToday
                :
  1             :---
  0 ____________/   \_____________________________________________ scheduleReady
                    :
                    :
                    : today.scheduleStartTime
                    :
  1                 :------------------------------------------
  0 ________________/:                                         \__ scheduleActive
                     :
  1                  :__
  0 _________________/  \_________________________________________ today.zoneScheduleDuration 1
                     :  :
  1                  :__:_________
  0 _________________/  :         \_______________________________ today.zoneScheduleDuration 2
                     :  :         :
                     :  :         :              Zone 3
                     :  :         :             Suspended
  1                  :__:_________:_________       ____
  0 _________________/  :         :         \_____/   :\__________ today.zoneScheduleDuration 3
                     :  :         :         :     :   ::
  1                  :  :         :         :     :   ::
  0 ____________________:_________:_________:_____:___::__________ today.zoneScheduleDuration 4
                        :         :         :     :   ::
  1                     :-------  :-------  :---  :   ::-----
  0 ____________________/   1   \_/   2   \_/ 3 \_:___:/  3  \____ zoneOnDuration
                                                  :   :
  1                                               :---:
  0 ______________________________________________/   \___________ manualOn

*/

void updateZones()
{
  uint32_t currentTime;
  uint32_t deltaTime;
  bool waterToday;
  int zone;

  if (!online.quadRelay)
    quadRelayOffline();

  //Verify that the previous schedule has completed.  If completed, check for
  //a watering schedule for today
  waterToday = false;
  for (zone = 0; zone < ZONE_NUMBER_MAX; zone++)
  {
    //Verify that the previous watering schedule was completed
    if (today.zoneScheduleDuration[zone])
    {
      waterToday = false;
      break;
    }

    //Determine if watering is necessary today
    if (week[dayOfWeek].zoneScheduleDuration[zone])
      waterToday = true;
  }

  //----------------------------------------
  //First finish turning on or off any of the latching relays
  //----------------------------------------
  currentTime = millis();
  if (pulseDuration)
  {
    //When the pulse duration is reached, turn off the relay
    if ((currentTime - pulseStartTime) >= pulseDuration)
      turnOffRelay();
  }

  //----------------------------------------
  //Second, process any change in manual on/off
  //----------------------------------------
  else if (zoneManualOn || (zoneManualPreviousOn != zoneManualOn))
  {
    if (zoneManualPreviousOn != zoneManualOn)
    {
      //Turn off the previous zone first
      if (zoneManualPreviousOn)
      {
        //Reduce the zone's watering schedule
        zone = zoneNumber - 1;
        if (today.zoneScheduleDuration[zone])
        {
          deltaTime = currentTime - onTime;
          today.zoneScheduleDuration[zone] = (today.zoneScheduleDuration[zone] < deltaTime)
                                           ? 0 : today.zoneScheduleDuration[zone] - deltaTime;
        }

        //Turn off this zone
        turnOffZone();
      }
      else
      {
        //No manual operation is active, if a scheduled operation is in progress
        //suspend the scheduled operation.
        if (zoneOnDuration)
        {
          //Remember the remaining time
          zone = zoneNumber - 1;
          deltaTime = zoneOnDuration - (currentTime - onTime);
          today.zoneScheduleDuration[zone] = deltaTime;

          //Turn off this zone
          turnOffZone();
        }
        else
        {
          //Turn on the zone
          turnOnZone(zoneManualOn);

          //Remember the state change
          zoneManualPreviousOn = zoneManualOn;
        }
      }
      zoneOnDuration = 0;
    }
  }

  //----------------------------------------
  //Third, turn off the zone
  //----------------------------------------

  else if (zoneOnDuration)
  {
    if ((currentTime - onTime) >= zoneOnDuration)
    {
      //Turn off the zone
      turnOffZone();
      zoneOnDuration = 0;
    }
  }

  //----------------------------------------
  //Fourth, execute the schedule
  //----------------------------------------
  else if (scheduleActive || scheduleReady)
  {
    if (scheduleActive || (timeOfDay >= today.scheduleStartTime))
    {
      zoneOnDuration = 0;
      if (scheduleReady)
      {
        //The start time has passed, switch to actively executing the schedule
        scheduleReady = false;
        if (settings.debugSprinklers)
        {
          systemPrintTimestamp(timeOfDay);
          systemPrint(": Starting ");
          systemPrint(dayName[dayOfWeek]);
          systemPrintln("'s schedule");
        }
      }
      for (zone = 0; zone < ZONE_NUMBER_MAX; zone++)
      {
        //Start this zone
        if (today.zoneScheduleDuration[zone])
        {
          //Turn on the zone
          turnOnZone(1 << zone);

          //Only water this zone once
          zoneOnTime = timeOfDay;
          zoneOnDuration = today.zoneScheduleDuration[zone];
          today.zoneScheduleDuration[zone] = 0;
          scheduleActive = true;

          //Debug the schedule
          if (settings.debugSprinklers)
          {
            systemPrintTimestamp(timeOfDay);
            systemPrint(": Watering zone ");
            systemPrint(zone + 1);
            systemPrint(" for ");
            systemPrintTimestamp(zoneOnDuration);
            systemPrintln();
          }
          break;
        }
      }

      //Done if watering is done for all of the zones
      if (!zoneOnDuration)
        scheduleActive = false;
    }
  }

  //----------------------------------------
  //Last, copy the schedule
  //----------------------------------------
  else if (waterToday && (!scheduleCopied) && enableSprinklerController)
  {
    //Copy the schedule
    today = week[dayOfWeek];
    scheduleReady = true;
    scheduleCopied = true;
    if (settings.debugSprinklers)
    {
      systemPrintTimestamp(timeOfDay);
      systemPrint(": Copied ");
      systemPrint(dayName[dayOfWeek]);
      systemPrintln("'s schedule to the active schedule");
      systemPrintTimestamp(timeOfDay);
      systemPrint("Start Time: ");
      systemPrintTimestamp(today.scheduleStartTime);
      systemPrintln();
      for (zone = 0; zone < ZONE_NUMBER_MAX; zone++)
      {
        //Start this zone
        if (today.zoneScheduleDuration[zone])
        {
          //Display the schedule
          if (settings.debugSprinklers)
          {
            systemPrintTimestamp(timeOfDay);
            systemPrint(": Zone ");
            systemPrint(zone + 1);
            systemPrint(": ");
            systemPrintTimestamp(today.zoneScheduleDuration[zone]);
            systemPrintln();
          }
        }
      }
    }
  }
}

void quadRelayOffline()
{
  //Report error back to the server
  //Delay for an hour
  //Reboot
}

uint8_t zoneMaskToZoneNumber(ZONE_MASK zoneMask)
{
  uint8_t zone;

  //Determine which zone is enabled
  if (zoneMask)
    for (zone = 0; zone < ZONE_NUMBER_MAX; zone++) {
      if (zoneMask & (1 << zone))
        return zone + 1;
    }

  //No zone enabled
  return 0;
}

void turnOnRelay(ZONE_MASK zoneMask)
{
  ZONE_MASK previousZoneActive;

  //Get the zone number: 1 - 8
  zoneNumber = zoneMaskToZoneNumber(zoneMask);

  //Set the active zone
  previousZoneActive = zoneActive;
  zoneActive = 1 << (zoneNumber - 1);

  //Update the display
  if (latchingSolenoid & zoneActive)
    relayOn |= zoneActive;
  if (!previousZoneActive)
    zoneOn |= zoneActive;

  //Output the debug messages
  if (settings.debugSprinklers)
  {
    //Update the relay status
    if (!previousZoneActive)
      systemPrintln("--------------------------------------------------");
    systemPrintTimestamp(timeOfDay);
    systemPrint(" Relay ");
    systemPrint(zoneNumber);
    systemPrint(" ON driving ");
    systemPrint((latchingSolenoid & zoneActive) ? "DC latching" : "AC");
    systemPrintln(" solenoid");

    if (!previousZoneActive)
    {
      //Update the zone status
      systemPrintTimestamp(timeOfDay);
      systemPrint(" Zone ");
      systemPrint(zoneNumber);
      systemPrintln(" turning ON");
    }
  }

  //Turn on the relay
  if (online.quadRelay)
    quadRelay.turnRelayOn(zoneNumber);

  //Determine the solenoid type and pulse duration
  pulseDuration = 0;
  if (latchingSolenoid & zoneActive)
  {
    //A latching solenoid is in use, apply power for a short pulse.
    pulseDuration = settings.pulseDuration;
    pulseStartTime = millis();
  }
}

//AC solenoids require power during the entire time the zone is on
//
//                 ON                                OFF
//                  __________________________________               24V
//AC ______________/                                  \______________ 0V
//
//DC latching solenoids need a short pulse to turn on/off the zone
//
//                 ON
//                  ___                                OFF             9V
//DC ______________/   \______________________________     __________  0V
//                                                    \___/           -9V

//Turn on the zone
void turnOnZone(ZONE_MASK zoneMask)
{
  //Use a positive pulse for the DC latching solenoids
  if (latchingSolenoid & zoneMask)
    hBridgeOutputVoltage(true);

  //Turn on specified relay
  onTime = millis();
  turnOnRelay(zoneMask);
}


//Turn off the zone
void turnOffZone()
{
  //Determine the solenoid type and pulse duration
  zoneManualPreviousOn = 0;
  turnWaterOff = true;

  //Update the display
  zoneOn &= ~zoneActive;

  //Turn off the zone
  if (latchingSolenoid & zoneActive)
    turnOnRelay(zoneActive);
  else
    turnOffRelay();
}

void turnOffRelay()
{
  uint8_t zone;

  //Get the zone number
  zone = zoneMaskToZoneNumber(zoneActive);

  //Turn off the relay
  if (online.quadRelay)
    quadRelay.turnRelayOff(zoneNumber);

  //Update the display
  relayOn &= ~zoneActive;

  //Output the debug messages
  if (settings.debugSprinklers)
  {
    if (turnWaterOff)
    {
      //Update the zone status
      systemPrintTimestamp(timeOfDay);
      systemPrint(" Zone ");
      systemPrint(zone);
      systemPrintln(" turning OFF");
    }

    //Update the relay status
    systemPrintTimestamp(timeOfDay);
    systemPrint(" Relay ");
    systemPrint(zone);
    systemPrintln(" OFF");
  }

  //Get ready for the next relay operation
  if (turnWaterOff)
  {
    turnWaterOff = false;
    zoneNumber = 0;
    zoneActive = 0;
    onTime = 0;
  }
  pulseDuration = 0;
  pulseStartTime = 0;
}
