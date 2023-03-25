/*
  January 16th, 2023
  Lee Leahy

  Sprinkler controller support routines.
*/

#define DC_VOLTAGE_OUTPUT_PORT        0
#define MAX_H_BRIDGE_RETRY_COUNT      25

void sprinklerControllerBegin()
{
  Wire.begin(); //Start I2C

  //Verify connection to quad relay board
  if(quadRelay.begin())
    online.quadRelay = true;

  hBridgeBegin(); //Start the H-bridge

  flowSensorBegin(); //Start the flow sensor

  beginDisplay(); //Start display first to be able to display any errors

  //Turn off all the zones
  turnOffAllZones();
}

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
  static bool previousEnableSprinklerControlller;

  if (!online.quadRelay)
    quadRelayOffline();

  //Check for disabling the sprinkler controller
  if ((!enableSprinklerController) && previousEnableSprinklerControlller)
  {
    turnOffAllZones();
  }
  previousEnableSprinklerControlller = enableSprinklerController;

  //Debounce the flow control switch and count the gallons of water
  flowSwitchDebounce();

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
  else if (enableSprinklerController && (scheduleActive || scheduleReady))
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
  else if (enableSprinklerController)
    updateSprinklerSchedule(waterToday);
}

void quadRelayOffline()
{
  //Report error back to the server
  //Delay for an hour
  //Reboot
}

uint8_t zoneMaskToZoneNumber(ZONE_T zoneMask)
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

void turnOnRelay(ZONE_T zoneMask)
{
  ZONE_T previousZoneActive;

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
void turnOnZone(ZONE_T zoneMask)
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
  {
    //Output the negative pulse
    hBridgeOutputVoltage(false);
    turnOnRelay(zoneActive);
  }
  else
    turnOffRelay();
}

void turnOffAllZones()
{
  int zone;
  ZONE_T latching;
  ZONE_T on;

  //Save the latching state
  latching = latchingSolenoid;
  on = zoneManualOn;

  //Turn off all the zones
  zoneManualOn = 0;
  for (zone = 0; zone < ZONE_NUMBER_MAX; zone++)
  {
    //Turn off this zone
    zoneActive = 1 << zone;
    zoneManualOn = 1 << zone;
    latchingSolenoid = 1 << zone;
    turnOffZone();

    //Wait for the zone to go off
    while (zoneActive)
    {
      petWDT();
      updateZones();
    }
  }

  //Update the sprinkler controller state
  zoneManualOn = on;
  zoneManualPreviousOn = 0;
  latchingSolenoid = latching;

  //Restart the scheduler
  scheduleActive = false;
  scheduleReady = false;
  scheduleCopied = false;
  memset((void *)&today, 0, sizeof(today));
}

