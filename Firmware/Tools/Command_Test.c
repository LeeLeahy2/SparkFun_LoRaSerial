/**********************************************************************
* Copyright 2022 Lee Leahy (lpleahyjr@gmail.com)
* All rights reserved
*
* Command_Test.c
*
* Test program to send a schedule to the sprinkler controller.
**********************************************************************/

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define START_OF_HEADING        1

#define RADIO                   "/dev/ttyACM0"
#define BAUD_RATE               B57600

#define ENTER_COMMAND_MODE      "+++\r"
#define OK_RESPONSE             "OK"
#define ERROR_RESPONSE          "ERROR"
#define READ_TIMEOUT            250     //Milliseconds

#define INPUT_BUFFER_SIZE       128
#define OUTPUT_BUFFER_SIZE      4096

#define STDIN                   0
#define STDOUT                  1
#define STDERR                  2

#define MAX_MESSAGE_SIZE      128

//Virtual-Circuit source and destination index values
#define MAX_VC              8
#define VC_SERVER           0
#define VC_BROADCAST        -1
#define VC_UNASSIGNED       -2

//Source and destinations reserved for the local radio
#define VC_COMMAND          -3    //Command input

//Source and destinations reserved for the local host
#define PC_COMMAND          -10   //Command input and command response
#define PC_LINK_STATUS      -11   //Asynchronous link status output

uint8_t inputBuffer[1 + 3 + INPUT_BUFFER_SIZE];
uint8_t outputBuffer[OUTPUT_BUFFER_SIZE];
int myVcAddr;
int remoteVcAddr;
int radio;
fd_set exceptfds;
fd_set readfds;
fd_set writefds;

char responseBuffer[4096];

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

const char * schedule[] =
{
    //Reset to factor settings
//    "rt&f",

    //Save the settings
//    "rt&w",

    //Clear the previous schedule
    "rtc",

    //Set the time: Thursday 20:00:00 (8 PM)
    "rts254=3",
    "rts253=72000000",

    //Schedule
    //  Thursday @ 20:00:05 (8:00:15 PM)
    "rts252=3",
    "rts249=72015000",
    //      Zone 1 AC solenoid: 1 second
    "rts251=1",
    "rts247=0",
    "rts250=1000",
    //      Zone 2 DC latching solenoid: 2 seconds
    "rts251=2",
    "rts247=1",
    "rts250=2000",
    //      Zone 3 DC latching solenoid: 3 seconds
    "rts251=3",
    "rts247=1",
    "rts250=3000",
    //      Zone 4 AC solenoid: 4 seconds
    "rts251=4",
    "rts247=0",
    "rts250=4000",

    //Display the schedule
    "rti19",

    //Enable the schedule
    "rts248=1",
};

size_t readWithTimeout(int radio)
{
    ssize_t bytesRead;
    ssize_t length;
    struct pollfd pollData= { .fd = radio, .events = POLLIN };
    ssize_t totalBytes;

    totalBytes = 0;
    length = sizeof(responseBuffer) - 1;
    while (poll (&pollData, 1, READ_TIMEOUT) == 1)
    {
        //Read the response data
        bytesRead = read (radio, &responseBuffer[totalBytes], length);
        if (bytesRead < 0)
        {
            perror("ERROR - Read failure!");
            return -22;
        }

        //Account for the data received
        totalBytes += bytesRead;
        length -= bytesRead;
        if (!length)
            break;
    }

    // TODO: IsTimeout = true;
    return totalBytes;
}

int commandModeResponse(int radio)
{
    ssize_t bytesRead;
    char * response;
    char * string;

    while (1)
    {
        //There are two types of responses
        //1. OK\n - Radio was not initialized and just entered command mode
        //2. +++\n ERROR\n - Radio already initialized

        //Read the response from entering command mode
        bytesRead = readWithTimeout(radio);
        if (bytesRead < 0)
            return bytesRead;
        if (bytesRead == 0)
        {
            fprintf(stderr, "ERROR - Command response not found!");
            return 25;
        }

        //Walk the lines in the response
        string = responseBuffer;
        do
        {
            //Locate the new line
            response = string;
            while (*string && (*string != '\n') && (*string != '\r'))
                string++;

            //Replace the new line with end-of-string
            if ((*string == '\n') && (*string != '\r'))
                *string++ = 0;

            //Echo the radio's response
            if (*response)
                printf("%s\n", response);

            //Determine the response
            if ((strcmp(response, OK_RESPONSE) == 0)
                || (strcmp(response, ERROR_RESPONSE) == 0))
                return 0;
        } while (*string);
    }
}

int enterCommandMode(int radio)
{
    ssize_t bytesWritten;
    ssize_t length;
    int status;

    //Enter command mode
    length = strlen(ENTER_COMMAND_MODE);
    bytesWritten = write(radio, ENTER_COMMAND_MODE, length);
    if (bytesWritten < 0)
    {
        perror("ERROR - Failed to enter command mode!");
        return errno;
    }
    if (bytesWritten != length)
    {
        fprintf(stderr, "ERROR - Write failed, tried to write %d bytes, only wrote %d bytes\n",
                (uint32_t)length, (uint32_t)bytesWritten);
        return 10;
    }

    //Get the response
    status = commandModeResponse(radio);
    return status;
}

