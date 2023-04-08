/**********************************************************************
* Copyright 2022 Lee Leahy (lpleahyjr@gmail.com)
* All rights reserved
*
* SprinklerServer.c
*
* Program to log data from the SparkFun weather station.
**********************************************************************/

#include "WeatherStation.h"

gid_t gid;
bool gotFirstRainValue;
bool gotFirstWindValue;
time_t now;
uint32_t previousRainCount;
uint32_t previousWindCount;
uint32_t rainCount;
int rainFile;
char responseBuffer[4096];
struct tm * timeCurrent;
struct tm timePrevious;
uid_t uid;
int8_t unknownData[60 * 60 * 24];
uint32_t windCount;
int windFile;

int createPath(const char * path)
{
  char character;
  char directoryPath[256];
  int offset;
  int status;

  offset = 0;
  status = 0;
  while (1)
  {
    //Check for end-of-string
    if (!path[offset])
      break;

    //Check for a directory separator
    character = path[offset];
    directoryPath[offset] = character;
    offset += 1;
    if ((offset <= 5) || (character != '/'))
      continue;

    //Zero terminate the directory name
    directoryPath[offset] = 0;

    //Attempt to create the directory
    status = mkdir(directoryPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    //Skip directories that exist
    if ((status == -1) && (errno == EEXIST))
    {
      status = 0;
      continue;
    }

    //Return an error if the directory create fails
    else if (status == -1)
    {
      status = errno;
      fprintf(stderr, "ERROR: Failed to create the directory %s!\n", directoryPath);
      break;
    }

    //The directory was successfully created, set the ownership
    status = chown(directoryPath, uid, gid);
    if (status)
    {
      status = errno;
      fprintf(stderr, "ERROR: Failed to set owner for %s!", directoryPath);
      break;
    }
  }
  return status;
}

int openSensorFile(const char * sensorName)
{
  int day;
  int file;
  char fileName[256];
  int month;
  int year;

  //Get the file name
  year = timeCurrent->tm_year + 1900;
  month = timeCurrent->tm_mon + 1;
  day = timeCurrent->tm_mday;
  sprintf(fileName,
          "/var/www/html/WeatherData/%4d/%02d/%s_%04d-%02d-%02d.txt",
          year, month, sensorName, year, month, day);

  //Create the directory path if necessary
  if (createPath(fileName))
    return -1;

  //Attempt to open the existing file
  file = open(fileName, O_RDWR);
  if (file >= 0)
    return file;

  //Attempt to create the file
  file = open(fileName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (file < 0)
  {
    perror(NULL);
    fprintf(stderr, "ERROR: Failed to open the %s sensor file!\n", sensorName);
  }
  return file;
}

int openLoRaSerial(const char * terminal)
{
  struct termios tty;

  //Wait for the radio
  printf("Waiting for the radio...\n");
  do
  {
    radio = open(terminal, O_RDWR | O_NOCTTY);

    //Give the radio some time to initialize
    sleep(1);
  }
  while (radio < 0);

  //Get the terminal characteristics
  if (tcgetattr(radio, &tty) < 0)
  {
    perror("ERROR: Failed to get the terminal characteristics");
    return 1;
  }

  cfmakeraw(&tty);

  //Set the baudrate for the terminal
  if (cfsetspeed(&tty, BAUD_RATE))
  {
    perror("ERROR: Failed to set the baudrate");
    return 2;
  }

  //Set the proper baud rate and characteristics for the radio
  if (tcsetattr(radio, TCSANOW, &tty) < 0)
  {
    perror("ERROR: Failed to set the terminal characteristics");
    return 3;
  }

  //Delay a while to let the radio complete its reset operation
  sleep(2);

  //Successfully connected to the radio
  return 0;
}

int fillSensorFiles()
{
  int bytesWritten;
  int deltaBytes;
  off_t fileLength;
  int length;
  int status;

  //Get the current time
  now = time(NULL);
  timeCurrent = localtime(&now);
  timePrevious = *timeCurrent;

  //Determine the amount of unknown data
  length = timeCurrent->tm_hour;
  length = (length * 60) + timeCurrent->tm_min;
  length = (length * 60) + timeCurrent->tm_sec;

  //Get the length of the rain file
  fileLength = lseek(rainFile, 0, SEEK_END);
  if (fileLength == (off_t)-1)
  {
    status = errno;
    perror("ERROR: Failed to get the length of the rain file!");
    return status;
  }

  //Write the unknowns to the rain file
  deltaBytes = length - fileLength;
  bytesWritten = write(rainFile, unknownData, deltaBytes);
  if (bytesWritten != deltaBytes)
  {
    perror("ERROR: Failed writing unknowns to rain file");
    return errno;
  }

  //Get the length of the wind file
  fileLength = lseek(windFile, 0, SEEK_END);
  if (fileLength == (off_t)-1)
  {
    status = errno;
    perror("ERROR: Failed to get the length of the wind file!");
    return status;
  }

  //Write the unknowns to the rain file
  deltaBytes = length - fileLength;
  bytesWritten = write(windFile, unknownData, deltaBytes);
  if (bytesWritten != deltaBytes)
  {
    perror("ERROR: Failed writing unknowns to wind file");
    return errno;
  }
  return 0;
}

int getUidAndGid(const char * userName)
{
  struct passwd * entry;

  //Walk the password file to locate the specified user
  do
  {
    entry = getpwent();
    if (strcmp(entry->pw_name, userName) == 0)
    {
      uid = entry->pw_uid;
      gid = entry->pw_gid;
      return 0;
    }
  } while(1);

  //The user was not found
  return -1;
}


void rainStatus(VC_SERIAL_MESSAGE_HEADER * header, uint8_t length)
{
  uint32_t count;
  char * data;

  data = (char *)&header[1];

  //Parse the rain count
  if (sscanf(data, "R%08x\r\n", &count) != 1)
  {
    fprintf(stderr, "ERROR: Failed to parse rain data!\r\n");
    return;
  }
  rainCount = count;
  if (!gotFirstRainValue)
    previousRainCount = rainCount;
  gotFirstRainValue = true;
}

void windStatus(VC_SERIAL_MESSAGE_HEADER * header, uint8_t length)
{
  uint32_t count;
  char * data;

  data = (char *)&header[1];

  //Parse the wind count
  if (sscanf(data, "W%08x\r\n", &count) != 1)
  {
    fprintf(stderr, "ERROR: Failed to parse rain data!\r\n");
    return;
  }
  windCount = count;
  if (!gotFirstWindValue)
    previousWindCount = windCount;
  gotFirstWindValue = true;
}

int initWeatherStation()
{
  int status;

  //Get the user's UID and GID
  if (getUidAndGid(WEB_SITE_OWNER))
  {
    fprintf(stderr,
            "ERROR: Failed to get UID and GID for the web site owner %s\n",
            WEB_SITE_OWNER);
    return -1;
  }

  //Initialize the unknown buffer
  memset(unknownData, UNKNOWN_VALUE, sizeof(unknownData));

  //Get the current time
  now = time(NULL);
  timeCurrent = localtime(&now);

  //Open the rain file
  rainFile = openSensorFile("Rain");
  if (rainFile < 0)
    return rainFile;

  //Open the wind file
  windFile = openSensorFile("Wind");
  if (windFile < 0)
    return windFile;

  //Fill the files with unknown values
  status = fillSensorFiles();
  if (status)
    return status;
  return 0;
}

int updateWeatherStation()
{
  int bytesWritten;
  int8_t deltaCount;

  //Update the files every second
  if (memcmp(&timePrevious, timeCurrent, sizeof(timePrevious)) != 0)
  {
    timePrevious = *timeCurrent;

    //Open the files if necessary
    if (rainFile < 0)
    {
      rainFile = openSensorFile("Rain");
      if (rainFile < 0)
        return 1;
    }

      //Open the wind file
    if (windFile < 0)
    {
      windFile = openSensorFile("Wind");
      if (windFile < 0)
        return 1;
    }

    //Get the rain sensor value
    deltaCount = rainCount - previousRainCount;
    if (!gotFirstRainValue)
      deltaCount = UNKNOWN_VALUE;
    previousRainCount = rainCount;
    bytesWritten = write(rainFile, &deltaCount, sizeof(deltaCount));
    if (bytesWritten != sizeof(deltaCount))
    {
      perror("ERROR: Write to rain file failed!");
      return errno;
    }

    //Get the wind sensor value
    deltaCount = windCount - previousWindCount;
    if (!gotFirstWindValue)
      deltaCount = UNKNOWN_VALUE;
    previousWindCount = windCount;
    bytesWritten = write(windFile, &deltaCount, sizeof(deltaCount));
    if (bytesWritten != sizeof(deltaCount))
    {
      perror("ERROR: Write to wind file failed!");
      return errno;
    }

    //Check for a new day
    if ((timeCurrent->tm_hour == 23) && (timeCurrent->tm_min == 59)
      && (timeCurrent->tm_sec == 59))
    {
      //Close the current files
      close(windFile);
      windFile = -1;
      close(rainFile);
      rainFile = -1;
    }
  }
  return 0;
}

void closeWeatherStation()
{
  //Close the sensor files
  if (windFile)
    close(windFile);
  if (rainFile)
    close(rainFile);
}