void turnOffRelay()
{
  uint8_t zone;

  //Get the zone number
  zone = zoneMaskToZoneNumber(zoneActive);

  //Turn off the relay
  if (online.quadRelay)
    quadRelay.turnRelayOff(zoneNumber);

  //Turn off the H-bridge
  if (latchingSolenoid & zoneActive)
    hBridgeSetDrive(false, true);

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

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

static bool flowDetected;
static uint32_t flowSensorLastInterruptTime;

#define FLOW_TIMEOUT                20 //Milliseconds

//Initializee the interrupt for the flow meter
void flowSensorBegin()
{
  //Flow sensor - Detect wind switch closure
  //
  //  5V                         ______________
  //  0V  ____________________|||              |||______________________
  //                         Noise            Noise
  //
  pinMode(pin_FlowSensor, INPUT);
  attachInterrupt(digitalPinToInterrupt(pin_FlowSensor), flowSensorIsr, RISING);
}

//Handle the flow meter interrupt
void flowSensorIsr()
{
  //Start the debounce timer for the flow control switch
  flowSensorLastInterruptTime = millis();
}

//Debounce the flow sensor switch
void flowSwitchDebounce()
{
  uint8_t inputValue;
  uint8_t zoneNumber;

  inputValue = digitalRead(pin_FlowSensor);

  //Wait until the debounce time is complete
  if ((millis() - flowSensorLastInterruptTime) >= FLOW_TIMEOUT)
  {
    //Determine the new state of the flow control switch
    if ((!flowDetected) && inputValue)
    {
      //Account for this gallon of flow
      gallons.total++;
      if (zoneActive)
      {
        //Bill this gallon to the corresponding zone
        zoneNumber = zoneMaskToZoneNumber(zoneActive);
        gallons.zone[zoneNumber - 1]++;
      }
      else
        gallons.leaked++;
      flowDetected = true;
      online.flowMeter = true;

      //Pulse the LED when flow is detected
      blinkFlowLed(true);
    }
    else if (flowDetected && (!inputValue))
      flowDetected = false;
  }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//Initialize the H-bridge
void hBridgeBegin()
{
  uint8_t retryCount;

  //Configure the driver for I2C operation with the default I2C address
  hBridge.settings.commInterface = I2C_MODE;
  hBridge.settings.I2CAddress = 0x5D;

  //Initialize the H-bridge driver
  if (settings.debugHBridge)
    systemPrintln("Initializing the H-bridge");

  retryCount = 0;
  while ((retryCount++ <= MAX_H_BRIDGE_RETRY_COUNT) && (hBridge.begin() != 0xA9))
  {
    petWDT();
    delay(100);
  }

  //Check for initialization failure
  if (retryCount > MAX_H_BRIDGE_RETRY_COUNT)
  {
    online.hBridge = false;
    if (settings.debugHBridge)
      systemPrintln("H-Briget offline - DC latching solenoids are not supported");

    //DC latching solenoids are not supported by this sprinkler controller
    latchingSolenoid = 0;
    return;
  }

  if (settings.debugHBridge)
    systemPrintln("Waiting for H-bridge driver to finish initialization");
  while (!hBridge.ready())
    petWDT();

  //By default don't invert the output
  hBridgeWaitNotBusy();
  hBridge.inversionMode(DC_VOLTAGE_OUTPUT_PORT, 0);

  //Enable the H-bridge
  hBridgeEnable(true);

  //By default apply negative voltage for off pulse
  hBridgeOutputVoltage(false);

  //The H-bridge is available to switch the polarity for DC latching solenoids
  online.hBridge = true;
  if (settings.debugHBridge)
    systemPrintln("H-bridge is online");
}

//Wait for the SCMD to be ready to accept another command
void hBridgeWaitNotBusy()
{
  if (online.hBridge)
  {
    if (settings.debugHBridge)
      systemPrintln("Waiting for SCMD to accept command");
    while (hBridge.busy())
      petWDT();
  }
}

//Enable/disable H-bridge output
void hBridgeEnable(bool enable)
{
  if (online.hBridge)
  {
    //Wait for the H-bridge to accept a command
    hBridgeWaitNotBusy();

    //Enable or disable the voltage output
    if (settings.debugHBridge)
    {
      systemPrint("H-bridge ");
      systemPrint(enable ? "en" : "dis");
      systemPrintln("abling voltage output");
    }
    if (enable)
      hBridge.enable();
    else
      hBridge.disable();
  }
}

//Set the H-bridge drive level
void hBridgeSetDrive(bool on, bool positiveVoltage)
{
  if (online.hBridge)
  {
    hBridgeEnable(true);

    //Wait for the H-bridge to accept a command
    hBridgeWaitNotBusy();

    //Select the output voltage
    if (settings.debugHBridge)
    {
      if (on)
      {
        systemPrint("H-bridge selecting ");
        systemPrint(positiveVoltage ? "positive" : "negative");
        systemPrintln(" voltage");
      }
      else
        systemPrintln("H-bridge turning voltage off");
    }
    hBridge.setDrive(DC_VOLTAGE_OUTPUT_PORT, positiveVoltage ? 0 : 1, on ? 255 : 0);

    //Remember the last voltage setting
    if (on)
      hBridgeLastVoltage = positiveVoltage ? 1 : -1;
    else
      hBridgeLastVoltage = 0;
  }
}

//Enable H-bridge output
void hBridgeOutputVoltage(bool positiveVoltage)
{
  if (online.hBridge)
  {
    //Turn off the voltage
    hBridgeSetDrive(false, true);

    //Turn on the new voltage
    hBridgeSetDrive(true, positiveVoltage);
  }
}

void updateSprinklerSchedule(bool waterToday)
{
  int zone;

  if (waterToday && enableSprinklerController
    && ((!scheduleCopied) || ((dayOfWeek == scheduleDay) && (timeOfDay < today.scheduleStartTime))))
  {
    //Set the day during which the schedule is being processed
    scheduleDay = dayOfWeek;
    scheduleCopied = true;
    if (timeOfDay < week[dayOfWeek].scheduleStartTime)
    {
      //Copy the schedule
      today = week[dayOfWeek];
      scheduleReady = true;
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
}