int commandResponse(int radio)
{
    ssize_t bytesRead;
    char * response;
    char * string;

    do
    {
        //There are two types of responses
        //1. OK\n - Radio was not initialized and just entered command mode
        //2. +++\n ERROR\n - Radio already initialized

        //Read the response from entering command mode
        bytesRead = readWithTimeout(radio);
        if (bytesRead < 0)
            return bytesRead;
        if (bytesRead == 0)
        {
            fprintf(stderr, "ERROR - Command timeout!\n");
            return 35;
        }

        //Walk the lines in the response
        string = responseBuffer;
        do
        {
            //Locate the new line
            response = string;
            while (*string && (*string != '\n') && (*string != '\r'))
                string++;

            //Replace the new line with end-of-string
            if ((*string == '\n') || (*string == '\r'))
                *string++ = 0;

            //Echo the radio's response
            if (*response)
                printf("%s\n", response);

            //Determine the response
            if (strcmp(response, OK_RESPONSE) == 0)
                return 0;
        } while (*string && (strcmp(response, ERROR_RESPONSE) != 0));
    } while (strcmp(response, ERROR_RESPONSE) != 0);
    return 30;
}

int downloadSchedule(int radio)
{
    ssize_t bytesWritten;
    const char * command;
    static char commandBuffer[256];
    int entry;
    ssize_t length;
    int status;

    status = 0;
    for (entry = 0; entry < (int)(sizeof(schedule) / sizeof(schedule[0])); entry++)
    {
        do
        {
            //Get the command
            command = schedule[entry];
            strcpy(commandBuffer, command);
            strcat(commandBuffer, "\r");
            length = strlen(commandBuffer);
            bytesWritten = write(radio, commandBuffer, length);
            if (bytesWritten < 0)
            {
                perror("ERROR - Failed to write the command!");
                return errno;
            }
            if (bytesWritten != length)
            {
                fprintf(stderr, "ERROR - Command write failed, tried to write %d bytes, only wrote %d bytes\n",
                        (uint32_t)length, (uint32_t)bytesWritten);
                return 40;
            }

            //Determine if the command was successful
            status = commandResponse(radio);
        }
        while (status);
    }
    return status;
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
    vcData[0] = START_OF_HEADING;
    vcData[1] = bytesRead + 3;
//    vcData[2] = remoteVcAddr;
//    vcData[3] = myVcAddr;
    vcData[2] = VC_COMMAND;
    vcData[3] = PC_COMMAND;
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
  uint8_t * data;
  uint8_t * dataEnd;
  uint8_t * dataStart;
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
    while ((data < dataEnd) && (*data != START_OF_HEADING))
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
        bytesWritten = write(STDOUT, &outputBuffer[3], length - 3);
        if (bytesWritten != (length - 3))
        {
          perror("ERROR: Failed to write VC message");
          return errno;
        }
      }
      dataStart += length;
    }
  } while (dataStart < dataEnd);
  return 0;
}

int main(int argc, char **argv)
{
    int maxfds;
    int status;
    struct termios tty;
    struct timeval timeout;

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

    printf("Waiting for VC data...\n");
    if (maxfds < radio)
      maxfds = radio;

    //Get the terminal characteristics
    if (tcgetattr(radio, &tty) < 0)
    {
        perror("ERROR - Failed to get the terminal characteristics");
        return 1;
    }

    cfmakeraw(&tty);
/*
    //Clear canonical mode to be able to specify timeouts, ignore CR, disable XON/XOFF
    tty.c_iflag &= ~(IGNCR | IXON | IXOFF | IXANY);

    //Don't echo input
    tty.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHOCTL | ECHOPRT | ECHOKE | ICANON);

    //Set read timeout to 0.1 seconds
    tty.c_cc[VTIME] = 1;

    //Set minimum input
    tty.c_cc[VMIN] = 255;
*/

    //Set the baudrate for the terminal
    if (cfsetspeed(&tty, BAUD_RATE))
    {
        perror("ERROR - Failed to set the baudrate");
        return 2;
    }

    //Set the proper baud rate and characteristics for the radio
    if (tcsetattr(radio, TCSANOW, &tty) < 0)
    {
        perror("ERROR - Failed to set the terminal characteristics");
        return 3;
    }

    //Initialize the fd_sets
    FD_ZERO(&exceptfds);
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    while (1)
    {
      //Set the timeout
      timeout.tv_sec = 0;
      timeout.tv_usec = 1000 * 1000;

      //Wait for receive data or timeout
      FD_SET(STDIN, &readfds);
      FD_SET(radio, &readfds);
      status = select(maxfds + 1, &readfds, &writefds, &exceptfds, &timeout);

      //Determine if console input is available
      if (FD_ISSET(STDIN, &readfds))
      {
        //Send the console input to the radio
        status = stdinToRadio();
        if (status)
          break;
      }

      if (FD_ISSET(radio, &readfds))
      {
        //Write the radio data to console output
        status = radioToStdout();
        if (status)
          break;
      }
    }

    //Done with the radio
    close(radio);
    radio = -1;
    return status;
}
