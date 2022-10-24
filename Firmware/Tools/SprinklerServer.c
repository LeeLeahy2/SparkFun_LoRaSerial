#include "settings.h"

#define BUFFER_SIZE           2048
#define MAX_MESSAGE_SIZE      32
#define STDIN                 0
#define STDOUT                1
#define START_OF_HEADING      0x01      //From ASCII table

struct tm * timeCurrent;
uint8_t inputBuffer[BUFFER_SIZE + 3];
uint8_t outputBuffer[BUFFER_SIZE + 3];
uint8_t tempBuffer[60 * 60];

int radioToVc()
{
  int bytesRead;
  int bytesSent;
  int bytesToSend;
  int dataBytes;
  int8_t destAddr;
  uint8_t length;
  int maxfds;
  int status;
  int8_t srcAddr;
  struct timeval timeout;
  uint8_t * vcData;

  status = 0;
  do
  {
    //Display any debug data on the console
    while (1)
    {
      bytesRead = read(tty, outputBuffer, 1);
      if (bytesRead < 0)
      {
        perror("ERROR: Read from radio failed!");
        status = bytesRead;
        break;
      }
      if (outputBuffer[0] == START_OF_HEADING)
        break;
      printf("%c", outputBuffer[0]);
    }

    //Read the virtual circuit header into the local buffer.
    vcData = outputBuffer;
    bytesRead = sizeof(outputBuffer);
    bytesRead = read(tty, outputBuffer, bytesRead);
    if (bytesRead < 0)
    {
      perror("ERROR: Read from radio failed!");
      status = bytesRead;
      break;
    }

    //Display the VC header
    length = vcData[0];
    destAddr = vcData[1];
    srcAddr = vcData[2];
    printf("length: %d\n", length);
    printf("destAddr: %d\n", destAddr);
    printf("srcAddr: %d\n", srcAddr);

    //Read the message
    bytesRead = 0;
    vcData = outputBuffer;
    while (bytesRead < length)
    {
      dataBytes = read(STDIN, &outputBuffer[bytesRead], length - bytesRead);
      if (dataBytes < 0)
      {
        perror("ERROR: Read from radio failed!");
        status = errno;
        break;
      }
    }

    //Dispatch this message
    switch (destAddr)
    {
    default:
      break;
    }
  } while (0);
  return status;
}

int addUnknownBytes(int file)
{
  size_t bytesToWrite;
  size_t bytesWritten;
  size_t dataBytes;
  size_t fileSize;
  time_t now;
  int status;

  status = 0;
  fileSize = lseek(file, 0, SEEK_END);
  now = time(NULL);
  timeCurrent = localtime(&now);
  bytesToWrite = (((timeCurrent->tm_hour * 60) + timeCurrent->tm_min) * 60)
               + timeCurrent->tm_sec + 1;
  bytesToWrite -= fileSize;
  do
  {
    dataBytes = bytesToWrite;
    if (dataBytes <= 0)
      break;
    if (dataBytes > sizeof(tempBuffer))
      dataBytes = sizeof(tempBuffer);
    bytesWritten = write(file, tempBuffer, dataBytes);
    if (bytesWritten < dataBytes)
    {
      perror("ERROR:Failed to write to rain file!");
      status = errno;
      break;
    }
    bytesToWrite -= dataBytes;
  } while (bytesToWrite > 0);
  return status;
}

int openSensorFile(const char * sensorName)
{
  int file;
  char fileName[64];

  //Get the file name
  sprintf(fileName, "%s_%04d-%02d-%02d.txt", sensorName, timeCurrent->tm_year + 1900,
          timeCurrent->tm_mon + 1, timeCurrent->tm_mday);
  file = open(fileName, O_CREAT | O_RDWR | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (file < 0)
    perror("ERROR: Failed to open the rain sensor file!");
  return file;
}

int
main (
  int argc,
  char ** argv
)
{
  int bytesToWrite;
  int fileSize;
  bool handleTimeout;
  int maxfds;
  time_t now;
  int rainFile;
  int status;
  struct timeval timeout;
  struct tm timePrevious;
  uint8_t * vcData;
  int windFile;

  status = 0;
  rainFile = -1;
  windFile = -1;
  do
  {
    //Display the help text if necessary
    if (argc != 2)
    {
      printf("%s   terminal\n", argv[0]);
      printf("\n");
      printf("terminal - Name or path to the terminal device for the radio\n");
      status = -1;
      break;
    }

    //Initialize the buffer of unknown bytes
    memset(tempBuffer, -1, sizeof(tempBuffer));

    //Get the current time
    now = time(NULL);
    timeCurrent = localtime(&now);

    //Open the rain file
    rainFile = openSensorFile("Rain");
    if (rainFile < 0)
      break;

    //Open the wind file
    windFile = openSensorFile("Wind");
    if (windFile < 0)
      break;

    //Add unknown values to the files
    do
    {
      timePrevious = *timeCurrent;
      status = addUnknownBytes(rainFile);
      if (status)
        break;
      addUnknownBytes(windFile);
      if (status)
        break;
    } while (memcmp(&timePrevious, timeCurrent, sizeof(timePrevious)) != 0);
    if (status)
      break;

    maxfds = STDIN;
#if 0
    //Open the terminal
    status = openTty(argv[1]);
    if (status)
    {
      perror("ERROR: Failed to open the terminal!");
      break;
    }
    if (maxfds < tty)
      maxfds = tty;
#endif  //0

    //Initialize the fd_sets
    FD_ZERO(&exceptfds);
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

//    handleTimeout = false;
    memset(&timeout, 0, sizeof(timeout));
    while (1)
    {
      //Set the timeout
      timeout.tv_sec = 0;
      timeout.tv_usec = 1000;

      //Wait for receive data or timeout
      FD_SET(STDIN, &readfds);
//      FD_SET(tty, &readfds);
      status = select(maxfds + 1, &readfds, &writefds, &exceptfds, &timeout);

/*
      if (FD_ISSET(tty, &readfds))
      {
        //Write the radio data to console output
        status = radioToStdout();
        if (status)
          break;
      }

      //Timeout
      else if (handleTimeout)
*/
      {
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

          //Get the weather sensor values
printf("Read sensor\n");

          //Write the values to the current file

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
    }
  } while (0);

  //Done with the files
  if (windFile >= 0)
    close(windFile);
  if (rainFile >= 0)
    close(rainFile);

  return status;
}
