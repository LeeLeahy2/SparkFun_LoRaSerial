/**********************************************************************
* Copyright 2023 Lee Leahy (lpleahyjr@gmail.com)
* All rights reserved
*
* ProcessLog.c
*
* Program to process the logs from the Sprinkler Controller
**********************************************************************/

#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

//----------------------------------------
// Constants
//----------------------------------------

#define DAYS_IN_WEEK    7
#define VC_MAX          32
#define ZONES_MAX       8

#define MILLISECONDS_IN_A_SECOND    1000
#define MILLISECONDS_IN_A_MINUTE    (60 * MILLISECONDS_IN_A_SECOND)
#define MILLISECONDS_IN_AN_HOUR     (60 * MILLISECONDS_IN_A_MINUTE)
#define MILLISECONDS_IN_A_DAY       (24 * MILLISECONDS_IN_AN_HOUR)

#define SECONDS_IN_A_MINUTE         60
#define SECONDS_IN_AN_HOUR          (60 * SECONDS_IN_A_MINUTE)
#define SECONDS_IN_A_DAY            (24 * SECONDS_IN_AN_HOUR)
#define SECONDS_IN_A_WEEK           (7 * SECONDS_IN_A_DAY)

#define NEW_LINE                    "\n"

const char * const dayName[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

//----------------------------------------
// Data structures
//----------------------------------------

typedef struct _CONTROLLER_DATA
{
    uint32_t duration[DAYS_IN_WEEK][ZONES_MAX];
    bool enabled;
    bool latchingSolenoid[ZONES_MAX];
    bool manualOn[ZONES_MAX];
    struct _CONTROLLER_DATA * previous;
    uint32_t startTime[DAYS_IN_WEEK];
    int vcNumber;
    bool wateringDay;
    int wateringDays;
} CONTROLLER_DATA;

typedef struct _FAILURE_EVENT
{
    struct _FAILURE_EVENT * previous;
    bool bootFailure;
    bool wateringFailure;
    bool wateringInterrupted;
    int wateringDays;
    time_t failureTime;
    struct tm failureTm;
} FAILURE_EVENT;

typedef struct _SCHEDULE_DATA
{
    int dayNumber;
    uint32_t duration[ZONES_MAX];
    bool enabled;
    bool manualOn[ZONES_MAX];
    struct _SCHEDULE_DATA * previous;
    time_t programmingTime;
    uint32_t startTime;
    bool watering;
    int wateringDays;
} SCHEDULE_DATA;

//----------------------------------------
// Globals
//----------------------------------------

FAILURE_EVENT * failureList;
CONTROLLER_DATA controllerData[VC_MAX];
SCHEDULE_DATA * controllerSchedule[VC_MAX];
int dayNumber;
int dayNumberSent;
uint32_t duration;
int failedToBootCount;
int failedToWaterCount;
FAILURE_EVENT * failureList;
time_t lastLogEntryTime;
bool latchingSolenoid;
bool manualOn;
uint32_t startTime;
int timeZoneOffset = -10 * SECONDS_IN_AN_HOUR;
int totalDays;
int vcNumberSent;
int wateringDays;
int zoneNumber;
int zoneNumberSent;

//----------------------------------------
// Routines
//----------------------------------------

bool processSendingCommand(int vcNumber, char * command)
{
    int value;

    // 2023-08-10 19:10:58: Sending at-CommandDay=0 command to VC 1
    if (sscanf(command, "AT-COMMANDDAY=%d", &value) == 1)
    {
        vcNumberSent = vcNumber;
        dayNumberSent = value;
    }

    // 2023-08-10 19:10:47: Sending at-CommandZone=1 command to VC 1
    else if (sscanf(command, "AT-COMMANDZONE=%d", &value) == 1)
    {
        vcNumberSent = vcNumber;
        zoneNumberSent = value;
    }

    // 2023-08-10 19:10:47: Sending at-LatchingSolenoid=1 command to VC 1
    else if (sscanf(command, "AT-LATCHINGSOLENOID=%d", &value) == 1)
    {
        vcNumberSent = vcNumber;
        latchingSolenoid = value;
    }

    // 2023-08-10 19:10:58: Sending at-StartTime=0 command to VC 1
    else if (sscanf(command, "AT-STARTTIME=%d", &value) == 1)
    {
        vcNumberSent = vcNumber;
        startTime = value;
    }

    // 2023-08-10 19:11:05: Sending at-ZoneDuration=0 command to VC 1
    else if (sscanf(command, "AT-ZONEDURATION=%d", &value) == 1)
    {
        vcNumberSent = vcNumber;
        duration = value;
    }

    // 2023-08-10 19:10:48: Sending at-ZoneManualOn=0 command to VC 1
    else if (sscanf(command, "AT-ZONEMANUALON=%d", &value) == 1)
    {
        vcNumberSent = vcNumber;
        manualOn = value;
    }

    return false;
}

bool wateringInterrupted(int vcNumber, int dayNumber, time_t logTime)
{
    time_t currentTime;
    bool watering;
    time_t wateringDuration;
    time_t wateringTime;
    int zoneNumber;

    // Adjust time for UTC offset
    currentTime = logTime + timeZoneOffset;

    // Convert the time to milliseconds
    currentTime = (currentTime % SECONDS_IN_A_DAY) * 1000;

    // Determine if this event overlaps with the watering schedule
    wateringTime = controllerData[vcNumber].startTime[dayNumber];
    for (zoneNumber = 0; zoneNumber < ZONES_MAX; zoneNumber++)
    {
        wateringDuration = controllerData[vcNumber].duration[dayNumber][zoneNumber];
        if ((currentTime >= wateringTime) && (currentTime < (wateringTime + wateringDuration)))
            return true;
        wateringTime += wateringDuration;
    }
    return false;
}

bool processCommandDone(int vcNumber, char * command, time_t logTime, struct tm * logTm)
{
    FAILURE_EVENT * failure;
    int value;

    // 2023-08-10 19:10:58: VC 1 AT-CommandDay done
    if (((strcmp("AT-COMMANDDAY", command) == 0) || (strcmp("AT-COMMANDDAY-2", command) == 0)) && (vcNumber == vcNumberSent))
        dayNumber = dayNumberSent;

    // 2023-08-10 19:10:47: VC 1 AT-CommandZone done
    else if (((strcmp("AT-COMMANDZONE", command) == 0) || (strcmp("AT-COMMANDZONE-2", command) == 0)) && (vcNumber == vcNumberSent))
        zoneNumber = zoneNumberSent - 1;

    // 2023-08-10 19:10:45: VC 1 AT-EnableController=0 done
    else if (sscanf(command, "AT-ENABLECONTROLLER=%d", &value) == 1)
    {
        controllerData[vcNumber].enabled = value;

        // Determine if the controller was disabled during the watering
        if (!value)
        {
            // Determine if watering got interrupted
            if (wateringInterrupted(vcNumber, logTm->tm_wday, logTime))
            {
                // Remember this event
                failure = malloc(sizeof(*failure));
                if (failure)
                {
                    failure->previous = failureList;
                    failure->bootFailure = false;
                    failure->wateringFailure = true;
                    failure->wateringInterrupted = true;
                    failure->failureTime = logTime;
                    failure->failureTm = *logTm;
                    failureList = failure;
                }
            }
        }
    }

    // 2023-08-10 19:10:47: VC 1 AT-LatchingSolenoid done
    else if ((strcmp("AT-LATCHINGSOLENOID", command) == 0) && (vcNumber == vcNumberSent))
        controllerData[vcNumber].latchingSolenoid[zoneNumber] = latchingSolenoid;

    // 2023-08-10 19:10:58: VC 1 AT-StartTime done
    else if ((strcmp("AT-STARTTIME", command) == 0) && (vcNumber == vcNumberSent))
        controllerData[vcNumber].startTime[dayNumber] = startTime;

    // 2023-08-10 19:11:05: VC 1 AT-ZoneDuration done
    else if ((strcmp("AT-ZONEDURATION", command) == 0) && (vcNumber == vcNumberSent))
        controllerData[vcNumber].duration[dayNumber][zoneNumber] = duration;

    // 2023-08-10 19:10:48: VC 1 AT-ManualOn done
    else if ((strcmp("AT-ZONEMANUALON", command) == 0) && (vcNumber == vcNumberSent))
        controllerData[vcNumber].manualOn[zoneNumber] = manualOn;

    return false;
}

void printStartTime(int vcNumber, int dayNumber)
{
    const char * amPm;
    uint32_t hours;
    uint32_t hours12;
    uint32_t milliseconds;
    uint32_t minutes;
    uint32_t seconds;

    milliseconds = controllerData[vcNumber].startTime[dayNumber];
    hours = milliseconds / MILLISECONDS_IN_AN_HOUR;
    milliseconds %= MILLISECONDS_IN_AN_HOUR;
    minutes = milliseconds / MILLISECONDS_IN_A_MINUTE;
    milliseconds %= MILLISECONDS_IN_A_MINUTE;
    seconds = milliseconds / MILLISECONDS_IN_A_SECOND;
    milliseconds %= MILLISECONDS_IN_A_SECOND;

    hours12 = hours;
    if (!hours)
        hours12 = 12;
    else if (hours > 12)
        hours12 -= 12;

    amPm = "AM";
    if (hours >= 12)
        amPm = "PM";

    printf(" @ %2d:%02d:%02d %s%s", hours12, minutes, seconds, amPm, NEW_LINE );
}

void printDuration(int vcNumber, int dayNumber, int zoneNumber)
{
    uint32_t milliseconds;
    uint32_t minutes;
    uint32_t seconds;

    milliseconds = controllerData[vcNumber].duration[dayNumber][zoneNumber];
    minutes = milliseconds / MILLISECONDS_IN_A_MINUTE;
    milliseconds %= MILLISECONDS_IN_A_MINUTE;
    seconds = milliseconds / MILLISECONDS_IN_A_SECOND;
    milliseconds %= MILLISECONDS_IN_A_SECOND;

    printf("   %2d:%02d", minutes, seconds);
}

bool wateringToday(int vcNumber, int dayNumber, time_t logTime)
{
//    time_t currentTime;
    int wateringDuration;
    bool watering;

    // Adjust time for UTC offset
//    currentTime = logTime + timeZoneOffset;

    // Determine if watering should occur
    watering = false;
    wateringDuration = 0;
    for (zoneNumber = 0; zoneNumber < ZONES_MAX; zoneNumber++)
    {
        // Determine if watering is occurring today
        watering |= controllerData[vcNumber].manualOn[zoneNumber];
        wateringDuration += controllerData[vcNumber].duration[dayNumber][zoneNumber];
    }
    watering |= (wateringDuration != 0);
    return watering;
}

void saveScheduleEntry(int vcNumber, time_t programmingTime, int dayNumber, bool watering)
{
    SCHEDULE_DATA * scheduleEntry;

    // Allocate the schedule entry
    scheduleEntry = malloc(sizeof(*scheduleEntry));
    if (scheduleEntry)
    {
        // Save the schedule entry
        scheduleEntry->dayNumber = dayNumber;
        memcpy((void *)scheduleEntry->duration,
               (void *)controllerData[vcNumber].duration[dayNumber],
               sizeof(scheduleEntry->duration));
        scheduleEntry->enabled = controllerData[vcNumber].enabled;
        memcpy((void *)scheduleEntry->manualOn,
               (void *)controllerData[vcNumber].manualOn,
               sizeof(scheduleEntry->manualOn));
        scheduleEntry->programmingTime = programmingTime;
        scheduleEntry->startTime = controllerData[vcNumber].startTime[dayNumber];
        scheduleEntry->watering = watering;
        scheduleEntry->wateringDays = wateringDays;

        // Link this entry into the list
        scheduleEntry->previous = controllerSchedule[vcNumber];
        controllerSchedule[vcNumber] = scheduleEntry;
    }
}

void processLine(struct tm * logTm, time_t startOfDay, time_t logTime, char * line)
{
    char command[256];
    char commandStatus[256];
    int dayIndex;
    FAILURE_EVENT * failure;
    bool printLine;
    int vcNumber;
    bool watering;
    int zoneNumber;

    do
    {
        // Skip over old (initial test) data
        if (((logTm->tm_year + 1900) < 2023)
            || (((logTm->tm_year + 1900) == 2023) && ((logTm->tm_mon + 1) < 7))
            || (((logTm->tm_year + 1900) == 2023) && ((logTm->tm_mon + 1) == 7) && (logTm->tm_mday < 12))
            || (((logTm->tm_year + 1900) == 2023) && ((logTm->tm_mon + 1) == 7) && (logTm->tm_mday == 12) && (logTm->tm_hour < 19)))
            break;

        // Parse the line to locate the command
        if (sscanf(line, "SENDING %s COMMAND TO VC %d", command, &vcNumber) == 2)
        {
            if (processSendingCommand(vcNumber, command))
                printf("%s", line);
            break;
        }

        // Parse the line to locate the VC, command and status
        if (sscanf(line, "VC %d %256s %256s", &vcNumber, command, commandStatus) <= 1)
            break;

        if (strcmp("DONE", commandStatus) == 0)
        {
            if (strcmp("PROGRAMMING_COMPLETED", command) == 0)
            {
                // Determine if this is the nightly power-on
                if ((logTm->tm_hour >= 16) && (logTm->tm_hour <= 20))
                {
                    static struct tm nextDayTm;

                    // Skip missing days check for first log entry
                    if (nextDayTm.tm_mday != 0)
                    {
                        time_t nextDayTime = mktime(&nextDayTm);
                        while (startOfDay > nextDayTime)
                        {
                            bool failedToWater;
                            char timeBuffer[128];
                            time_t programmingTime;

                            // Estimate the programming time
                            dayIndex = nextDayTm.tm_wday;
                            nextDayTm.tm_hour = logTm->tm_hour;
                            nextDayTm.tm_min = logTm->tm_min;
                            programmingTime = mktime(&nextDayTm);

                            // Account for the boot failure
                            failedToBootCount += 1;

                            // Determine if watering should occur
                            failedToWater = wateringToday(vcNumber, dayIndex, programmingTime);
                            wateringDays += failedToWater ? 1 : 0;

                            // Save the estimated schedule entry
                            saveScheduleEntry(vcNumber, programmingTime, dayIndex, failedToWater);

                            // Remember this boot failure
                            failure = malloc(sizeof(*failure));
                            if (failure)
                            {
                                failure->previous = failureList;
                                failure->bootFailure = true;
                                failure->wateringFailure = failedToWater;
                                failure->wateringInterrupted = false;
                                failure->failureTime = programmingTime;
                                failure->failureTm = nextDayTm;
                                failureList = failure;
                            }

                            // Display the boot failure
                            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S %A", &nextDayTm);
                            printf("*****   %s, Failed to boot%s   *****%s",
                                   timeBuffer,
                                   failedToWater ? ", Failed to water" : "",
                                   NEW_LINE);

                            // Total the number of days that watering failed
                            if (failedToWater)
                                failedToWaterCount += 1;

                            // Set the next day
                            nextDayTm.tm_hour = 0;
                            nextDayTm.tm_min = 0;
                            nextDayTm.tm_mday += 1;
                            nextDayTime = mktime(&nextDayTm);
                            totalDays += 1;
                        }
                    }
                    nextDayTm = *localtime(&startOfDay);
                    nextDayTm.tm_mday += 1;
                    totalDays += 1;
                }

                // Determine if watering occurs today
                dayIndex = logTm->tm_wday;
                watering = wateringToday(vcNumber, dayIndex, logTime);
                wateringDays += watering ? 1 : 0;

                // Save the schedule entry
                saveScheduleEntry(vcNumber, logTime, dayIndex, watering);

                // Display the date
                printf("%4d-%02d-%02d %2d:%02d:%02d %s",
                       logTm->tm_year + 1900,
                       logTm->tm_mon + 1,
                       logTm->tm_mday,
                       logTm->tm_hour,
                       logTm->tm_min,
                       logTm->tm_sec,
                       dayName[dayIndex]);

                // Display the start time
                if (!watering)
                    printf(" watering not scheduled%s", NEW_LINE);
                else
                {
                    printStartTime(vcNumber, dayIndex);

                    // Display the zone numbers
                    printf ("         Zone:");
                    for (zoneNumber = 0; zoneNumber < ZONES_MAX; zoneNumber++)
                        if (controllerData[vcNumber].duration[dayIndex][zoneNumber]
                            || controllerData[vcNumber].manualOn[zoneNumber])
                            printf("       %d", zoneNumber + 1);
                    printf("%s", NEW_LINE);

                    // Display the solenoid types
                    printf ("     Solenoid: ");
                    for (zoneNumber = 0; zoneNumber < ZONES_MAX; zoneNumber++)
                        if (controllerData[vcNumber].duration[dayIndex][zoneNumber]
                            || controllerData[vcNumber].manualOn[zoneNumber])
                            printf("      %s", controllerData[vcNumber].latchingSolenoid[zoneNumber] ? "DC" : "AC");
                    printf("%s", NEW_LINE);

                    // Display the manual on state
                    printf ("    Manual On: ");
                    for (zoneNumber = 0; zoneNumber < ZONES_MAX; zoneNumber++)
                        if (controllerData[vcNumber].duration[dayIndex][zoneNumber]
                            || controllerData[vcNumber].manualOn[zoneNumber])
                            printf("     %3s", controllerData[vcNumber].manualOn[zoneNumber] ? "On" : "Off");
                    printf("%s", NEW_LINE);

                    // Display the zone duration
                    printf ("     Duration:  ");
                    for (zoneNumber = 0; zoneNumber < ZONES_MAX; zoneNumber++)
                        if (controllerData[vcNumber].duration[dayIndex][zoneNumber]
                            || controllerData[vcNumber].manualOn[zoneNumber])
                            printDuration(vcNumber, dayIndex, zoneNumber);
                    printf("%s", NEW_LINE);

                    printf("%s", NEW_LINE);
                }
            }
            else
            {
                if (processCommandDone(vcNumber, command, logTime, logTm))
                    printf("%s", line);
            }
            break;
        }
    } while (0);
}

int parseLine (char * line)
{
    char * byte;
    static int lineNumber;
    static char logString[256];
    time_t logTime;
    struct tm logTm;
    struct tm currentDay;
    time_t startOfDay;
    int status = -1;
    int values;

    do
    {
        // Get the time printed in the log
        memset(logString, 0, sizeof(logString));
        memset(&logTm, 0, sizeof(logTm));
        values = sscanf(line, "%4d-%2d-%2d %2d:%2d:%2d: %256c",
                        &logTm.tm_year,
                        &logTm.tm_mon,
                        &logTm.tm_mday,
                        &logTm.tm_hour,
                        &logTm.tm_min,
                        &logTm.tm_sec,
                        logString);
        if (values < 6)
        {
            fprintf(stderr, "ERROR: Failed to parse the log time!%s", NEW_LINE);
            break;
        }
        logTm.tm_year -= 1900;
        logTm.tm_mon -= 1;

        // Fill in the tm_wday, tm_yday and tm_isdst fields and set tzname
        logTime = mktime(&logTm);
        if (logTime < 0)
        {
            status = errno;
            perror("ERROR: Failed to convert the log time!\n");
            break;
        }
        lastLogEntryTime = logTime;

        // Convert the string to upper case
        byte = logString;
        while (*byte)
            *byte++ = toupper(*byte);

        // Set the time at the start of the day
        currentDay = logTm;
        currentDay.tm_hour = 0;
        currentDay.tm_min = 0;
        currentDay.tm_sec = 0;
        startOfDay = mktime(&currentDay);
        if (startOfDay < 0)
        {
            status = errno;
            perror("ERROR: mktime failed\n");
            break;
        }

        // Process the line
        processLine(&logTm, startOfDay, logTime, logString);
        status = 0;
    } while (0);

    return status;
}

void displayFailure(int count, int totalDays, int maxDays, bool alwaysDisplay)
{
    int days;

    // Determine the number of days to use
    days = totalDays;
    if (days > maxDays)
        days = maxDays;

    // Determine if the value should be displayed
    if (alwaysDisplay || (maxDays && (totalDays > maxDays)))
    {
        if (count)
            printf("   %4d (%4.1f%%)", count, (float)count * 100. / (float)days);
        else
            printf("          0    ");
    }
}

int main (int argc, char ** argv)
{
    FAILURE_EVENT * failure;
    FILE * file;
    size_t length = 0;
    char * line = NULL;
    ssize_t lineLength;
    SCHEDULE_DATA * scheduleEntry;
    int status = 0;

    // Determine the input file
    file = stdin;
    if (argc > 1)
    {
        file = fopen(argv[1], "r");
        if (!file)
        {
            status = errno;
            perror("ERROR: Failed to open the input file\n");
        }
    }

    // Process the file contents
    if (!status)
    {
        do
        {
            // Read an input line
            lineLength = getline(&line, &length, file);
            if (lineLength < 0)
            {
                status = errno;
                break;
            }

            // Parse the line
            status = parseLine(line);
            if (status)
                break;
        } while (1);
    }

    if (!status)
    {
        // Count the boot failures
        int days;
        int failedToBoot = 0;
        int failedToBoot30Days = 0;
        int failedToBoot60Days = 0;
        int failedToBoot90Days = 0;
        int failedToWater = 0;
        int failedToWater30Days = 0;
        int failedToWater60Days = 0;
        int failedToWater90Days = 0;
        int watering30Days = 0;
        int watering60Days = 0;
        int watering90Days = 0;

        // Separate the analysis from the summary
        printf("%s", NEW_LINE);
        printf("--------------------------------------------------------------------------------%s", NEW_LINE);
        printf("%s", NEW_LINE);
        printf("Failure Summary:%s", NEW_LINE);
        printf("%s", NEW_LINE);

        // Display the failure summary
        failure = failureList;
        if (!failure)
            printf("    No boot or watering failures%s", NEW_LINE);
        else
        {
            while (failure)
            {
                // Count failures over 30 days
                if (failure->failureTime >= lastLogEntryTime - (30 * SECONDS_IN_A_DAY))
                {
                    failedToBoot30Days += failure->bootFailure ? 1 : 0;
                    failedToWater30Days += failure->wateringFailure ? 1 : 0;
                }

                // Count failures over 60 days
                if (failure->failureTime >= lastLogEntryTime - (60 * SECONDS_IN_A_DAY))
                {
                    failedToBoot60Days += failure->bootFailure ? 1 : 0;
                    failedToWater60Days += failure->wateringFailure ? 1 : 0;
                }

                // Count failures over 90 days
                if (failure->failureTime >= lastLogEntryTime - (90 * SECONDS_IN_A_DAY))
                {
                    failedToBoot90Days += failure->bootFailure ? 1 : 0;
                    failedToWater90Days += failure->wateringFailure ? 1 : 0;
                }

                // Count total failures
                failedToBoot += failure->bootFailure ? 1 : 0;
                failedToWater += failure->wateringFailure ? 1 : 0;

                // Get the previous failure
                failure = failure->previous;
            }

            // Count the watering days
            scheduleEntry = controllerSchedule[vcNumberSent];
            while (scheduleEntry)
            {
                if (scheduleEntry->watering)
                {
                    // Count watering days over 30 days
                    if (scheduleEntry->programmingTime >= lastLogEntryTime - (30 * SECONDS_IN_A_DAY))
                        watering30Days += 1;

                    // Count watering days over 60 days
                    if (scheduleEntry->programmingTime >= lastLogEntryTime - (60 * SECONDS_IN_A_DAY))
                        watering60Days += 1;

                    // Count watering days over 90 days
                    if (scheduleEntry->programmingTime >= lastLogEntryTime - (90 * SECONDS_IN_A_DAY))
                        watering90Days += 1;
                }
                scheduleEntry = scheduleEntry->previous;
            }

            // Display the failure table header
            printf("                        %4d Days", totalDays);
            if (totalDays > 90)
                printf("   Last 90 Days");
            if (totalDays > 60)
                printf("   Last 60 Days");
            if (totalDays > 30)
                printf("   Last 30 Days");
            printf("%s", NEW_LINE);

            printf("                     ------------");
            if (totalDays > 90)
                printf("   ------------");
            if (totalDays > 60)
                printf("   ------------");
            if (totalDays > 30)
                printf("   ------------");
            printf("%s", NEW_LINE);

            // Display the boot failures
            printf("Boot Failures:    ");
            displayFailure(failedToBoot,       totalDays, totalDays, true);
            displayFailure(failedToBoot90Days, totalDays,        90, false);
            displayFailure(failedToBoot60Days, totalDays,        60, false);
            displayFailure(failedToBoot30Days, totalDays,        30, false);
            printf("%s", NEW_LINE);

            // Display the watering days
            printf("Watering Days:    ");
            printf("           %4d", wateringDays);
            if (totalDays > 90)
                printf("           %4d", watering90Days);
            if (totalDays > 60)
                printf("           %4d", watering60Days);
            if (totalDays > 30)
                printf("           %4d", watering30Days);
            printf("%s", NEW_LINE);

            // Display the watering failures
            printf("Watering Failures:");
            displayFailure(failedToWater,       wateringDays, wateringDays,   true);
            displayFailure(failedToWater90Days, wateringDays, watering90Days, false);
            displayFailure(failedToWater60Days, wateringDays, watering60Days, false);
            displayFailure(failedToWater30Days, wateringDays, watering30Days, false);
            printf("%s", NEW_LINE);

            // Separate the tables
            printf("%s", NEW_LINE);

            // Display the individual failures table header
            printf("  Failures%s", NEW_LINE);
            printf("Boot   Watering     Failure Date%s", NEW_LINE);
            printf("---- ----------- --------------------%s", NEW_LINE);

            // Display the individual failures
            failure = failureList;
            while (failure)
            {
                printf(" %c  %s %-9s %4d-%02d-%02d%s",
                       failure->bootFailure ? '*' : ' ',
                       failure->wateringInterrupted ? "Interrupted" :
                       (failure->wateringFailure ?     "     *     " :
                                                      "           "),
                       dayName[failure->failureTm.tm_wday],
                       failure->failureTm.tm_year + 1900,
                       failure->failureTm.tm_mon + 1,
                       failure->failureTm.tm_mday,
                       NEW_LINE);
                failure = failure->previous;
            }
        }
    }

    // Done with the file
    if (file && (file != stdin))
        fclose(file);

    // Return the final status
    return status;
}
