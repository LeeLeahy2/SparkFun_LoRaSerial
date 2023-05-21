/**********************************************************************
* Copyright 2022 Lee Leahy (lpleahyjr@gmail.com)
* All rights reserved
*
* WeatherStation.h
*
* Include file that provides definitions for the WeatherStation code.
**********************************************************************/

#ifndef __WEATHER_STATION_H__
#define __WEATHER_STATION_H__

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <mariadb/mysql.h> //Install libmariadb-dev
#include <netinet/in.h>
#include <poll.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "../LoRaSerial/Sprinkler_Controller.h"
#include "../LoRaSerial/Virtual_Circuit_Protocol.h"
#include "../LoRaSerial/Weather_Station_Protocol.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//Defines
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

#define POLL_TIMEOUT_USEC       50 * 1000

#define UNKNOWN_VALUE           -1

#define WEB_SITE_OWNER          "lee"
#define BAUD_RATE               B57600

#define READ_TIMEOUT            250     //Milliseconds

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//Macros
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

#define ARRAY_SIZE(x)       (sizeof(x) / sizeof(x[0]))

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//Globals
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

extern gid_t gid;
int radio;
fd_set readfds;
extern uid_t uid;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//Forward Declarations
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//WeatherStation.c
void closeWeatherStation();
int initWeatherStation();
int openLoRaSerial(const char * terminal);
void rainStatus(VC_SERIAL_MESSAGE_HEADER * header, uint8_t length);
int updateWeatherStation();
void windStatus(VC_SERIAL_MESSAGE_HEADER * header, uint8_t length);

#endif  //__WEATHER_STATION_H__
