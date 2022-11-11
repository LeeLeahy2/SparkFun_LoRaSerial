/**********************************************************************
* Copyright 2022 Lee Leahy (lpleahyjr@gmail.com)
* All rights reserved
*
* SprinklerServer.c
*
* Program to log data from the SparkFun weather station.
**********************************************************************/

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
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

#include "../LoRaSerial_Firmware/Virtual_Circuit_Protocol.h"

#define UNKNOWN_VALUE           -1

#define WEB_SITE_OWNER          "lee"
#define RADIO                   "/dev/ttyACM0"
#define BAUD_RATE               B57600

#define READ_TIMEOUT            250     //Milliseconds

#define INPUT_BUFFER_SIZE       128
#define OUTPUT_BUFFER_SIZE      4096

#define STDIN                   0
#define STDOUT                  1
#define STDERR                  2

#define MAX_MESSAGE_SIZE      128

#define PC_RAIN_STATUS      MAX_VC
#define PC_WIND_STATUS      (PC_RAIN_STATUS + 1)

uid_t uid;
gid_t gid;
uint8_t inputBuffer[1 + 3 + INPUT_BUFFER_SIZE];
uint8_t outputBuffer[OUTPUT_BUFFER_SIZE];
int myVcAddr;
int remoteVcAddr;
int radio;
fd_set readfds;
struct tm * timeCurrent;
struct tm timePrevious;
time_t now;

char responseBuffer[4096];
uint32_t rainCount;
uint32_t previousRainCount;
uint32_t windCount;
uint32_t previousWindCount;
bool gotFirstRainValue;
bool gotFirstWindValue;

int rainFile;
int windFile;

int8_t unknownData[60 * 60 * 24];

void dumpBuffer(uint8_t * data, int length)
{
  char byte;
  int bytes;
  uint8_t * dataEnd;
  uint8_t * dataStart;
  const int displayWidth = 16;
  int index;

  dataStart = data;
  dataEnd = &data[length];
  while (data < dataEnd)
  {
    // Display the offset
    printf("    0x%02x: ", (unsigned int)(data - dataStart));

    // Determine the number of bytes to display
    bytes = dataEnd - data;
    if (bytes > displayWidth)
      bytes = displayWidth;

    // Display the data bytes in hex
    for (index = 0; index < bytes; index++)
      printf(" %02x", *data++);

    // Space over to the ASCII display
    for (; index < displayWidth; index++)
      printf("   ");
    printf("  ");

    // Display the ASCII bytes
    data -= bytes;
    for (index = 0; index < bytes; index++) {
      byte = *data++;
      printf("%c", ((byte < ' ') || (byte >= 0x7f)) ? '.' : byte);
    }
    printf("\n");
  }
}

int stdinToRadio()
{
  int bytesRead;
  int bytesSent;
  int bytesToSend;
  int maxfds;
  int status;
  struct timeval timeout;
  uint8_t * vcData;

  //Read the console input data into the local buffer.
  vcData = inputBuffer;
  bytesRead = read(STDIN, &inputBuffer[1 + 3], INPUT_BUFFER_SIZE);
  if (bytesRead < 0)
  {
    perror("ERROR: Read from stdin failed!");
    status = bytesRead;
  }
  else
  {
    //Adjust bytesRead to account for the VC header
    vcData[0] = START_OF_VC_SERIAL;
    vcData[1] = bytesRead + 3;
    vcData[2] = remoteVcAddr;
    vcData[3] = (remoteVcAddr == VC_COMMAND) ? PC_COMMAND : myVcAddr;
    bytesRead += 1 + 3;

    //Send this data over the VC
    bytesSent = 0;
    status = 0;
    while (bytesSent < bytesRead)
    {
      //Break up the data if necessary
      bytesToSend = bytesRead - bytesSent;
      if (bytesToSend > MAX_MESSAGE_SIZE)
        bytesToSend = MAX_MESSAGE_SIZE;

      //Send the data
      bytesToSend = write(radio, &vcData[bytesSent], bytesRead - bytesSent);
      if (bytesToSend <= 0)
      {
        perror("ERROR: Write to radio failed!");
        status = errno;
        break;
      }

      //Account for the bytes written
      bytesSent += bytesToSend;
    }
  }
  return status;
}

