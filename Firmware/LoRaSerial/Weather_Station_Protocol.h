#ifndef __WEATHER_STATION_PROTOCOL_H__
#define __WEATHER_STATION_PROTOCOL_H__

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

//Virtual-Circuit source and destination index values
#define PC_RAIN_STATUS      (THIRD_PARTY_RESP + 1)
#define PC_WIND_STATUS      (PC_RAIN_STATUS + 1)

//------------------------------------------------------------------------------
// Protocol Exchanges
//------------------------------------------------------------------------------
/*
Host Interaction using Virtual-Circuits

       Host A                   LoRa A

                  <---- Rain Status
                  <---- Wind Status

*/
#endif  //__WEATHER_STATION_PROTOCOL_H__
