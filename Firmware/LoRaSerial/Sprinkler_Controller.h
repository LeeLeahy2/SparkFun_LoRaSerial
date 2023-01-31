#ifndef __SPRINKLER_CONTROLLER_H__
#define __SPRINKLER_CONTROLLER_H__

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

//Increasing above 4 requires adding support for second quad relay board
//Increasing above 8 requires more relay boards an changing ZONE_MASK type
#define ZONE_NUMBER_MAX     4   //0 = No zone (off), 1 - 8 = Zone number

//------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------

typedef uint8_t ZONE_MASK;      //0 = No zone (off), bit # + 1: 1 - 8 = Zone number

typedef struct _CONTROLLER_SCHEDULE
{
  uint32_t scheduleStartTime;     //Schedule start time offset in milliseconds
  uint32_t zoneScheduleDuration[ZONE_NUMBER_MAX]; //Scheduled duration for the zone
} CONTROLLER_SCHEDULE;

#endif  //__SPRINKLER_CONTROLLER_H__