int radioToStdout()
{
  int bytesRead;
  int bytesWritten;
  uint32_t count;
  uint8_t * data;
  uint8_t * dataEnd;
  uint8_t * dataStart;
  uint8_t vcDest;
  int length;

  //Read the virtual circuit header into the local buffer.
  bytesRead = read(radio, outputBuffer, OUTPUT_BUFFER_SIZE);
  if (bytesRead <= 0)
  {
    perror("ERROR: Read from radio failed!");
    return errno;
  }

  dataStart = outputBuffer;
  dataEnd = &dataStart[bytesRead];
  do
  {
    //Locate the start of the virtual circuit data
    data = dataStart;
    while ((data < dataEnd) && (*data != START_OF_VC_SERIAL))
      data++;

    //Just print any non-virtual-circuit data
    length = data - dataStart;
    if (length)
    {
      bytesWritten = write(STDOUT, dataStart, length);
      if (bytesWritten != length)
      {
        perror("ERROR: Failed to write non-VC data");
        return errno;
      }
    }
    dataStart = &data[1];

    if (data < dataEnd)
    {
      //Move the message to the beginning of the buffer
      length = dataEnd - dataStart;
      memcpy(outputBuffer, dataStart, length);
      data = outputBuffer;
      dataStart = data;

      //Make sure that we have the length byte
      if (!length)
      {
        dataStart = &data[length];
        bytesRead = read(radio, dataStart, 1);
        if (bytesRead <= 0)
        {
          perror("ERROR: Read from radio failed!");
          return errno;
        }
        dataStart++;
        length = 1;
      }

      //Determine if more data is necessary
      if (*data > length)
      {
        dataEnd = &data[*data];
        dataStart = &data[length];

        //Read in the rest of the message
        bytesRead = 0;
        while (dataEnd > dataStart)
        {
          length = read(radio, dataStart, dataEnd - dataStart);
          if (length <= 0)
          {
            perror("ERROR: Read from radio failed!");
            return errno;
          }
          dataStart += length;
        }
      }
      dataStart = data;
      dataEnd = &data[length];

      //Process this message
      length = *data;
      if (length > 3)
      {
        vcDest = data[1];
        if (vcDest == PC_RAIN_STATUS)
        {
          if (sscanf((char *)&outputBuffer[3], "R%08x\r\n", &count) != 1)
          {
            fprintf(stderr, "ERROR: Failed to parse rain data!\r\n");
            return -1;
          }
          rainCount = count;
          if (!gotFirstRainValue)
            previousRainCount = rainCount;
          gotFirstRainValue = true;
        }
        if (vcDest == PC_WIND_STATUS)
        {
          if (sscanf((char *)&outputBuffer[3], "W%08x\r\n", &count) != 1)
          {
            fprintf(stderr, "ERROR: Failed to parse rain data!\r\n");
            return -1;
          }
          windCount = count;
          if (!gotFirstWindValue)
            previousWindCount = windCount;
          gotFirstWindValue = true;
        }
      }
      dataStart += length;
    }
  } while (dataStart < dataEnd);
  return 0;
}

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

int main(int argc, char **argv)
{
    int bytesWritten;
    fd_set currentfds;
    int8_t deltaCount;
    int maxfds;
    int numfds;
    int status;
    struct termios tty;
    struct timeval timeout;

    //Display the help text if necessary
    if (argc != 4)
    {
      printf("%s   target_VC\n", argv[0]);
      printf("\n");
      printf("target_VC:\n");
      printf("    Client: 1 - %d\n", MAX_VC - 1);
      printf("    Loopback: %d\n", VC_SERVER);
      printf("    Broadcast: %d\n", VC_BROADCAST);
      printf("    Command: %d\n", VC_COMMAND);
      return -1;
    }

    //Determine the remote VC address
    if ((sscanf(argv[1], "%d", &remoteVcAddr) != 1)
      || (remoteVcAddr < VC_COMMAND) || (remoteVcAddr >= MAX_VC))
    {
      fprintf(stderr, "ERROR: Invalid target VC address, please use one of the following:\n");
      if (myVcAddr)
        fprintf(stderr, "    Server: 0\n");
      fprintf(stderr, "    Client: 1 - %d\n", MAX_VC - 1);
      fprintf(stderr, "    Loopback: my_VC\n");
      fprintf(stderr, "    Broadcast: %d\n", VC_BROADCAST);
      fprintf(stderr, "    Command: %d\n", VC_COMMAND);
      return -1;
    }

    //Get the user's UID and GID
    if (getUidAndGid(WEB_SITE_OWNER))
    {
      fprintf(stderr,
              "ERROR: Failed to get UID and GID for the web site owner %s\n",
              WEB_SITE_OWNER);
      return -1;
    }

    //Wait for the radio
    printf("Waiting for the radio...\n");
    maxfds = STDIN;
    do
    {
        radio = open(RADIO, O_RDWR | O_NOCTTY);

        //Give the radio some time to initialize
        sleep(1);
    }
    while (radio < 0);

    if (maxfds < radio)
      maxfds = radio;

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

    //Initialize the fd_sets
    FD_ZERO(&readfds);
    FD_SET(STDIN, &readfds);
    FD_SET(radio, &readfds);

    //Set the timeout
    timeout.tv_sec = 0;
    timeout.tv_usec = 50 * 1000;
    printf("Waiting for VC data...\n");
    while (1)
    {
      //Wait for receive data or timeout
      memcpy((void *)&currentfds, (void *)&readfds, sizeof(readfds));
      numfds = select(maxfds + 1, &currentfds, NULL, NULL, &timeout);
      if (numfds < 0)
      {
        perror("ERROR: select call failed!");
        status = errno;
        break;
      }

      //Determine if console input is available
      if (FD_ISSET(STDIN, &currentfds))
      {
        //Send the console input to the radio
        status = stdinToRadio();
        if (status)
          break;
      }

      if (FD_ISSET(radio, &currentfds))
      {
        //Write the radio data to console output
        status = radioToStdout();
        if (status)
          break;
      }

      //Update the files every second
      now = time(NULL);
      timeCurrent = localtime(&now);
      if (memcmp(&timePrevious, timeCurrent, sizeof(timePrevious)) != 0)
      {
        timePrevious = *timeCurrent;

        //Open the files if necessary
        if (rainFile < 0)
        {
          rainFile = openSensorFile("Rain");
          if (rainFile < 0)
            break;
        }

          //Open the wind file
        if (windFile < 0)
        {
          windFile = openSensorFile("Wind");
          if (windFile < 0)
            break;
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
          status = errno;
          break;
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
          status = errno;
          break;
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
    }

    //Close the sensor files
    if (windFile)
      close(windFile);
    if (rainFile)
      close(rainFile);

    //Done with the radio
    close(radio);
    radio = -1;
    return status;
}
