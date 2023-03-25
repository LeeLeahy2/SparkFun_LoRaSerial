#ifndef __SPRINKLER_CONTROLLER_H__
#define __SPRINKLER_CONTROLLER_H__

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

//Increasing above 4 requires adding support for second quad relay board
//Increasing above 8 requires more relay boards an changing ZONE_MASK type
#define ZONE_NUMBER_MAX     4   //0 = No zone (off), 1 - 8 = Zone number

#define PC_WATER_USE    THIRD_PARTY_RESP  //Read the water use

//------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------

typedef uint8_t ZONE_T;           //0 = No zone (off), bit # + 1: 1 - 8 = Zone number
#define ZONE_MASK       ((1 << ZONE_NUMBER_MAX) - 1)

typedef struct _CONTROLLER_SCHEDULE
{
  uint32_t scheduleStartTime;     //Schedule start time offset in milliseconds
  uint32_t zoneScheduleDuration[ZONE_NUMBER_MAX]; //Scheduled duration for the zone
} CONTROLLER_SCHEDULE;

typedef struct _WATER_USE
{
  uint32_t total;                 //Total water use for this controller
  uint32_t leaked;                //Water leaked while zones are off
  uint32_t zone[ZONE_NUMBER_MAX]; //Water usage per zone
} WATER_USE;

#endif  //__SPRINKLER_CONTROLLER_H__
