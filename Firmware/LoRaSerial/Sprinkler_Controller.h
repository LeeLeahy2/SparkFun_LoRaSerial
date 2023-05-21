#ifndef __SPRINKLER_CONTROLLER_H__
#define __SPRINKLER_CONTROLLER_H__

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

//Increasing above 2 requires more relay boards and changing ZONE_MASK type
#define MAX_RELAY_BOARDS    2   //Number of relay boards
#define RELAYS_ON_BOARD     4   //Number of relays on the board

#define ZONE_NUMBER_MAX     (MAX_RELAY_BOARDS * RELAYS_ON_BOARD) //0 = No zone (off), 1 - 8 = Zone number

#define ZONE_TO_RELAY(zone) (((zone - 1) % RELAYS_ON_BOARD) + 1)
#define ZONE_TO_BOARD(zone) ((zone - 1) / RELAYS_ON_BOARD)

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
