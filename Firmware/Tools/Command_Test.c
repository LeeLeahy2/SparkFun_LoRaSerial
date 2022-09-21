/**********************************************************************
* Copyright 2022 Lee Leahy (lpleahyjr@gmail.com)
* All rights reserved
*
* Command_Test.c
*
* Test program to send a schedule to the sprinkler controller.
**********************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define RADIO                   "/dev/ttyACM0"
#define BAUD_RATE               B57600

#define ENTER_COMMAND_MODE      "+++\r"
#define OK_RESPONSE             "OK"
#define ERROR_RESPONSE          "ERROR"
#define READ_TIMEOUT            250     //Milliseconds

char responseBuffer[4096];

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
        fprintf(stderr, "ERROR - Write failed, tried to write %ld bytes, only wrote %ld bytes\n",
                length, bytesWritten);
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
                fprintf(stderr, "ERROR - Command write failed, tried to write %ld bytes, only wrote %ld bytes\n",
                        length, bytesWritten);
                return 40;
            }

            //Determine if the command was successful
            status = commandResponse(radio);
        }
        while (status);
    }
    return status;
}

int main(int argc, char **argv)
{
    int radio;
    int status;
    struct termios tty;

    //Wait for the radio
    printf("Waiting for the radio...\n");
    do
    {
        radio = open(RADIO, O_RDWR);

        //Give the radio some time to initialize
        sleep(1);
    }
    while (radio < 0);

    //Get the terminal characteristics
    if (tcgetattr(radio, &tty) < 0)
    {
        perror("ERROR - Failed to get the terminal characteristics");
        return 1;
    }

    //Clear canonical mode to be able to specify timeouts, ignore CR, disable XON/XOFF
    tty.c_iflag &= ~(IGNCR | IXON | IXOFF);

    //Don't echo input
    tty.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHOCTL | ECHOPRT | ECHOKE | ICANON);

    //Set read timeout to 0.1 seconds
    tty.c_cc[VTIME] = 1;

    //Set minimum input
    tty.c_cc[VMIN] = 255;

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

    //Enter command mode
    status = enterCommandMode(radio);
    if (!status)
    {
        //Download the schedule
        status = downloadSchedule(radio);
    }

    //Done with the radio
    close(radio);
    radio = -1;
    return status;
}
