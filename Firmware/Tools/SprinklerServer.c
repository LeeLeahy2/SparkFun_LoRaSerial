/**********************************************************************
* Copyright 2023 Lee Leahy (lpleahyjr@gmail.com)
* All rights reserved
*
* SprinklerServer.c
*
* Program to log data from the SparkFun weather station.
**********************************************************************/

#define DATABASE_DEBUG        0

#define ADD_VC_STATE_NAMES_TABLE
#include "Sprinkler_Server_Notification.h"
#include "WeatherStation.h"

#define LOG_ALL                 0
#define LOG_CMD_COMPLETE        1 //LOG_ALL
#define LOG_CMD_ISSUE           1 //LOG_ALL
#define LOG_CMD_SCHEDULE        1 //LOG_ALL
#define LOG_DATA_ACK            LOG_ALL
#define LOG_DATA_NACK           LOG_ALL
#define LOG_FILE_PATH           "/var/www/html/MH2/vc-logs"
#define LOG_HOST_TO_RADIO       LOG_ALL
#define LOG_LINK_STATUS         1 //LOG_ALL
#define LOG_RADIO_TO_HOST       LOG_ALL
#define LOG_RESPONSE_TYPE       LOG_ALL
#define LOG_RUNTIME             LOG_ALL
#define LOG_SPRINKLER_CHANGES   1 //LOG_ALL
#define LOG_VC_ID               1 //LOG_ALL
#define LOG_VC_STATE            1 //LOG_ALL
#define LOG_WATER_USE           1 //LOG_ALL
#define VC_PC                   VC_SERVER

#define ISSUE_COMMANDS_IN_PARALLEL      0
#ifndef POLL_TIMEOUT_USEC
#define POLL_TIMEOUT_USEC       (10 * 1000)
#endif  // POLL_TIMEOUT_USEC

#define ONE_SECOND_COUNT        100 // (1000000 / POLL_TIMEOUT_USEC)
#define COMMAND_POLL_COUNT      (ONE_SECOND_COUNT / 20) //50 mSec
#define STALL_CHECK_COUNT       15 * ONE_SECOND_COUNT  //15 Sec

#define BUFFER_SIZE             2048
#define INPUT_BUFFER_SIZE       BUFFER_SIZE
#define MAX_MESSAGE_SIZE        256
#define STDIN                   0
#define STDOUT                  1
#define STDERR                  2

#define BREAK_LINKS_COMMAND     "atb"
#define DISABLE_CONTROLLER      "at-EnableController=0"
#define DISPLAY_DATE_TIME       "ati89"
#define ENABLE_CONTROLLER       "at-EnableController=1"
#define GET_CLIENT_MODEL        "ati99"
#define GET_DEVICE_INFO         "ati"
#define GET_MY_VC_ADDRESS       "ati30"
#define GET_RUNTIME             "ati11"
#define GET_SERVER_MODEL        "ati99"
#define GET_UNIQUE_ID           "ati8"
#define GET_VC_STATE            "ati31"
#define GET_VC_STATUS           "ata"
#define GET_WATER_USE           "ati82"
#define LINK_RESET_COMMAND      "atz"
#define MY_VC_ADDRESS           "myVc: "
#define SET_COMMAND_DAY         "at-CommandDay="
#define SET_COMMAND_ZONE        "at-CommandZone="
#define SET_MANUAL_ON           "at-ZoneManualOn="
#define SET_MSEC_PER_INCH       "at-mSecPerInch="
#define SET_PROGRAM_COMPLETE    "ati12"
#define SET_SOLENOID_TYPE       "at-LatchingSolenoid="
#define SET_START_TIME          "at-StartTime="
#define SET_ZONE_DURATION       "at-ZoneDuration="
#define START_3_WAY_HANDSHAKE   "atc"

#define DEBUG_CMD_ISSUE           0
#define DEBUG_DATABASE_UPDATES    0
#define DEBUG_LOCAL_COMMANDS      0
#define DEBUG_PC_CMD_ISSUE        0
#define DEBUG_PC_TO_RADIO         0
#define DEBUG_RADIO_TO_PC         0
#define DEBUG_SPRINKLER_CHANGES   1
#define DISPLAY_COLLECTION_TIME   0
#define DISPLAY_COMMAND_COMPLETE  0
#define DISPLAY_DATA_ACK          0
#define DISPLAY_DATA_NACK         1
#define DISPLAY_RESOURCE_USAGE    0
#define DISPLAY_RUNTIME           0
#define DISPLAY_STATE_TRANSITION  0
#define DISPLAY_UNKNOWN_COMMANDS  0
#define DISPLAY_VC_ID             0
#define DISPLAY_VC_STATE          0
#define DISPLAY_WATER_USE         1
#define DUMP_RADIO_TO_PC          0

#define DATABASE_DEBUG        0

#define DAYS_IN_WEEK          7
#define MSEC_PER_INCH_ENTRIES (MAX_VC * ZONE_NUMBER_MAX)
#define START_TIME_ENTRIES    (MAX_VC * DAYS_IN_WEEK)
#define DURATION_ENTRIES      (START_TIME_ENTRIES * ZONE_NUMBER_MAX)

#define QUEUE_T                   uint64_t
#define QUEUE_T_BITS              ((int)(sizeof(QUEUE_T) * 8))
#define QUEUE_T_MASK              (QUEUE_T_BITS - 1)
#define COMMAND_QUEUE_SIZE        ((CMD_LIST_SIZE + QUEUE_T_MASK) / QUEUE_T_BITS)

#define SECS_IN_MINUTE            60
#define SECS_IN_HOUR              (60 * SECS_IN_MINUTE)
#define SECS_IN_DAY               (24 * SECS_IN_HOUR)

#define MILLIS_IN_SECOND          1000
#define MILLIS_IN_MINUTE          (60 * MILLIS_IN_SECOND)
#define MILLIS_IN_HOUR            (60 * MILLIS_IN_MINUTE)
#define MILLIS_IN_DAY             (24 * MILLIS_IN_HOUR)

#define COMMAND_COMPLETE(queue, active)                                 \
{                                                                       \
  if (COMMAND_PENDING(queue, active))                                   \
  {                                                                     \
    if (DEBUG_CMD_ISSUE)                                                \
    {                                                                   \
      if (queue == pcCommandQueue)                                      \
        printf("PC %s done\n", commandName[active]);                    \
      else                                                              \
      {                                                                 \
        int vc = (&queue[0] - &virtualCircuitList[0].commandQueue[0])   \
               * sizeof(QUEUE_T) / sizeof(virtualCircuitList[0]);       \
        printf("VC %d %s done\n", vc, commandName[active]);             \
      }                                                                 \
    }                                                                   \
    if (LOG_CMD_SCHEDULE)                                               \
    {                                                                   \
      if (queue == pcCommandQueue)                                      \
      {                                                                 \
        sprintf(logBuffer, "PC %s done\n", commandName[active]);        \
        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));       \
      }                                                                 \
      else                                                              \
      {                                                                 \
        int vc = (&queue[0] - &virtualCircuitList[0].commandQueue[0])   \
               * sizeof(QUEUE_T) / sizeof(virtualCircuitList[0]);       \
        int logFileIndex = logFileValidateVcIndex(vc);                  \
        sprintf(logBuffer, "VC %d %s done\n", vc, commandName[active]); \
        logTimeStampAndData(logFileIndex, logBuffer, strlen(logBuffer));\
        if (logFileIndex != VC_PC)                                      \
          logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));     \
      }                                                                 \
    }                                                                   \
    queue[active / QUEUE_T_BITS] &= ~(1ull << (active & QUEUE_T_MASK)); \
    active = CMD_LIST_SIZE;                                             \
  }                                                                     \
}

#define COMMAND_SCHEDULE(queue, pollCount, cmd)                       \
{                                                                     \
  if (DEBUG_CMD_ISSUE)                                                \
  {                                                                   \
    if (!COMMAND_PENDING(queue, cmd))                                 \
    {                                                                 \
      if (queue == pcCommandQueue)                                    \
        printf("PC %s scheduled\n", commandName[cmd]);                \
      else                                                            \
      {                                                               \
        int vc = (&queue[0] - &virtualCircuitList[0].commandQueue[0]) \
               * sizeof(QUEUE_T) / sizeof(virtualCircuitList[0]);     \
        printf("VC %d %s scheduled\n", vc, commandName[cmd]);         \
      }                                                               \
    }                                                                 \
  }                                                                   \
  if (LOG_CMD_SCHEDULE)                                               \
  {                                                                   \
    if (!COMMAND_PENDING(queue, cmd))                                 \
    {                                                                 \
      if (queue == pcCommandQueue)                                    \
      {                                                               \
        sprintf(logBuffer, "PC %s scheduled\n", commandName[cmd]);    \
        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));     \
      }                                                               \
      else                                                            \
      {                                                               \
        int vc = (&queue[0] - &virtualCircuitList[0].commandQueue[0]) \
               * sizeof(QUEUE_T) / sizeof(virtualCircuitList[0]);     \
        sprintf(logBuffer, "VC %d %s scheduled\n", vc, commandName[cmd]);\
        logTimeStampAndData(vc, logBuffer, strlen(logBuffer));        \
      }                                                               \
    }                                                                 \
  }                                                                   \
                                                                      \
  /* Place the command in the queue */                                \
  queue[cmd / QUEUE_T_BITS] |= 1ull << (cmd & QUEUE_T_MASK);          \
                                                                      \
  /* Timeout the command processor */                                 \
  if (!commandProcessorRunning)                                       \
    commandProcessorRunning = STALL_CHECK_COUNT;                      \
                                                                      \
  /* Remember when this command was issued */                         \
  if (!pollCount)                                                     \
  {                                                                   \
    if (timeoutCount)                                                 \
      pollCount = timeoutCount;                                       \
    else                                                              \
      pollCount = 1;                                                  \
  }                                                                   \
}

#define COMMAND_PENDING(queue,cmd)  ((queue[cmd / QUEUE_T_BITS] >> (cmd & QUEUE_T_MASK)) & 1)

typedef enum
{
  //List commands in priority order
  CMD_ATI30 = 0,              //Get myVC
  CMD_ATB,                    //Break all VC links
  CMD_GET_SERVER_MODEL,       //Get the model name
  CMD_ATI,                    //Get the device type
  CMD_ATI8,                   //Get the radio's unique ID
  CMD_ATA,                    //Get VC status

  //Connect to the remote radio
  CMD_AT_CMDVC,               //Select target VC
  CMD_ATC,                    //Start the 3-way handshake

  CMD_WAIT_CONNECTED,         //Wait until the client is connected

  //Get remote radio connection status, type and ID
  CMD_AT_CMDVC_2,             //Select target VC
  CMD_ATI31,                  //Get the VC state
  CMD_GET_CLIENT_MODEL,       //Get the model name
  CMD_ATI_2,                  //Get the device type
  CMD_ATI8_2,                 //Get the radio's unique ID
  CMD_ATI11,                  //Get the runtime

  //Determine if programming is necessary
  CHECK_FOR_UPDATE,

  CMD_AT_DISABLE_CONTROLLER,  //Disable the sprinkler controller

  //Program the date and time
  CMD_AT_DAY_OF_WEEK,         //Set the day of week
  CMD_AT_TIME_OF_DAY,         //Set the time of day
  CMD_ATI89,                  //Display the date and time

  //Get the water use
  CMD_GET_WATER_USE,          //Get the water use

  //Select the solenoids and manual control for each of the zone
  CMD_SELECT_ZONE,            //Select the zone number
  CMD_SELECT_SOLENOID,        //Select the solenoid for the zone
  CMD_SET_MANUAL_ON,          //Set the manual state of the zone valve
  CMD_SET_MSEC_PER_INCH,      //Set the milliseconds per inch
  COMPLETE_ZONE_CONFIGURATION,//Verify that all of the zones are configured

  //Set the start times
  CMD_SET_DAY_OF_WEEK,        //Set the day of the week
  CMD_SET_START_TIME,         //Set the start time
  CMD_SET_ALL_START_TIMES,    //Verify that all of the start times are set

  //Set the zone durations
  CMD_SET_DAY_OF_WEEK_2,      //Set the day of the week
  CMD_SELECT_ZONE_2,          //Select the zone number
  CMD_SET_ZONE_DURATION,      //Set the zone on duration
  CMD_SET_ALL_DURATIONS,      //Verify that all of the durations are set

  CMD_AT_ENABLE_CONTROLLER,   //Enable the sprinkler controller

  //Done programming
  CMD_ATI12,                  //Tell the radio that programming is complete
  PROGRAMMING_COMPLETED,      //Configuration is up-to-date

  //Last in the list
  CMD_LIST_SIZE
} COMMANDS;

const char * const commandName[] =
{
  "ATI30", "ATIB", "ATI99", "ATI", "ATI8", "ATA", "AT-CMDVC", "ATC",
  "WAIT_CONNECT",
  "AT-CMDVC_2", "ATI31", "ATI99", "ATI_2", "ATI8_2", "ATI11",
  "CHECK_FOR_UPDATE",
  "AT-EnableController=0",
  "AT-DayOfWeek", "AT-TimeOfDay", "ATI89",
  "ATI82",
  "AT-CommandZone", "AT-LatchingSolenoid", "AT-ManualOn", "AT-mSecPerInch=", "COMPLETE_ZONE_CONFIGURATION",
  "AT-CommandDay", "AT-StartTime", "CMD_SET_ALL_START_TIMES",
  "AT-CommandDay-2", "AT-CommandZone-2", "AT-ZoneDuration", "CMD_SET_ALL_DURATIONS",
  "AT-EnableController=1",
  "ATI12", "PROGRAMMING_COMPLETED",
};

typedef struct _VIRTUAL_CIRCUIT
{
  int vcState;
  uint32_t activeCommand;
  bool collectData[DAYS_IN_WEEK];
  bool collectData5Am[DAYS_IN_WEEK];
  int collectionTimeSec[DAYS_IN_WEEK];
  QUEUE_T commandQueue[COMMAND_QUEUE_SIZE];
  uint32_t commandTimer;
  uint64_t programmed;
  uint64_t programUpdated;
  uint64_t runtime;
  uint8_t uniqueId[UNIQUE_ID_BYTES];
  bool valid;
  WATER_USE gallons;
} VIRTUAL_CIRCUIT;

uint32_t commandProcessorRunning;
bool commandStatus;
ZONE_T configureSolenoids[MAX_VC];
char * controllerIds[MAX_VC];
char * controllerNames[MAX_VC];
int controllerZones[MAX_VC];
const char * database;
const char * const dayName[] =
{
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};
int dayNumber[MAX_VC];
int32_t durationArray[DURATION_ENTRIES];
bool findMyVc;
char * host;
char htmlMessage[16384];
uint8_t inputBuffer[INPUT_BUFFER_SIZE];
ZONE_T latchingSolenoid[MAX_VC];
char logBuffer[4096];
char logErrorBuffer[4096];
int logFile[MAX_VC];
int manualControl[MAX_VC];
ZONE_T manualOn[MAX_VC];
ZONE_T manualZones[MAX_VC];
uint32_t mSecPerInch[MSEC_PER_INCH_ENTRIES];
MYSQL * mysql;
int myVc = VC_SERVER;
uint8_t outputBuffer[VC_SERIAL_HEADER_BYTES + BUFFER_SIZE];
const char * password;
uint32_t pcActiveCommand = CMD_LIST_SIZE;
char pcCommandBuffer[128];
QUEUE_T pcCommandQueue[COMMAND_QUEUE_SIZE];
uint32_t pcCommandTimer;
int pcCommandVc = MAX_VC;
uint8_t remoteCommandVc;
int remoteVc;
bool setDurations[DURATION_ENTRIES];
bool setMSecPerInch[MSEC_PER_INCH_ENTRIES];
int setStartTimes[START_TIME_ENTRIES];
int solenoidType[MAX_VC];
int32_t startTimeArray[START_TIME_ENTRIES];
ZONE_T tempLatching[MAX_VC];
ZONE_T tempManualOn[MAX_VC];
uint32_t tempMSecPerInch[MSEC_PER_INCH_ENTRIES];
int32_t tempStartTimes[START_TIME_ENTRIES];
int32_t tempZoneDuration[DURATION_ENTRIES];
uint32_t timeoutCount;
const char * username;
char vcCommandBuffer[MAX_VC][128];
VIRTUAL_CIRCUIT virtualCircuitList[MAX_VC];
volatile bool waitingForCommandComplete;
int zoneNumber[MAX_VC];

int logFileValidateVcIndex(int vcIndex);

// Close the log files for each of the virtual circuits
void logFileClose()
{
    int vcIndex;

    // Close the log files
    for (vcIndex = 0; vcIndex < MAX_VC; vcIndex++)
    {
        if (logFile[vcIndex] > 0)
            close(logFile[vcIndex]);
    }
}

// Open the log file associated with a vcIndex
int logFileOpen(int vcIndex)
{
    char filename[256];
    int logFileIndex;
    int status;

    status = -1;
    do
    {
        // Validate the VC index
        logFileIndex = logFileValidateVcIndex(vcIndex);

        // Only open the file once
        if (logFile[logFileIndex] > 0)
        {
            status = 0;
            break;
        }

        // Create the log file name
        sprintf (filename, "%s/vc%02d.txt", LOG_FILE_PATH, vcIndex);
        status = open(filename, O_APPEND | O_CREAT | O_WRONLY, S_IRWXU | S_IRWXG | S_IROTH);

        // Verify that the log file was created successfully
        if (status < 0)
        {
            status = errno;
            perror("ERROR: Failed to open VC log file!");
            break;
        }

        // Save the file number
        logFile[logFileIndex] = status;
        status = 0;
    } while (0);

    // Return the open status
    return status;
}

// Write data to the log file
int logWrite(int vcIndex, char * text, int length)
{
    int bytesWritten;
    int dataWritten;
    int logFileIndex;
    int status;

    do
    {
        // Validate the VC index
        logFileIndex = logFileValidateVcIndex(vcIndex);

        // Open the file if necessary
        status = logFileOpen(logFileIndex);
        if (status)
            break;

        // Write the entire buffer to the log file or return an error
        bytesWritten = 0;
        while (bytesWritten < length)
        {
            // Write the data to the log file
            dataWritten = write(logFile[logFileIndex], &text[bytesWritten], length - bytesWritten);

            // Verify that the data was successfully written
            if (dataWritten < 0)
            {
                status = errno;
                perror("ERROR: Failed write to log file!");
                break;
            }

            // Determine how much data was written
            bytesWritten += dataWritten;
        }
    } while (0);

    // Return the write status
    return status;
}

// Write a timestamp to the log file
int logTimeStamp(int vcIndex)
{
    time_t t;
    char timeBuffer[32];
    struct tm * tm;

    // Get the current time
    time(&t);
    tm = localtime(&t);
    sprintf(timeBuffer, "%04d-%02d-%02d %2d:%02d:%02d: ", tm->tm_year + 1900, tm->tm_mon + 1,
            tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    return logWrite(vcIndex, timeBuffer, strlen(timeBuffer));
}

// Write timestamp and data
int logTimeStampAndData(int vcIndex, char * text, int length)
{
    int logFileIndex;
    int status;

    // Validate the VC index
    logFileIndex = logFileValidateVcIndex(vcIndex);

    // Timestamp the line
    status = logTimeStamp(logFileIndex);
    if (!status)

        // Output the line of text
        status = logWrite(logFileIndex, text, length);
    return status;
}

// Translate the vcIndex into a valid file index value
int logFileValidateVcIndex(int vcIndex)
{
    int channel;
    int error;
    int logFileIndex;

    // Validate the VC index
    error = 1;
    logFileIndex = VC_PC;

    // Verify that it is within the 256 values of the VC index range
    if ((vcIndex >= 0) && (vcIndex <= 255))
    {
        // Verify that a reserved channel is not being used
        channel = (vcIndex & VCAB_CHANNEL_MASK) >> VCAB_NUMBER_BITS;
        if ((channel < 3) || (channel > 4))
        {
            // Valid channel
            error = 0;

            // Determine if the local PC is being addressed
            if ((channel != 7) && (channel != 5))

                // A local or remote radio is being addressed, extract the VC index
                logFileIndex = vcIndex & VCAB_NUMBER_MASK;
        }
    }

    // Log the error
    if (error)
    {
        fprintf(stderr, "ERROR: Invalid vcIndex (%d)!\n", vcIndex);
        sprintf(logErrorBuffer, "ERROR: Invalid vcIndex (%d)!\n", vcIndex);
        logTimeStampAndData(logFileIndex, logErrorBuffer, strlen(logErrorBuffer));
    }

    // Return a valid log file index
    return logFileIndex;
}

// Dump a buffer to the log file
void logDumpBuffer(int vcIndex, uint8_t * data, int length)
{
  char byte;
  int bytes;
  uint8_t * dataEnd;
  uint8_t * dataStart;
  const int displayWidth = 16;
  int index;
  int logFileIndex;
  char text[128];

  // Validate the VC index
  logFileIndex = logFileValidateVcIndex(vcIndex);

  dataStart = data;
  dataEnd = &data[length];
  while (data < dataEnd)
  {
    // Display the offset
    sprintf(text, "    0x%02x: ", (unsigned int)(data - dataStart));
    length = strlen(text);

    // Determine the number of bytes to display
    bytes = dataEnd - data;
    if (bytes > displayWidth)
      bytes = displayWidth;

    // Display the data bytes in hex
    for (index = 0; index < bytes; index++)
    {
      sprintf(&text[length], " %02x", *data++);
      length += 3;
    }

    // Space over to the ASCII display
    for (; index < displayWidth; index++)
    {
      sprintf(&text[length], "   ");
      length += 3;
    }
    sprintf(&text[length], "  ");
    length += 3;

    // Display the ASCII bytes
    data -= bytes;
    for (index = 0; index < bytes; index++)
    {
      byte = *data++;
      text[length++] = ((byte < ' ') || (byte >= 0x7f)) ? '.' : byte;
    }
    text[length++] = '\n';
    logTimeStampAndData(logFileIndex, text, length);
  }
}

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

//Initialize the database
MYSQL * databaseInitialization(bool debug)
{
  MYSQL * mysql;

  //Get the connection to the database engine
  mysql = mysql_init(NULL);
  if (mysql == NULL)
    fprintf(stderr, "ERROR - mysql_init failed to initialize the database connection\n");
  else if (debug)
    printf("mysql_init successful\n");
  return mysql;
}

int databaseConnect(MYSQL * mysql, bool debug)
{
  int status;

  status = 0;
  do
  {
    if (mysql_real_connect(mysql, host, username, password,
            database, 0, NULL, 0) == NULL)
    {
      status = mysql_errno(mysql);
      fprintf(stderr, "%s\n", mysql_error(mysql));
      break;
    }
    if (debug)
      printf("mysql_real_connect successful\n");
  } while (0);
  return status;
}

//Get the field names
int databaseValidateResults(MYSQL_RES * results)
{
  bool error;

  //Validate the inputs
  error = 0;
  if (!results)
  {
    fprintf(stderr, "ERROR: results must be the address of a MYSQL_RES structure\n");
    error = 1;
  }
  return error;
}

//Get the field names
MYSQL_FIELD * databaseGetFields(MYSQL_RES * results, bool debug)
{
  MYSQL_FIELD * fields;

  do
  {
    //Validate the inputs
    fields = NULL;
    if (databaseValidateResults(results))
      break;

    //Get the field names
    fields = mysql_fetch_fields(results);
    if (debug)
      printf("mysql_fetch_fields successfully returned the fields\n");
  } while (0);

  //Return the fields
  return fields;
}

//Get the column count
int databaseGetColumns(MYSQL * mysql, bool debug)
{
  int columns;

  do
  {
    //Validate the inputs
    columns = 0;
    if (!mysql)
    {
      fprintf(stderr, "ERROR: mysql must be the address of a MYSQL structure\n");
      break;
    }

    //Get the column count
    columns = mysql_field_count(mysql);
    if (debug)
      printf("Columns: %d\n", columns);
  } while (0);

  //Return the column count
  return columns;
}

//Get the row count
int databaseGetRows(MYSQL_RES * results, bool debug)
{
  int rows;

  do
  {
    //Validate the inputs
    rows = 0;
    if (databaseValidateResults(results))
      break;

    //Get the row count
    rows = mysql_num_rows(results);
    if (debug)
      printf("Rows: %d\n", rows);
  } while (0);

  //Return the row count
  return rows;
}

//Get the next row in the table
MYSQL_ROW databaseGetNextRow(MYSQL_RES * results)
{
  return mysql_fetch_row(results);
}

//Query the database
int databaseQuery(
  MYSQL * mysql,
  const char * query,
  MYSQL_RES ** resultsAddr,
  MYSQL_FIELD ** fieldsAddr,
  int * columnsAddr,
  int * rowsAddr,
  bool debug
  )
{
  int columns;
  MYSQL_FIELD * fields;
  MYSQL_RES * results;
  MYSQL_ROW row;
  int rows;
  int status;

  do
  {
    if (debug)
    {
      printf("query: %p\n", (void *)query);
      printf("resultsAddr: %p\n", (void *)resultsAddr);
      printf("fieldsAddr: %p\n", (void *)fieldsAddr);
      printf("columnsAddr: %p\n", (void *)columnsAddr);
      printf("rowsAddr: %p\n", (void *)rowsAddr);
    }

    //Initialize the results
    if (resultsAddr)
      *resultsAddr = NULL;
    if (fieldsAddr)
      *fieldsAddr = NULL;
    if (columnsAddr)
      *columnsAddr = 0;
    if (rowsAddr)
      *rowsAddr = 0;

    //Perform the query
    status = mysql_query(mysql, query);
    if (status)
    {
      printf("Query: %s\n", query);
      status = mysql_errno(mysql);
      fprintf(stderr, "ERROR: mysql_query failed, status: %d\n", status);
      break;
    }
    if (debug)
      printf("mysql_query successful\n");

    //Get the results
    if (!resultsAddr)
      break;
    results = mysql_store_result(mysql);
    if (results == NULL)
    {
      status = mysql_errno(mysql);
      fprintf(stderr, "ERROR: mysql_store_result failed, status: %d\n", status);
      break;
    }
    *resultsAddr = results;
    if (debug)
      printf("mysql_store_result successful\n");

    //Get the field names
    fields = databaseGetFields(results, debug);
    if (fieldsAddr)
      *fieldsAddr = fields;

    //Get the number of fields and rows
    columns = databaseGetColumns(mysql, debug);
    if (columnsAddr)
      *columnsAddr = columns;

    //Get the number of rows
    rows = databaseGetRows(results, debug);
    if (rowsAddr)
      *rowsAddr = rows;

    //Display the rows
    if (debug)
    {
      MYSQL_ROW_OFFSET offset;
      int rowCount;

      //Save the row cursor
      offset = mysql_row_tell(results);

      //Walk through the results
      rowCount = 0;
      while ((rowCount < rows) && (row = databaseGetNextRow(results)))
      {
          printf("Row %d\n", rowCount++);
          for(int i = 0; i < columns; i++)
            printf("    %s: %s\n", fields[i].name, row[i] ? row[i] : "NULL");
          printf("\n");
      }

      //Restore the row cursor
      mysql_row_seek(results, offset);
    }
  } while(0);
  return status;
}

//Get an integer from the database
int databaseGetInteger(const char * string, int * value)
{
  //Check for NULL
  if (!string)
    return 0;

  //Convert the string into an integer
  if (sscanf(string, "%d", value) == 1)
    return 1;
  else
    return 0;
}

//Get a time from the database
int databaseGetTime(const char * string, int * hours, int * minutes, int * seconds)
{
  //Check for NULL
  if (!string)
    return 0;

  //Convert the string into an integer
  if (sscanf(string, "%d:%d:%d", hours, minutes, seconds) == 3)
    return 1;
  else
    return 0;
}

//Done with the results
void databaseFreeResults(MYSQL_RES * results)
{
  mysql_free_result(results);
}

//Done with the database connection
void databaseCloseDatabase(MYSQL * mysql)
{
  if (mysql)
    mysql_close(mysql);
}

//Update the VC based upon the radio ID
int databaseUpdateVc(MYSQL * mysql, uint8_t vcIndex, uint8_t * uniqueId, bool debug)
{
  int id;
  MYSQL_RES * results;
  MYSQL_ROW row;
  int rows;
  MYSQL_STMT * statement;
  int status;
  char string[128];
  char uniqueIdString[40];
  int vcNumber;

  do
  {
    status = 0;
    statement = NULL;

    //Convert the unique ID into text
    sprintf(uniqueIdString, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
            uniqueId[0], uniqueId[1], uniqueId[2], uniqueId[3],
            uniqueId[4], uniqueId[5], uniqueId[6], uniqueId[7],
            uniqueId[8], uniqueId[9], uniqueId[10], uniqueId[11],
            uniqueId[12], uniqueId[13], uniqueId[14], uniqueId[15]);

    sprintf(string, "SELECT UniqueId, VcNumber FROM unique_id WHERE (RadioId = \"%s\")",
            uniqueIdString);
      if (debug || DEBUG_DATABASE_UPDATES)
        printf("%s\n", string);

    //Determine if this radio ID is in the database
    status = databaseQuery(mysql, string, &results, NULL, NULL, &rows, debug);
    if (status)
      break;
    if (rows)
    {
      row = databaseGetNextRow(results);
      sscanf(row[0], "%d", &id);
      sscanf(row[1], "%d", &vcNumber);
    }
    databaseFreeResults(results);

    //Allocate the SQL statement
    statement = mysql_stmt_init(mysql);
    if (!statement)
    {
      status = -1;
      fprintf(stderr, "ERROR: VC update failed calling mysql_stmt_init\n");
      fprintf(stderr, "Out of memory\n");
      break;
    }

    if (rows)
    {
      if (vcNumber != vcIndex)
      {
        //Disable the previous radio
        if (vcNumber != MAX_VC)
        {
          //Build the update command
          sprintf(string, "UPDATE unique_id SET VcNumber = %d WHERE (VcNumber = %d)",
                  MAX_VC, vcNumber);
          if (debug || DEBUG_DATABASE_UPDATES)
            printf("SQL: %s\n", string);
          if (mysql_stmt_prepare(statement, string, strlen(string)))
          {
            status = mysql_stmt_errno(statement);
            fprintf(stderr, "ERROR: VC update failed calling mysql_stmt_prepare\n");
            fprintf(stderr, "%s\n", mysql_stmt_error(statement));
            break;
          }

          //Execute the update statement
          if (mysql_stmt_execute(statement))
          {
            status = mysql_stmt_errno(statement);
            fprintf(stderr, "ERROR: VC update failed calling mysql_stmt_execute\n");
            fprintf(stderr, "%s\n", mysql_stmt_error(statement));
            break;
          }

          //Determine the number of affected rows
          rows = mysql_stmt_affected_rows(statement);
          if (debug || DEBUG_DATABASE_UPDATES)
            printf("%d rows updated\n", rows);
        }

        //Build the update command
        sprintf(string, "UPDATE unique_id SET VcNumber = %d WHERE (UniqueId = %d)",
                vcIndex, id);
        if (debug || DEBUG_DATABASE_UPDATES)
          printf("SQL: %s\n", string);
        if (mysql_stmt_prepare(statement, string, strlen(string)))
        {
          status = mysql_stmt_errno(statement);
          fprintf(stderr, "ERROR: VC update failed calling mysql_stmt_prepare\n");
          fprintf(stderr, "%s\n", mysql_stmt_error(statement));
          break;
        }

        //Execute the update statement
        if (mysql_stmt_execute(statement))
        {
          status = mysql_stmt_errno(statement);
          fprintf(stderr, "ERROR: VC update failed calling mysql_stmt_execute\n");
          fprintf(stderr, "%s\n", mysql_stmt_error(statement));
          break;
        }

        //Determine the number of affected rows
        rows = mysql_stmt_affected_rows(statement);
        if (debug || DEBUG_DATABASE_UPDATES)
          printf("%d rows updated\n", rows);
      }
    }
    else
    {
      //Build the insert statement
      sprintf(string, "INSERT INTO unique_id (RadioId, VcNumber) VALUES (\"%s\", %d)",
              uniqueIdString, vcIndex);
      if (debug || DEBUG_DATABASE_UPDATES)
        printf("SQL: %s\n", string);
      if (mysql_stmt_prepare(statement, string, strlen(string)))
      {
        status = mysql_stmt_errno(statement);
        fprintf(stderr, "ERROR: VC insert failed calling mysql_stmt_prepare\n");
        fprintf(stderr, "%s\n", mysql_stmt_error(statement));
        break;
      }

      //Execute the insert statement
      if (mysql_stmt_execute(statement))
      {
        status = mysql_stmt_errno(statement);
        fprintf(stderr, "ERROR: VC insert failed calling mysql_stmt_execute\n");
        fprintf(stderr, "%s\n", mysql_stmt_error(statement));
        break;
      }
    }
  } while (0);

  //Done with the SQL statement
  if (statement)
    mysql_stmt_close(statement);
  return status;
}

int cmdToRadio(uint8_t * buffer, int length)
{
  int bytesSent;
  int bytesWritten;
  VC_SERIAL_MESSAGE_HEADER header;

  //Build the virtual circuit serial header
  header.start = START_OF_VC_SERIAL;
  header.radio.length = VC_RADIO_HEADER_BYTES + length;
  header.radio.destVc = VC_COMMAND;
  header.radio.srcVc = PC_COMMAND;

  //Display the data being sent to the radio
  if (DEBUG_PC_TO_RADIO)
  {
    dumpBuffer((uint8_t *)&header, VC_SERIAL_HEADER_BYTES);
    dumpBuffer(buffer, length);
  }
  if (DEBUG_LOCAL_COMMANDS)
    printf("Sending LoRaSerial command: %s\n", buffer);

  //Send the header
  bytesWritten = write(radio, (uint8_t *)&header, VC_SERIAL_HEADER_BYTES);
  if (bytesWritten < (int)VC_SERIAL_HEADER_BYTES)
  {
    perror("ERROR: Write of header to radio failed!");
    return -1;
  }

  //Send the message
  bytesSent = 0;
  while (bytesSent < length)
  {
    bytesWritten = write(radio, &buffer[bytesSent], length - bytesSent);
    if (bytesWritten < 0)
    {
      perror("ERROR: Write of data to radio failed!");
      return bytesWritten;
    }
    bytesSent += bytesWritten;
  }

  //Return the amount of the buffer that was sent
  return length;
}

int hostToRadio(uint8_t destVc, uint8_t * buffer, int length)
{
  int bytesSent;
  int bytesWritten;
  VC_SERIAL_MESSAGE_HEADER header;
  int logFileIndex;

  //Build the virtual circuit serial header
  header.start = START_OF_VC_SERIAL;
  header.radio.length = VC_RADIO_HEADER_BYTES + length;
  header.radio.destVc = destVc;
  header.radio.srcVc = myVc;

  //Display the data being sent to the radio
  if (DEBUG_PC_TO_RADIO)
  {
    dumpBuffer((uint8_t *)&header, VC_SERIAL_HEADER_BYTES);
    dumpBuffer(buffer, length);
  }
  if (LOG_HOST_TO_RADIO)
  {
    logDumpBuffer(VC_PC, (uint8_t *)&header, VC_SERIAL_HEADER_BYTES);
    logFileIndex = logFileValidateVcIndex(destVc);
    if (logFileIndex != VC_PC)
    {
      logDumpBuffer(logFileIndex, (uint8_t *)&header, VC_SERIAL_HEADER_BYTES);
      logDumpBuffer(logFileIndex, buffer, length);
    }
    logDumpBuffer(VC_PC, buffer, length);
    fdatasync(logFile[VC_PC]);
  }

  //Send the header
  bytesWritten = write(radio, (uint8_t *)&header, VC_SERIAL_HEADER_BYTES);
  if (bytesWritten < (int)VC_SERIAL_HEADER_BYTES)
  {
    perror("ERROR: Write of header to radio failed!");
    return -1;
  }

  //Send the message
  bytesSent = 0;
  while (bytesSent < length)
  {
    bytesWritten = write(radio, &buffer[bytesSent], length - bytesSent);
    if (bytesWritten < 0)
    {
      perror("ERROR: Write of data to radio failed!");
      return bytesWritten;
    }
    bytesSent += bytesWritten;
  }

  //Return the amount of the buffer that was sent
  return length;
}

int stdinToRadio()
{
  int bytesRead;
  int bytesSent;
  int bytesToSend;
  int bytesWritten;
  int length;
  int maxfds;
  int status;
  struct timeval timeout;
  static int index;

  status = 0;
  if (!waitingForCommandComplete)
  {
    if (remoteVc == VC_COMMAND)
    {
      do
      {
        do
        {
          //Read the console input data into the local buffer.
          bytesRead = read(STDIN, &inputBuffer[index], 1);
          if (bytesRead < 0)
          {
            perror("ERROR: Read from stdin failed!");
            status = bytesRead;
            break;
          }
          index += bytesRead;
        } while (bytesRead && (inputBuffer[index - bytesRead] != '\n'));

        //Check for end of data
        if (!bytesRead)
          break;

        //Send this command the VC
        bytesWritten = cmdToRadio(inputBuffer, index);
        waitingForCommandComplete = true;
        remoteCommandVc = myVc;
        index = 0;
      } while (0);
    }
    else
      do
      {
        //Read the console input data into the local buffer.
        bytesRead = read(STDIN, inputBuffer, BUFFER_SIZE);
        if (bytesRead < 0)
        {
          perror("ERROR: Read from stdin failed!");
          status = bytesRead;
          break;
        }

        //Determine if this is a remote command
        if ((remoteVc >= PC_REMOTE_COMMAND) && (remoteVc < THIRD_PARTY_COMMAND))
        {
          remoteCommandVc = remoteVc & VCAB_NUMBER_MASK;
          waitingForCommandComplete = true;
        }

        //Send this data over the VC
        bytesSent = 0;
        while (bytesSent < bytesRead)
        {
          //Break up the data if necessary
          bytesToSend = bytesRead - bytesSent;
          if (bytesToSend > MAX_MESSAGE_SIZE)
            bytesToSend = MAX_MESSAGE_SIZE;

          //Send the data
          bytesWritten = hostToRadio(remoteVc, &inputBuffer[bytesSent], bytesToSend);
          if (bytesWritten < 0)
          {
            perror("ERROR: Write to radio failed!");
            status = bytesWritten;
            break;
          }

          //Account for the bytes written
          bytesSent += bytesWritten;
        }
      } while (0);
  }
  return status;
}

int hostToStdout(VC_SERIAL_MESSAGE_HEADER * header, uint8_t * data, uint8_t bytesToSend)
{
  uint8_t * buffer;
  uint8_t * bufferEnd;
  int bytesSent;
  int bytesWritten;
  static uint8_t compareBuffer[4 * BUFFER_SIZE];
  static int offset;
  int status;
  int vcNumber;

  //Locate myVc if necessary
  if (findMyVc)
  {
    //Place the data into the compare buffer
    buffer = compareBuffer;
    memcpy(&compareBuffer[offset], data, bytesToSend);
    offset += bytesToSend;
    bufferEnd = &buffer[offset];

    //Walk through the buffer
    while (buffer < bufferEnd)
    {
      if ((strncmp((const char *)buffer, MY_VC_ADDRESS, strlen(MY_VC_ADDRESS)) == 0)
        && (&buffer[strlen(MY_VC_ADDRESS) + 3] <= bufferEnd))
      {
        if ((sscanf((const char *)&buffer[strlen(MY_VC_ADDRESS)], "%d", &vcNumber) == 1)
          && ((uint8_t)vcNumber < MAX_VC))
        {
          findMyVc = false;

          //Set the local radio's VC number
          myVc = (int8_t)vcNumber;
          printf("myVc: %d\n", myVc);

          //Complete this command
          COMMAND_COMPLETE(pcCommandQueue, pcActiveCommand);
          break;
        }
      }

      //Skip to the end of the line
      while ((buffer < bufferEnd) && (*buffer != '\n'))
        buffer++;

      if ((buffer < bufferEnd) && (*buffer == '\n'))
      {
        //Skip to the next line
        while ((buffer < bufferEnd) && (*buffer == '\n'))
          buffer++;

        //Move this data to the beginning of the buffer
        offset = bufferEnd - buffer;
        memcpy(compareBuffer, buffer, offset);
        buffer = compareBuffer;
        bufferEnd = &buffer[offset];
      }
    }
  }

  //Write this data to stdout
  bytesSent = 0;
  status = 0;
  fflush(stdout);
  while (bytesSent < bytesToSend)
  {
    bytesWritten = write(STDOUT, &data[bytesSent], bytesToSend - bytesSent);
    if (bytesWritten < 0)
    {
      perror("ERROR: Write to stdout!");
      status = bytesWritten;
      break;
    }

    //Account for the bytes written
    bytesSent += bytesWritten;
  }
  return status;
}

void radioToPcLinkStatus(VC_SERIAL_MESSAGE_HEADER * header, uint8_t * data, uint8_t length)
{
  int newState;
  int previousState;
  int srcVc;
  uint8_t uniqueId[UNIQUE_ID_BYTES];
  VC_STATE_MESSAGE * vcMsg;

  //Remember the previous state
  srcVc = header->radio.srcVc;
  previousState = virtualCircuitList[srcVc].vcState;
  vcMsg = (VC_STATE_MESSAGE *)&header[1];

  //Set the new state
  newState = vcMsg->vcState;
  virtualCircuitList[srcVc].vcState = newState;

  //Display the state if requested
  if (DISPLAY_STATE_TRANSITION || LOG_LINK_STATUS
    || (newState == VC_STATE_LINK_DOWN) || (previousState == VC_STATE_LINK_DOWN)
    || ((newState != previousState) && (virtualCircuitList[srcVc].activeCommand < CMD_LIST_SIZE)))
  {
    if (DISPLAY_STATE_TRANSITION)
      printf("VC%d: %s --> %s\n", srcVc, vcStateNames[previousState], vcStateNames[newState]);
    if (LOG_LINK_STATUS)
    {
      sprintf(logBuffer, "VC%d: %s --> %s\n", srcVc, vcStateNames[previousState], vcStateNames[newState]);
      logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
    }
  }

  //Save the LoRaSerial radio's unique ID
  //Determine if the PC's value is valid
  memset(uniqueId, UNIQUE_ID_ERASE_VALUE, sizeof(uniqueId));
  if (!virtualCircuitList[srcVc].valid)
  {
    //Determine if the radio knows the value
    if (memcmp(vcMsg->uniqueId, uniqueId, sizeof(uniqueId)) != 0)
    {
      //The radio knows the value, save it in the PC
      memcpy(virtualCircuitList[srcVc].uniqueId, vcMsg->uniqueId, sizeof(vcMsg->uniqueId));
      virtualCircuitList[srcVc].valid = true;

      //Display this ID value
      if (DISPLAY_VC_ID)
        printf("VC %d unique ID: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
               srcVc,
               vcMsg->uniqueId[0], vcMsg->uniqueId[1], vcMsg->uniqueId[2], vcMsg->uniqueId[3],
               vcMsg->uniqueId[4], vcMsg->uniqueId[5], vcMsg->uniqueId[6], vcMsg->uniqueId[7],
               vcMsg->uniqueId[8], vcMsg->uniqueId[9], vcMsg->uniqueId[10], vcMsg->uniqueId[11],
               vcMsg->uniqueId[12], vcMsg->uniqueId[13], vcMsg->uniqueId[14], vcMsg->uniqueId[15]);
      if (LOG_VC_ID)
      {
        sprintf(logBuffer, "VC %d unique ID: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
               srcVc,
               vcMsg->uniqueId[0], vcMsg->uniqueId[1], vcMsg->uniqueId[2], vcMsg->uniqueId[3],
               vcMsg->uniqueId[4], vcMsg->uniqueId[5], vcMsg->uniqueId[6], vcMsg->uniqueId[7],
               vcMsg->uniqueId[8], vcMsg->uniqueId[9], vcMsg->uniqueId[10], vcMsg->uniqueId[11],
               vcMsg->uniqueId[12], vcMsg->uniqueId[13], vcMsg->uniqueId[14], vcMsg->uniqueId[15]);
        logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
      }

      //Update the database
      databaseUpdateVc(mysql, srcVc, vcMsg->uniqueId, 1);
    }
  }
  else
  {
    //Determine if the radio has changed for this VC
    if ((memcmp(vcMsg->uniqueId, virtualCircuitList[srcVc].uniqueId, sizeof(vcMsg->uniqueId)) != 0)
        && (memcmp(vcMsg->uniqueId, uniqueId, sizeof(uniqueId)) != 0))
    {
      //The radio knows the value, save it in the PC
      memcpy(virtualCircuitList[srcVc].uniqueId, vcMsg->uniqueId, sizeof(vcMsg->uniqueId));
      virtualCircuitList[srcVc].valid = true;

      //Display this ID value
      if (DISPLAY_VC_ID)
        printf("VC %d unique ID: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
               srcVc,
               vcMsg->uniqueId[0], vcMsg->uniqueId[1], vcMsg->uniqueId[2], vcMsg->uniqueId[3],
               vcMsg->uniqueId[4], vcMsg->uniqueId[5], vcMsg->uniqueId[6], vcMsg->uniqueId[7],
               vcMsg->uniqueId[8], vcMsg->uniqueId[9], vcMsg->uniqueId[10], vcMsg->uniqueId[11],
               vcMsg->uniqueId[12], vcMsg->uniqueId[13], vcMsg->uniqueId[14], vcMsg->uniqueId[15]);
      if (LOG_VC_ID)
      {
        sprintf(logBuffer, "VC %d unique ID: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
               srcVc,
               vcMsg->uniqueId[0], vcMsg->uniqueId[1], vcMsg->uniqueId[2], vcMsg->uniqueId[3],
               vcMsg->uniqueId[4], vcMsg->uniqueId[5], vcMsg->uniqueId[6], vcMsg->uniqueId[7],
               vcMsg->uniqueId[8], vcMsg->uniqueId[9], vcMsg->uniqueId[10], vcMsg->uniqueId[11],
               vcMsg->uniqueId[12], vcMsg->uniqueId[13], vcMsg->uniqueId[14], vcMsg->uniqueId[15]);
        logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
      }

      //Update the database
      databaseUpdateVc(mysql, srcVc, vcMsg->uniqueId, 1);
    }
  }

  switch (newState)
  {
  default:
    if (DEBUG_PC_CMD_ISSUE)
      printf("VC %d unknown state!\n", srcVc);
    if (LOG_CMD_SCHEDULE)
    {
      sprintf(logBuffer, "VC %d unknown state!\n", srcVc);
      logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
    }
    if (DISPLAY_VC_STATE)
      printf("------- VC %d State %3d ------\n", srcVc, vcMsg->vcState);
    if (LOG_VC_STATE)
    {
      sprintf(logBuffer, "------- VC %d State %3d ------\n", srcVc, vcMsg->vcState);
      logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
    }
    break;

  case VC_STATE_LINK_DOWN:
    //Stop the command processing for this VC
    virtualCircuitList[srcVc].activeCommand = CMD_LIST_SIZE;
    virtualCircuitList[srcVc].commandTimer = 0;
    if (DEBUG_PC_CMD_ISSUE)
      printf("VC %d DOWN\n", srcVc);
    if (LOG_CMD_SCHEDULE)
    {
      sprintf(logBuffer, "VC %d DOWN\n", srcVc);
      logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
    }
    if (DISPLAY_VC_STATE)
      printf("--------- VC %d DOWN ---------\n", srcVc);
    if (LOG_VC_STATE)
    {
      sprintf(logBuffer, "--------- VC %d DOWN ---------\n", srcVc);
      logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
    }
    break;

  case VC_STATE_LINK_ALIVE:
    //Upon transition to ALIVE, if is the server or the source VC matches the
    //target VC or myVc, bring up the connection
    if ((previousState != newState) && (myVc == VC_SERVER))
    {
      if (DEBUG_PC_CMD_ISSUE)
        printf("VC %d ALIVE\n", srcVc);
      if (LOG_CMD_SCHEDULE)
      {
        sprintf(logBuffer, "VC %d ALIVE\n", srcVc);
        logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
      }
      COMMAND_SCHEDULE(virtualCircuitList[srcVc].commandQueue,
                       virtualCircuitList[srcVc].commandTimer,
                       CMD_WAIT_CONNECTED);
      COMMAND_SCHEDULE(virtualCircuitList[srcVc].commandQueue,
                       virtualCircuitList[srcVc].commandTimer,
                       CMD_ATC);
      COMMAND_SCHEDULE(virtualCircuitList[srcVc].commandQueue,
                       virtualCircuitList[srcVc].commandTimer,
                       CMD_AT_CMDVC);
    }

    if (DISPLAY_VC_STATE)
      printf("-=--=--=- VC %d ALIVE =--=--=-\n", srcVc);
    if (LOG_VC_STATE)
    {
      sprintf(logBuffer, "-=--=--=- VC %d ALIVE =--=--=-\n", srcVc);
      logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
    }
    break;

  case VC_STATE_SEND_UNKNOWN_ACKS:
    if (DEBUG_PC_CMD_ISSUE)
      printf("VC %d SEND_UNKNOWN_ACKS\n", srcVc);
    if (LOG_CMD_SCHEDULE)
    {
      sprintf(logBuffer, "VC %d SEND_UNKNOWN_ACKS\n", srcVc);
      logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
    }
    if (DISPLAY_VC_STATE)
      printf("-=--=-- VC %d ALIVE UA --=--=-\n", srcVc);
    if (LOG_VC_STATE)
    {
      sprintf(logBuffer, "-=--=-- VC %d ALIVE UA --=--=-\n", srcVc);
      logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
    }
    break;

  case VC_STATE_WAIT_SYNC_ACKS:
    if (DEBUG_PC_CMD_ISSUE)
      printf("VC %d WAIT_SYNC_ACKS\n", srcVc);
    if (LOG_CMD_SCHEDULE)
    {
      sprintf(logBuffer, "VC %d WAIT_SYNC_ACKS\n", srcVc);
      logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
    }
    if (DISPLAY_VC_STATE)
      printf("-=--=-- VC %d ALIVE SA --=--=-\n", srcVc);
    if (LOG_VC_STATE)
    {
      sprintf(logBuffer, "-=--=-- VC %d ALIVE SA --=--=-\n", srcVc);
      logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
    }
    break;

  case VC_STATE_WAIT_ZERO_ACKS:
    if (DEBUG_PC_CMD_ISSUE)
      printf("VC %d WAIT_ZERO_ACKS\n", srcVc);
    if (DISPLAY_VC_STATE)
    {
      if (previousState == VC_STATE_CONNECTED)
        printf("-=-=- VC %d DISCONNECTED -=-=-", srcVc);
      printf("-=--=-- VC %d ALIVE ZA --=--=-\n", srcVc);
    }
    if (LOG_VC_STATE)
    {
      if (previousState == VC_STATE_CONNECTED)
      {
        sprintf(logBuffer, "-=-=- VC %d DISCONNECTED -=-=-", srcVc);
        logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
      }
      sprintf(logBuffer, "-=--=-- VC %d ALIVE ZA --=--=-\n", srcVc);
      logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
    }
    break;

  case VC_STATE_CONNECTED:
    if ((previousState == VC_STATE_LINK_DOWN) && (srcVc < MAX_VC)
      && (!COMMAND_PENDING(virtualCircuitList[srcVc].commandQueue, CMD_WAIT_CONNECTED)))
    {
      //Issue the necessary commands when the link is connected
      COMMAND_SCHEDULE(virtualCircuitList[srcVc].commandQueue,
                       virtualCircuitList[srcVc].commandTimer,
                       CMD_WAIT_CONNECTED);
    }
    if (DEBUG_PC_CMD_ISSUE)
      printf("VC %d CONNECTED\n", srcVc);
    if (LOG_CMD_SCHEDULE)
    {
      sprintf(logBuffer, "VC %d CONNECTED\n", srcVc);
      logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
    }
    if ((pcActiveCommand == CMD_ATC) && COMMAND_PENDING(pcCommandQueue, CMD_ATC))
    {
      if (srcVc == pcCommandVc)
        COMMAND_COMPLETE(pcCommandQueue, pcActiveCommand);
      if ((pcCommandVc < MAX_VC) && (virtualCircuitList[pcCommandVc].activeCommand == CMD_ATC))
        COMMAND_COMPLETE(virtualCircuitList[pcCommandVc].commandQueue, virtualCircuitList[srcVc].activeCommand);
    }
    if (DISPLAY_VC_STATE)
      printf("======= VC %d CONNECTED ======\n", srcVc);
    if (LOG_VC_STATE)
    {
      sprintf(logBuffer, "======= VC %d CONNECTED ======\n", srcVc);
      logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
    }
    break;
  }

  //Clear the waiting for command complete if the link fails
  if (waitingForCommandComplete && (newState != VC_STATE_CONNECTED)
    && (srcVc == remoteCommandVc))
  {
    commandStatus = VC_CMD_ERROR;
    waitingForCommandComplete = false;
  }
}

void radioDataAck(VC_SERIAL_MESSAGE_HEADER * header, uint8_t * data, uint8_t length)
{
  VC_DATA_ACK_NACK_MESSAGE * vcMsg;

  vcMsg = (VC_DATA_ACK_NACK_MESSAGE *)data;
  if (DISPLAY_DATA_ACK)
    printf("ACK from VC %d\n", vcMsg->msgDestVc);
  if (LOG_DATA_ACK)
  {
    sprintf(logBuffer, "ACK from VC %d\n", vcMsg->msgDestVc);
    logTimeStampAndData(vcMsg->msgDestVc, logBuffer, strlen(logBuffer));
  }
}

void radioDataNack(VC_SERIAL_MESSAGE_HEADER * header, uint8_t * data, uint8_t length)
{
  int index;
  int vcIndex;
  VC_DATA_ACK_NACK_MESSAGE * vcMsg;

  vcMsg = (VC_DATA_ACK_NACK_MESSAGE *)data;
  vcIndex = vcMsg->msgDestVc & VCAB_NUMBER_MASK;
  if (DISPLAY_DATA_NACK)
    printf("NACK from VC %d\n", vcIndex);
  if (LOG_DATA_NACK)
  {
    sprintf(logBuffer, "NACK from VC %d\n", vcIndex);
    logTimeStampAndData(vcMsg->msgDestVc, logBuffer, strlen(logBuffer));
  }

  //Clear the command queue for this VC
  for (index = 0; index < COMMAND_QUEUE_SIZE; index++)
    virtualCircuitList[vcIndex].commandQueue[index] = 0;
  virtualCircuitList[vcIndex].activeCommand = CMD_LIST_SIZE;

  //Stop the command timer
  virtualCircuitList[vcIndex].commandTimer = 0;

  //Set the VC state to down
  virtualCircuitList[vcIndex].vcState = VC_STATE_LINK_DOWN;
}

void radioRuntime(VC_SERIAL_MESSAGE_HEADER * header, uint8_t * data, uint8_t length)
{
  int index;
  int vcIndex;
  VC_RUNTIME_MESSAGE * vcMsg;

  vcMsg = (VC_RUNTIME_MESSAGE *)data;
  vcIndex = header->radio.srcVc & VCAB_NUMBER_MASK;
  if (DISPLAY_RUNTIME)
    printf("VC %d runtime: %lld, programmed: %lld\n",
           vcIndex, (long long)vcMsg->runtime, (long long)vcMsg->programmed);
  if (LOG_RUNTIME)
  {
    sprintf(logBuffer, "VC %d runtime: %lld, programmed: %lld\n",
           vcIndex, (long long)vcMsg->runtime, (long long)vcMsg->programmed);
    logTimeStampAndData(header->radio.srcVc, logBuffer, strlen(logBuffer));
  }

  //Set the time values
  memcpy(&virtualCircuitList[vcIndex].runtime, &vcMsg->runtime, sizeof(vcMsg->runtime));
  memcpy(&virtualCircuitList[vcIndex].programmed, &vcMsg->programmed, sizeof(vcMsg->programmed));
}

void sendLeakWarning()
{
  printf("%s\n", htmlMessage);
}

void updateWaterUse(VC_SERIAL_MESSAGE_HEADER * header, uint8_t * data, uint8_t length)
{
  WATER_USE deltaGallons;
  int htmlIndex;
  int index;
  static char tempBuffer[sizeof(htmlMessage)];
  int vcIndex;
  int zone;

  //Get the water use message
  vcIndex = header->radio.srcVc & VCAB_NUMBER_MASK;
  if (length > sizeof(WATER_USE))
    length = sizeof(WATER_USE);

  //Determine the delta water use
  memcpy(&deltaGallons, data, length);
  deltaGallons.total -= virtualCircuitList[vcIndex].gallons.total;
  deltaGallons.leaked -= virtualCircuitList[vcIndex].gallons.leaked;
  for (zone = 0; zone < ZONE_NUMBER_MAX; zone++)
    deltaGallons.zone[zone] -= virtualCircuitList[vcIndex].gallons.zone[zone];

  //Check for leak when valves are off
  //This may be a leak between the flow meter and the valves or one or
  //more of the valves may not be turning off fully.
  if (deltaGallons.leaked)
  {
    sprintf(tempBuffer, "The sprinkler controller near %s detected a leak of %d gallons with the valves off.  The leak may be between the water flow meter and the valves or possibly one of the valves is not turning off fully.\n",
           controllerNames[vcIndex], deltaGallons.leaked);

    //Remove #, the PHP comment character
    htmlIndex = 0;
    for (index = 0; index < (int)sizeof(tempBuffer); index++)
    {
      if (tempBuffer[index] != '#')
        htmlMessage[htmlIndex++] = tempBuffer[index];
      if (!tempBuffer[index])
        break;
    }

    //Send the email message
    if (!fork())
    {
      sendLeakWarning();
      exit(0);
    }
  }

  //Update the water use
  memcpy(&virtualCircuitList[vcIndex].gallons, data, length);

  //Display the water use
  if (DISPLAY_WATER_USE)
  {
    printf("VC %d Water Use:\n", vcIndex);
    printf("    Total Gallons: %d\n", virtualCircuitList[vcIndex].gallons.total);
    printf("    Leaked Gallons: %d\n", virtualCircuitList[vcIndex].gallons.leaked);
    for (zone = 0; zone < ZONE_NUMBER_MAX; zone++)
      printf("    Zone  %d Gallons: %d\n", zone + 1, virtualCircuitList[vcIndex].gallons.zone[zone]);
  }
  if (LOG_WATER_USE)
  {
    int logFileIndex = logFileValidateVcIndex(header->radio.srcVc);
    sprintf(logBuffer, "VC %d Water Use:\n", vcIndex);
    logTimeStampAndData(logFileIndex, logBuffer, strlen(logBuffer));
    sprintf(logBuffer, "    Total Gallons: %d\n", virtualCircuitList[vcIndex].gallons.total);
    logTimeStampAndData(logFileIndex, logBuffer, strlen(logBuffer));
    sprintf(logBuffer, "    Leaked Gallons: %d\n", virtualCircuitList[vcIndex].gallons.leaked);
    logTimeStampAndData(logFileIndex, logBuffer, strlen(logBuffer));
    for (zone = 0; zone < ZONE_NUMBER_MAX; zone++)
    {
      sprintf(logBuffer, "    Zone  %d Gallons: %d\n", zone + 1, virtualCircuitList[vcIndex].gallons.zone[zone]);
      logTimeStampAndData(logFileIndex, logBuffer, strlen(logBuffer));
    }
  }
}

void radioCommandComplete(VC_SERIAL_MESSAGE_HEADER * header, uint8_t * data, uint8_t length)
{
  int activeCommand;
  VC_COMMAND_COMPLETE_MESSAGE * vcMsg;
  uint8_t srcVc;

  //The command processor is still running
  commandProcessorRunning = STALL_CHECK_COUNT;

  //Validate the srcVc
  srcVc = header->radio.srcVc;
  if (srcVc >= PC_REMOTE_COMMAND)
  {
    if (srcVc < (uint8_t)VC_RSVD_SPECIAL_VCS)
      srcVc &= VCAB_NUMBER_MASK;
    else
      switch(srcVc)
      {
      default:
        fprintf(stderr, "ERROR: Unknown VC: %d (0x%02x)\n", srcVc, srcVc);
        sprintf(logBuffer, "ERROR: Unknown VC: %d (0x%02x)\n", srcVc, srcVc);
        logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
        exit(-2);
        break;

      //Ignore this command
      case (uint8_t)VC_UNASSIGNED:
        return;
      }
  }

  //Done with this command
  if (srcVc == myVc)
  {
    if (pcActiveCommand < CMD_LIST_SIZE)
    {
      activeCommand = pcActiveCommand;

      //Done with the PC command
      COMMAND_COMPLETE(pcCommandQueue, pcActiveCommand);

      //Determine if a VC command moved to the PC queue
      if ((pcCommandVc < MAX_VC)
        && COMMAND_PENDING(virtualCircuitList[pcCommandVc].commandQueue,
                           activeCommand))
      {
        //Done with the VC command
        COMMAND_COMPLETE(virtualCircuitList[pcCommandVc].commandQueue,
                         virtualCircuitList[pcCommandVc].activeCommand);
      }
    }
    else if (virtualCircuitList[srcVc].activeCommand < CMD_LIST_SIZE)
    {
      //This was a VC command
      COMMAND_COMPLETE(virtualCircuitList[srcVc].commandQueue,
                       virtualCircuitList[srcVc].activeCommand);
    }
  }
  else
  {
    //Finish the programming
    if (virtualCircuitList[srcVc].activeCommand == CMD_ATI12)
      virtualCircuitList[srcVc].programUpdated = virtualCircuitList[srcVc].programmed;

    //This was a VC command
    COMMAND_COMPLETE(virtualCircuitList[srcVc].commandQueue,
                     virtualCircuitList[srcVc].activeCommand);
  }

  vcMsg = (VC_COMMAND_COMPLETE_MESSAGE *)data;
  if (DISPLAY_COMMAND_COMPLETE)
    printf("Command complete from VC %d: %s\n", srcVc,
           (vcMsg->cmdStatus == VC_CMD_SUCCESS) ? "OK" : "ERROR");
  if (LOG_CMD_COMPLETE)
  {
    sprintf(logBuffer, "Command complete from VC %d: %s\n", srcVc,
           (vcMsg->cmdStatus == VC_CMD_SUCCESS) ? "OK" : "ERROR");
    logTimeStampAndData(srcVc, logBuffer, strlen(logBuffer));
  }
  commandStatus = vcMsg->cmdStatus;
  waitingForCommandComplete = false;
}

int commandResponse(uint8_t * data, uint8_t length)
{
  uint8_t * dataStart;
  uint8_t * dataEnd;
  VC_SERIAL_MESSAGE_HEADER * header;
  int status;

  dataEnd = &data[length];
  status = 0;
  do
  {
    dataStart = data;

    //Walk through the command response looking for an embedded VC message
    while ((data < dataEnd) && (*data != START_OF_VC_SERIAL))
      data++;

    //Output the serial data
    length = data - dataStart;
    if (length)
    {
      status = hostToStdout(NULL, dataStart, length);
      if (status)
        break;
    }

    //Determine if there is an embedded VC message
    if (data >= dataEnd)
      break;

    //Verify that the entire VC message is in the buffer
    header = (VC_SERIAL_MESSAGE_HEADER *)data;
    data++; //Skip over the START_OF_VC_SERIAL byte
    if ((data >= dataEnd) || (&data[*data] > dataEnd))
    {
      fprintf(stderr, "ERROR: VC message not fully contained in command response");
      status = -20;
      break;
    }

    //Locate the VC message header and the remainder of the command response
    length = header->radio.length;
    dataStart = &data[VC_RADIO_HEADER_BYTES];
    data += length;
    length -= VC_RADIO_HEADER_BYTES;

    //Display the VC header and message
    if (DEBUG_RADIO_TO_PC)
    {
      printf("VC Header:\n");
      printf("    length: %d\n", header->radio.length);
      printf("    destVc: %d (0x%02x)\n", (uint8_t)header->radio.destVc, (uint8_t)header->radio.destVc);
      printf("    srcVc: %d (0x%02x)\n", header->radio.srcVc, header->radio.srcVc);
      if (length > 0)
        dumpBuffer(dataStart, length);
    }
    if (LOG_RADIO_TO_HOST)
    {
      int logFileIndex = logFileValidateVcIndex(header->radio.srcVc);
      sprintf(logBuffer, "VC Header:\n");
      logTimeStampAndData(logFileIndex, logBuffer, strlen(logBuffer));
      sprintf(logBuffer, "    length: %d\n", header->radio.length);
      logTimeStampAndData(logFileIndex, logBuffer, strlen(logBuffer));
      sprintf(logBuffer, "    destVc: %d (0x%02x)\n", (uint8_t)header->radio.destVc, (uint8_t)header->radio.destVc);
      logTimeStampAndData(logFileIndex, logBuffer, strlen(logBuffer));
      sprintf(logBuffer, "    srcVc: %d (0x%02x)\n", header->radio.srcVc, header->radio.srcVc);
      logTimeStampAndData(logFileIndex, logBuffer, strlen(logBuffer));
      if (length > 0)
        logDumpBuffer(logFileIndex, dataStart, length);
    }

    //------------------------------
    //Process the message
    //------------------------------

    switch (header->radio.destVc)
    {
      //Dump the unknown VC message
      default:
        dumpBuffer((uint8_t*)header, length + VC_SERIAL_HEADER_BYTES);
        break;

      //Display radio runtime
      case PC_RUNTIME:
        radioRuntime(header, dataStart, length);
        break;

      //Update the water use
      case PC_WATER_USE:
        updateWaterUse(header, dataStart, length);
        break;
    }
  } while (data < dataEnd);
  return status;
}

int radioToHost()
{
  int bytesRead;
  int bytesSent;
  int bytesToRead;
  int bytesToSend;
  int logFileIndex;
  static uint8_t * data = outputBuffer;
  static uint8_t * dataStart = outputBuffer;
  static uint8_t * dataEnd = outputBuffer;
  int8_t destAddr;
  static VC_SERIAL_MESSAGE_HEADER * header = (VC_SERIAL_MESSAGE_HEADER *)outputBuffer;
  int length;
  int maxfds;
  int status;
  int8_t srcAddr;
  struct timeval timeout;

  status = 0;
  do
  {
    //Read the virtual circuit header into the local buffer.
    bytesToRead = &outputBuffer[sizeof(outputBuffer)] - dataEnd;
    bytesRead = read(radio, dataEnd, bytesToRead);
    if (bytesRead == 0)
      break;
    if (bytesRead < 0)
    {
      perror("ERROR: Read from radio failed!");
      status = bytesRead;
      break;
    }
    dataEnd += bytesRead;

    //Display the data received from the radio
    if (DUMP_RADIO_TO_PC)
      if (bytesRead)
        dumpBuffer(dataStart, dataEnd - dataStart);

    //The data read is a mix of debug serial output and virtual circuit messages
    //Any data before the VC_SERIAL_MESSAGE_HEADER is considered debug serial output
    data = dataStart;
    while ((data < dataEnd) && (*data != START_OF_VC_SERIAL))
      data++;

    //Process any debug data
    length = data - dataStart;
    if (length)
    {
      if (LOG_RADIO_TO_HOST)
        if (bytesRead)
        {
          logDumpBuffer(VC_PC, dataStart, length);
          fdatasync(logFile[VC_PC]);
        }

      //Output the debug data
      hostToStdout(NULL, dataStart, length);
    }

    //Determine if this is the beginning of a virtual circuit message
    length = dataEnd - data;
    if (length <= 0)
    {
      //Refill the empty buffer
      dataStart = outputBuffer;
      data = dataStart;
      dataEnd = data;
      break;
    }

    //This is the beginning of a virtual circuit message
    //Move it to the beginning of the buffer to make things easier
    if (data != outputBuffer)
      memcpy(outputBuffer, data, length);
    dataEnd = &outputBuffer[length];
    dataStart = outputBuffer;
    data = dataStart;

    //Determine if the VC header is in the buffer
    if (length < (int)VC_SERIAL_HEADER_BYTES)
      //Need more data
      break;

    //Determine if the entire message is in the buffer
    if (length < (header->radio.length + 1))
      //Need more data
      break;

    //Set the beginning of the message
    data = &outputBuffer[VC_SERIAL_HEADER_BYTES];
    length = header->radio.length - VC_RADIO_HEADER_BYTES;

    //Display the VC header and message
    if (DEBUG_RADIO_TO_PC)
    {
      printf("VC Header:\n");
      printf("    length: %d\n", header->radio.length);
      printf("    destVc: %d (0x%02x)\n", (uint8_t)header->radio.destVc, (uint8_t)header->radio.destVc);
      printf("    srcVc: %d (0x%02x)\n", header->radio.srcVc, header->radio.srcVc);
      if (length > 0)
        dumpBuffer(data, length);
    }

    // Log the VC messages
    if (LOG_RADIO_TO_HOST)
    {
      logFileIndex = logFileValidateVcIndex((uint8_t)header->radio.destVc);
      sprintf(logBuffer, "VC Header:\n");
      logTimeStampAndData(logFileIndex, logBuffer, strlen(logBuffer));
      sprintf(logBuffer, "    length: %d\n", header->radio.length);
      logTimeStampAndData(logFileIndex, logBuffer, strlen(logBuffer));
      sprintf(logBuffer, "    destVc: %d (0x%02x)\n", (uint8_t)header->radio.destVc, (uint8_t)header->radio.destVc);
      logTimeStampAndData(logFileIndex, logBuffer, strlen(logBuffer));
      sprintf(logBuffer, "    srcVc: %d (0x%02x)\n", header->radio.srcVc, header->radio.srcVc);
      logTimeStampAndData(logFileIndex, logBuffer, strlen(logBuffer));
      if (length > 0)
        logDumpBuffer(logFileIndex, data, length);
      fdatasync(logFile[logFileIndex]);

      if (logFileIndex != VC_PC)
      {
        sprintf(logBuffer, "VC Header:\n");
        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
        sprintf(logBuffer, "    length: %d\n", header->radio.length);
        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
        sprintf(logBuffer, "    destVc: %d (0x%02x)\n", (uint8_t)header->radio.destVc, (uint8_t)header->radio.destVc);
        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
        sprintf(logBuffer, "    srcVc: %d (0x%02x)\n", header->radio.srcVc, header->radio.srcVc);
        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
        if (length > 0)
          logDumpBuffer(VC_PC, data, length);
        fdatasync(logFile[VC_PC]);
      }
    }

    //------------------------------
    //Process the message
    //------------------------------

    //Display link status
    if (header->radio.destVc == PC_LINK_STATUS)
    {
      if (LOG_RESPONSE_TYPE)
      {
        sprintf(logBuffer, "Response: Link status\n");
        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
      }
      radioToPcLinkStatus(header, data, VC_SERIAL_HEADER_BYTES + length);
    }

    else if (header->radio.destVc == PC_RAIN_STATUS)
    {
      if (LOG_RESPONSE_TYPE)
      {
        sprintf(logBuffer, "Response: Rain status\n");
        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
      }
      rainStatus(header, VC_SERIAL_HEADER_BYTES + length);
    }

    else if (header->radio.destVc == PC_WIND_STATUS)
    {
      if (LOG_RESPONSE_TYPE)
      {
        sprintf(logBuffer, "Response: Wind status\n");
        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
      }
      windStatus(header, VC_SERIAL_HEADER_BYTES + length);
    }

    //Display remote command response
    else if (header->radio.destVc == (PC_REMOTE_RESPONSE | myVc))
    {
      if (LOG_RESPONSE_TYPE)
      {
        sprintf(logBuffer, "Response: Command response\n");
        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
      }
      status = commandResponse(data, length);
    }

    //Display command completion status
    else if (header->radio.destVc == PC_COMMAND_COMPLETE)
    {
      if (LOG_RESPONSE_TYPE)
      {
        sprintf(logBuffer, "Response: Command complete\n");
        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
      }
      radioCommandComplete(header, data, length);
    }

    //Display ACKs for transmitted messages
    else if (header->radio.destVc == PC_DATA_ACK)
    {
      if (LOG_RESPONSE_TYPE)
      {
        sprintf(logBuffer, "Response: ACK\n");
        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
      }
      radioDataAck(header, data, length);
    }

    //Display NACKs for transmitted messages
    else if (header->radio.destVc == PC_DATA_NACK)
    {
      if (LOG_RESPONSE_TYPE)
      {
        sprintf(logBuffer, "Response: NACK\n");
        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
      }
      radioDataNack(header, data, length);
    }

    //Display radio runtime
    else if (header->radio.destVc == PC_RUNTIME)
    {
      if (LOG_RESPONSE_TYPE)
      {
        sprintf(logBuffer, "Response: Runtime\n");
        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
      }
      radioRuntime(header, data, length);
    }

    //Display received messages
    else if ((header->radio.destVc == myVc) || (header->radio.destVc == VC_BROADCAST))
    {
      if (LOG_RESPONSE_TYPE)
      {
        sprintf(logBuffer, "Response: Message\n");
        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
      }

      //Output this message
      status = hostToStdout(header, data, length);
    }

    //Unknown messages
    else
    {
      if (DISPLAY_UNKNOWN_COMMANDS)
      {
        printf("Unknown message, VC Header:\n");
        printf("    length: %d\n", header->radio.length);
        printf("    destVc: %d (0x%02x)\n", (uint8_t)header->radio.destVc, (uint8_t)header->radio.destVc);
        printf("    srcVc: %d (0x%02x)\n", header->radio.srcVc, header->radio.srcVc);
        if (length > 0)
          dumpBuffer(data, length);
      }

      logFileIndex = logFileValidateVcIndex((uint8_t)header->radio.destVc);
      sprintf(logBuffer, "Unknown message, VC Header:\n");
      logTimeStampAndData(logFileIndex, logBuffer, strlen(logBuffer));
      sprintf(logBuffer, "    length: %d\n", header->radio.length);
      logTimeStampAndData(logFileIndex, logBuffer, strlen(logBuffer));
      sprintf(logBuffer, "    destVc: %d (0x%02x)\n", (uint8_t)header->radio.destVc, (uint8_t)header->radio.destVc);
      logTimeStampAndData(logFileIndex, logBuffer, strlen(logBuffer));
      sprintf(logBuffer, "    srcVc: %d (0x%02x)\n", header->radio.srcVc, header->radio.srcVc);
      logTimeStampAndData(logFileIndex, logBuffer, strlen(logBuffer));
      if (length > 0)
        logDumpBuffer(logFileIndex, data, length);
    }

    //Continue processing the rest of the data in the buffer
    if (length > 0)
      data += length;
    dataStart = data;
  } while(0);
  return status;
}

void issuePcCommands()
{
  int cmd;
  int index;

  //Wait until this command completes
  if (pcActiveCommand >= CMD_LIST_SIZE)
  {
    for (index = 0; index < CMD_LIST_SIZE; index += QUEUE_T_BITS)
    {
      if (pcCommandQueue[index / QUEUE_T_BITS])
      {
        for (cmd = index; (cmd < CMD_LIST_SIZE) && (cmd < (index + QUEUE_T_BITS)); cmd++)
        {
          if (COMMAND_PENDING(pcCommandQueue, cmd))
          {
            pcActiveCommand = cmd;
            switch (cmd)
            {
              case CMD_ATI30: //Get myVC
                if (myVc == VC_UNASSIGNED)
                {
                  //Get myVc address
                  if (DEBUG_PC_CMD_ISSUE)
                    printf("Issuing ATI30 command\n");
                  if (LOG_CMD_ISSUE)
                  {
                    sprintf(logBuffer, "Issuing ATI30 command\n");
                    logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
                  }
                  findMyVc = true;
                  cmdToRadio((uint8_t *)GET_MY_VC_ADDRESS, strlen(GET_MY_VC_ADDRESS));
                  return;
                }
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Skipping ATI30 command, myVC already known\n");
                if (LOG_CMD_ISSUE)
                {
                  sprintf(logBuffer, "Skipping ATI30 command, myVC already known\n");
                  logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
                }
                COMMAND_COMPLETE(pcCommandQueue, pcActiveCommand);
                break;

              case CMD_ATB: //Break all of the VC links
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Issuing ATB command\n");
                if (LOG_CMD_ISSUE)
                {
                  sprintf(logBuffer, "Issuing ATB command\n");
                  logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
                }
                cmdToRadio((uint8_t *)BREAK_LINKS_COMMAND, strlen(BREAK_LINKS_COMMAND));
                return;

              case CMD_GET_SERVER_MODEL:
                //Get the server model name
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Issuing %s command\n", GET_SERVER_MODEL);
                if (LOG_CMD_ISSUE)
                {
                  sprintf(logBuffer, "Issuing %s command\n", GET_SERVER_MODEL);
                  logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
                }
                cmdToRadio((uint8_t *)GET_SERVER_MODEL, strlen(GET_SERVER_MODEL));
                return;

              case CMD_ATI:
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Issuing ATI command\n");
                if (LOG_CMD_ISSUE)
                {
                  sprintf(logBuffer, "Issuing ATI command\n");
                  logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
                }
                cmdToRadio((uint8_t *)GET_DEVICE_INFO, strlen(GET_DEVICE_INFO));
                return;

              case CMD_ATI8:
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Issuing ATI8 command\n");
                if (LOG_CMD_ISSUE)
                {
                  sprintf(logBuffer, "Issuing ATI8 command\n");
                  logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
                }
                cmdToRadio((uint8_t *)GET_UNIQUE_ID, strlen(GET_UNIQUE_ID));
                return;

              case CMD_ATA: //Get all the VC states
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Issuing ATA command\n");
                if (LOG_CMD_ISSUE)
                {
                  sprintf(logBuffer, "Issuing ATA command\n");
                  logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
                }
                cmdToRadio((uint8_t *)GET_VC_STATUS, strlen(GET_VC_STATUS));
                return;

              case CMD_AT_CMDVC: //Select the VC to target
              case CMD_AT_CMDVC_2:
                //Select the VC to use
                sprintf(pcCommandBuffer, "at-CmdVc=%d", pcCommandVc);
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Issuing %s command\n", pcCommandBuffer);
                if (LOG_CMD_ISSUE)
                {
                  sprintf(logBuffer, "Issuing %s command\n", pcCommandBuffer);
                  logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
                }
                cmdToRadio((uint8_t *)pcCommandBuffer, strlen(pcCommandBuffer));
                return;

              case CMD_ATC: //Perform the 3-way handshake
                //Bring up the VC connection to this remote system
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Issuing ATC command\n");
                if (LOG_CMD_ISSUE)
                {
                  sprintf(logBuffer, "Issuing ATC command\n");
                  logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
                }
                cmdToRadio((uint8_t *)START_3_WAY_HANDSHAKE, strlen(START_3_WAY_HANDSHAKE));
                return;

              case CMD_ATI31:
                //Get the VC state
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Issuing ATI31 command\n");
                if (LOG_CMD_ISSUE)
                {
                  sprintf(logBuffer, "Issuing ATI31 command\n");
                  logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
                }
                cmdToRadio((uint8_t *)GET_VC_STATE, strlen(GET_VC_STATE));
                return;
            }
          }
        }
      }
    }

    //No more PC commands to process
    if (DEBUG_CMD_ISSUE && pcCommandTimer)
      printf("PC command list empty\n");
    if (LOG_CMD_ISSUE && pcCommandTimer)
    {
      sprintf(logBuffer, "PC command list empty\n");
      logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
    }
    pcActiveCommand = CMD_LIST_SIZE;
    pcCommandTimer = 0;
    pcCommandVc = MAX_VC;
  }
}

int sendVcCommand(const char * commandString, int destVc)
{
  int bytesSent;
  int bytesWritten;
  VC_SERIAL_MESSAGE_HEADER header;
  int length;

  //Build the virtual circuit serial header
  length = strlen(commandString);
  header.start = START_OF_VC_SERIAL;
  header.radio.length = VC_RADIO_HEADER_BYTES + length;
  header.radio.destVc = PC_REMOTE_COMMAND | destVc;
  header.radio.srcVc = myVc;

  //Display the data being sent to the radio
  if (DEBUG_PC_TO_RADIO)
  {
    dumpBuffer((uint8_t *)&header, VC_SERIAL_HEADER_BYTES);
    dumpBuffer((uint8_t *)commandString, length);
  }
  if (DEBUG_LOCAL_COMMANDS)
    printf("Sending LoRaSerial command: %s\n", commandString);
  if (DEBUG_PC_CMD_ISSUE)
    printf("Sending %s command to VC %d\n", commandString, destVc);
  if (LOG_CMD_ISSUE)
  {
    sprintf(logBuffer, "Sending %s command to VC %d\n", commandString, destVc);
    logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
    sprintf(logBuffer, "Sending %s command to VC %d\n", commandString, destVc);
    logTimeStampAndData(destVc, logBuffer, strlen(logBuffer));
  }

  //Send the header
  bytesWritten = write(radio, (uint8_t *)&header, VC_SERIAL_HEADER_BYTES);
  if (bytesWritten < (int)VC_SERIAL_HEADER_BYTES)
  {
    perror("ERROR: Write of header to radio failed!");
    return -1;
  }

  //Send the message
  bytesSent = 0;
  while (bytesSent < length)
  {
    bytesWritten = write(radio, &commandString[bytesSent], length - bytesSent);
    if (bytesWritten < 0)
    {
      perror("ERROR: Write of data to radio failed!");
      return bytesWritten;
    }
    bytesSent += bytesWritten;
  }

  //Return the amount of the buffer that was sent
  return length;
}

bool commandProcessorBusy()
{
  bool busy;
  int index;

  //Determine if the command processor is using the VC
  busy = (pcActiveCommand < CMD_LIST_SIZE);
  for (index = 0; index < COMMAND_QUEUE_SIZE; index++)
    if (pcCommandQueue[index])
    {
      busy = 1;
      break;
    }
  return busy;
}

bool commandProcessorIdle(int vcIndex)
{
  bool idle;

  //Determine if the command processor is using the VC
  idle = !commandProcessorBusy();
  if (idle)
    //Set the command VC
    pcCommandVc = vcIndex;

  //Return the idle status
  return idle;
}

bool issueVcCommands(int vcIndex)
{
  int cmd;
  uint32_t dayBit;
  int dayIndex;
  int durationBase;
  static int entry;
  int index;
  time_t now;
  struct tm * timeStruct;
  uint32_t zoneBit;
  int zoneIndex;

  if (virtualCircuitList[vcIndex].activeCommand >= CMD_LIST_SIZE)
  {
    for (index = 0; index < CMD_LIST_SIZE; index += QUEUE_T_BITS)
    {
      if (virtualCircuitList[vcIndex].commandQueue[index / QUEUE_T_BITS])
      {
        for (cmd = index; (cmd < CMD_LIST_SIZE) && (cmd < (index + QUEUE_T_BITS)); cmd++)
        {
          if (COMMAND_PENDING(virtualCircuitList[vcIndex].commandQueue, cmd))
          {
            virtualCircuitList[vcIndex].activeCommand = cmd;
            switch (cmd)
            {
              case CMD_AT_CMDVC:
                //Determine if the local command processor is idle
                if (commandProcessorIdle(vcIndex))
                {
                  if (DEBUG_PC_CMD_ISSUE)
                    printf("Migrating AT-CMDVC=%d and ATC commands to PC command queue\n", vcIndex);
                  if (LOG_CMD_SCHEDULE)
                  {
                    sprintf(logBuffer, "Migrating AT-CMDVC=%d and ATC commands to PC command queue\n", vcIndex);
                    logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
                    if (vcIndex != VC_PC)
                    {
                      sprintf(logBuffer, "Migrating AT-CMDVC=%d and ATC commands to PC command queue\n", vcIndex);
                      logTimeStampAndData(vcIndex, logBuffer, strlen(logBuffer));
                    }
                  }
                  if (COMMAND_PENDING(virtualCircuitList[vcIndex].commandQueue, CMD_ATC))
                    COMMAND_SCHEDULE(pcCommandQueue, pcCommandTimer, CMD_ATC);
                  COMMAND_SCHEDULE(pcCommandQueue, pcCommandTimer, CMD_AT_CMDVC);
                  return true;
                }
                virtualCircuitList[vcIndex].activeCommand = CMD_LIST_SIZE;
                return true;

              case CMD_ATC:
                return true;

              case CMD_WAIT_CONNECTED:
                if ((virtualCircuitList[vcIndex].vcState != VC_STATE_CONNECTED)
                    || (!commandProcessorIdle(vcIndex)))
                {
                  //Mark the list as empty to allow this entry to be executed again
                  virtualCircuitList[vcIndex].activeCommand = CMD_LIST_SIZE;
                  virtualCircuitList[vcIndex].commandTimer = timeoutCount;
                  return true;
                }

                //The wait is complete, this VC is now connected to the sprinkler server
                COMMAND_COMPLETE(virtualCircuitList[vcIndex].commandQueue,
                                 virtualCircuitList[vcIndex].activeCommand);

                //Get the sprinkler controller information
                if (vcIndex != myVc)
                {
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CHECK_FOR_UPDATE);
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_ATI11);
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_ATI8_2);
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_ATI_2);
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_GET_CLIENT_MODEL);
                }

                //Get the VC state
                if (vcIndex != myVc)
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_ATI31);
                COMMAND_SCHEDULE(pcCommandQueue, pcCommandTimer, CMD_ATI31);
                if (vcIndex != myVc)
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_AT_CMDVC_2);
                COMMAND_SCHEDULE(pcCommandQueue, pcCommandTimer, CMD_AT_CMDVC_2);
                break;

              case CMD_AT_CMDVC_2:
                return true;

              case CMD_ATI31:
                return true;

              case CMD_GET_CLIENT_MODEL:
                //Get the client model name
                sendVcCommand(GET_CLIENT_MODEL, vcIndex);
                return true;

              case CMD_ATI_2:
                sendVcCommand(GET_DEVICE_INFO, vcIndex);
                return true;

              case CMD_ATI8_2:
                sendVcCommand(GET_UNIQUE_ID, vcIndex);
                return true;

              case CMD_ATI11:
                sendVcCommand(GET_RUNTIME, vcIndex);
                return true;

              case CHECK_FOR_UPDATE:
                //Done with the CHECK_FOR_UPDATE command
                COMMAND_COMPLETE(virtualCircuitList[vcIndex].commandQueue,
                                 virtualCircuitList[vcIndex].activeCommand);

                //Determine if the sprinkler controller needs to be programmed
                if ((!virtualCircuitList[vcIndex].programUpdated)
                  || (virtualCircuitList[vcIndex].programUpdated > virtualCircuitList[vcIndex].programmed))
                {
                  //Complete the programming
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   PROGRAMMING_COMPLETED);
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_ATI12);

                  //Make sure the sprinkler controller is enabled
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_AT_ENABLE_CONTROLLER);

                  //Set the duration for all of the zones for each day of the week
                  durationBase = vcIndex * DAYS_IN_WEEK * ZONE_NUMBER_MAX;
                  memset(&setDurations[durationBase], 0xff, DAYS_IN_WEEK * ZONE_NUMBER_MAX);
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_SET_DAY_OF_WEEK_2);

                  //Set the start times for each day of the week
                  setStartTimes[vcIndex] = (1 << DAYS_IN_WEEK) - 1;
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_SET_DAY_OF_WEEK);

                  //Configure the sprinkler controller to properly drive the zone solenoids
                  configureSolenoids[vcIndex] = ZONE_MASK;
                  manualZones[vcIndex] = ZONE_MASK;
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_SELECT_ZONE);

                  //Get the previous water use
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_GET_WATER_USE);

                  //Set the time of day
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_ATI89);
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_AT_TIME_OF_DAY);
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_AT_DAY_OF_WEEK);
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_AT_DISABLE_CONTROLLER);
                }
                return true;

              case CMD_AT_DISABLE_CONTROLLER:
                //Disable the sprinkler controller
                sendVcCommand(DISABLE_CONTROLLER, vcIndex);
                return true;

              case CMD_AT_DAY_OF_WEEK:
                //Set the day of week
                time(&now);
                timeStruct = localtime(&now);
                sprintf(vcCommandBuffer[vcIndex], "AT-DayOfWeek=%d", timeStruct->tm_wday);
                sendVcCommand(vcCommandBuffer[vcIndex], vcIndex);
                return true;

              case CMD_AT_TIME_OF_DAY:
                //Set the time of day
                time(&now);
                timeStruct = localtime(&now);
                uint32_t milliseconds = (timeStruct->tm_sec * MILLIS_IN_SECOND)
                                      + (timeStruct->tm_min * MILLIS_IN_MINUTE)
                                      + (timeStruct->tm_hour * MILLIS_IN_HOUR);
                sprintf(vcCommandBuffer[vcIndex], "AT-TimeOfDay=%d", milliseconds);
                sendVcCommand(vcCommandBuffer[vcIndex], vcIndex);
                return true;

              case CMD_ATI89:
                //Display the date and time
                sendVcCommand(DISPLAY_DATE_TIME, vcIndex);
                return true;

              case CMD_GET_WATER_USE:
                //Get the water use
                sendVcCommand(GET_WATER_USE, vcIndex);
                return true;

              //Select the zone
              case CMD_SELECT_ZONE:
                //Locate the next zone to configure
                for (zoneIndex = 0; zoneIndex < ZONE_NUMBER_MAX; zoneIndex++)
                {
                  //Determine if this zone needs configuration
                  zoneBit = 1 << zoneIndex;
                  if ((configureSolenoids[vcIndex] | manualZones[vcIndex]) & zoneBit)
                  {
                    //Send the command to select the zone
                    sprintf(vcCommandBuffer[vcIndex], "%s%d", SET_COMMAND_ZONE, zoneIndex + 1);
                    sendVcCommand(vcCommandBuffer[vcIndex], vcIndex);

                    //Complete the sequence for this zone
                    COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                     virtualCircuitList[vcIndex].commandTimer,
                                     COMPLETE_ZONE_CONFIGURATION);

                    //Issue the command to manually control the zone
                    if (manualZones[vcIndex] & zoneBit)
                    {
                      manualZones[vcIndex] &= ~zoneBit;
                      manualControl[vcIndex] = (manualOn[vcIndex] >> zoneIndex) & 1;
                      COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                       virtualCircuitList[vcIndex].commandTimer,
                                       CMD_SET_MANUAL_ON);
                    }

                    //Issue the command to select the solenoid type
                    if (configureSolenoids[vcIndex] & zoneBit)
                    {
                      configureSolenoids[vcIndex] &= ~zoneBit;
                      solenoidType[vcIndex] = (latchingSolenoid[vcIndex] >> zoneIndex) & 1;
                      COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                       virtualCircuitList[vcIndex].commandTimer,
                                       CMD_SELECT_SOLENOID);
                    }
                    return true;
                  }
                }
                COMMAND_COMPLETE(virtualCircuitList[vcIndex].commandQueue,
                                 virtualCircuitList[vcIndex].activeCommand);
                break;

              //Select the solenoid type for the zone
              case CMD_SELECT_SOLENOID:
                //Send the command to specify the solenoid type
                sprintf(vcCommandBuffer[vcIndex], "%s%d", SET_SOLENOID_TYPE, solenoidType[vcIndex]);
                sendVcCommand(vcCommandBuffer[vcIndex], vcIndex);
                return true;

              case CMD_SET_MANUAL_ON:
                //Send the command to manually turn on or off the zone valve
                sprintf(vcCommandBuffer[vcIndex], "%s%d", SET_MANUAL_ON, manualControl[vcIndex]);
                sendVcCommand(vcCommandBuffer[vcIndex], vcIndex);
                return true;

              case CMD_SET_MSEC_PER_INCH:
                //Send the command to set the milliseconds per inch
                sprintf(vcCommandBuffer[vcIndex], "%s%d", SET_MSEC_PER_INCH, mSecPerInch[entry]);
                sendVcCommand(vcCommandBuffer[vcIndex], vcIndex);
                return true;

              //Determine if all of the zones are configured
              case COMPLETE_ZONE_CONFIGURATION:
                COMMAND_COMPLETE(virtualCircuitList[vcIndex].commandQueue,
                                 virtualCircuitList[vcIndex].activeCommand);
                if (configureSolenoids[vcIndex] | manualZones[vcIndex])
                {
                  //Configure the next zone
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_SELECT_ZONE);
                  return true;
                }
                break;

              //Set the day of the week
              case CMD_SET_DAY_OF_WEEK:
                //Locate the next zone to configure
                for (dayIndex = 0; dayIndex < DAYS_IN_WEEK; dayIndex++)
                {
                  //Determine if this zone needs configuration
                  dayBit = 1 << dayIndex;
                  if (!(setStartTimes[vcIndex] & dayBit))
                    continue;
                  setStartTimes[vcIndex] &= ~dayBit;

                  //Send the command to select the day
                  sprintf(vcCommandBuffer[vcIndex], "%s%d", SET_COMMAND_DAY, dayIndex);
                  sendVcCommand(vcCommandBuffer[vcIndex], vcIndex);

                  //Issue the command to set the start time
                  dayNumber[vcIndex] = dayIndex;
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_SET_ALL_START_TIMES);
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_SET_START_TIME);
                  return true;
                }
                COMMAND_COMPLETE(virtualCircuitList[vcIndex].commandQueue,
                                 virtualCircuitList[vcIndex].activeCommand);
                break;

              //Set the start time
              case CMD_SET_START_TIME:
                //Send the command to specify the start time
                sprintf(vcCommandBuffer[vcIndex], "%s%d", SET_START_TIME, startTimeArray[(vcIndex * DAYS_IN_WEEK) + dayNumber[vcIndex]]);
                sendVcCommand(vcCommandBuffer[vcIndex], vcIndex);
                return true;

              //Determine if all of the start times are set
              case CMD_SET_ALL_START_TIMES:
                COMMAND_COMPLETE(virtualCircuitList[vcIndex].commandQueue,
                                 virtualCircuitList[vcIndex].activeCommand);
                if (setStartTimes[vcIndex])
                {
                  //Configure the next zone
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_SET_DAY_OF_WEEK);
                  return true;
                }
                break;

              case CMD_SET_DAY_OF_WEEK_2:
                //Locate the next day of the week
                for (dayIndex = 0; dayIndex < DAYS_IN_WEEK; dayIndex++)
                {
                  //Determine if this day has any zones that need the durations set
                  for (zoneIndex = 0; zoneIndex < ZONE_NUMBER_MAX; zoneIndex++)
                  {
                    entry = (vcIndex * DAYS_IN_WEEK * ZONE_NUMBER_MAX)
                          + (dayIndex * ZONE_NUMBER_MAX)
                          + zoneIndex;
                    if (setDurations[entry])
                      break;
                  }
                  if (zoneIndex >= ZONE_NUMBER_MAX)
                    continue;

                  //Send the command to select the day
                  sprintf(vcCommandBuffer[vcIndex], "%s%d", SET_COMMAND_DAY, dayIndex);
                  sendVcCommand(vcCommandBuffer[vcIndex], vcIndex);

                  //Issue the command to set the start time
                  dayNumber[vcIndex] = dayIndex;
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_SELECT_ZONE_2);
                  return true;
                }
                dayNumber[vcIndex] = dayIndex;
                COMMAND_COMPLETE(virtualCircuitList[vcIndex].commandQueue,
                                 virtualCircuitList[vcIndex].activeCommand);
                break;

              //Select the zone
              case CMD_SELECT_ZONE_2:
                //Locate the next zone to set the duration
                for (zoneIndex = 0; zoneIndex < ZONE_NUMBER_MAX; zoneIndex++)
                {
                  //Determine if this zone needs the duration set
                  entry = (vcIndex * DAYS_IN_WEEK * ZONE_NUMBER_MAX)
                        + (dayNumber[vcIndex] * ZONE_NUMBER_MAX)
                        + zoneIndex;
                  if (!setDurations[entry])
                    continue;
                  setDurations[entry] = 0;

                  //Send the command to select the zone
                  sprintf(vcCommandBuffer[vcIndex], "%s%d", SET_COMMAND_ZONE, zoneIndex + 1);
                  sendVcCommand(vcCommandBuffer[vcIndex], vcIndex);

                  //Save the zone number
                  zoneNumber[vcIndex] = zoneIndex + 1;
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_SET_ALL_DURATIONS);
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_SET_ZONE_DURATION);
                  return true;
                }
                COMMAND_COMPLETE(virtualCircuitList[vcIndex].commandQueue,
                                 virtualCircuitList[vcIndex].activeCommand);
                zoneNumber[vcIndex] = zoneIndex + 1;
                break;

              //Set the zone duration
              case CMD_SET_ZONE_DURATION:
                //Send the command to specify the zone duration
                entry = (vcIndex * DAYS_IN_WEEK * ZONE_NUMBER_MAX)
                      + (dayNumber[vcIndex] * ZONE_NUMBER_MAX)
                      + zoneNumber[vcIndex] - 1;
                sprintf(vcCommandBuffer[vcIndex], "%s%d", SET_ZONE_DURATION, durationArray[entry]);
                sendVcCommand(vcCommandBuffer[vcIndex], vcIndex);
                return true;

              //Determine if all of the durations are set
              case CMD_SET_ALL_DURATIONS:
                COMMAND_COMPLETE(virtualCircuitList[vcIndex].commandQueue,
                                 virtualCircuitList[vcIndex].activeCommand);
                if (zoneNumber[vcIndex] < ZONE_NUMBER_MAX)
                {
                  //Set the duration for the next zone
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_SET_ALL_DURATIONS);
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_SELECT_ZONE_2);
                  return true;
                }
                if (dayNumber[vcIndex] < (DAYS_IN_WEEK- 1))
                {
                  //Configure the next zone
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_SET_ALL_DURATIONS);
                  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                                   virtualCircuitList[vcIndex].commandTimer,
                                   CMD_SET_DAY_OF_WEEK_2);
                  return true;
                }
                break;

              case CMD_AT_ENABLE_CONTROLLER:
                //Disable the sprinkler controller
                sendVcCommand(ENABLE_CONTROLLER, vcIndex);
                return true;

              case CMD_ATI12:
                sendVcCommand(SET_PROGRAM_COMPLETE, vcIndex);
                return true;

              case PROGRAMMING_COMPLETED:
                virtualCircuitList[vcIndex].programmed = virtualCircuitList[vcIndex].runtime;
                COMMAND_COMPLETE(virtualCircuitList[vcIndex].commandQueue,
                                 virtualCircuitList[vcIndex].activeCommand);
                return true;
            }
          }
        }
      }

      //Done processing VC commands
      if (DEBUG_CMD_ISSUE && virtualCircuitList[vcIndex].commandTimer)
        printf ("VC %d command list empty\n", vcIndex);
      if (LOG_CMD_SCHEDULE && virtualCircuitList[vcIndex].commandTimer)
      {
        sprintf (logBuffer, "VC %d command list empty\n", vcIndex);
        logTimeStampAndData(vcIndex, logBuffer, strlen(logBuffer));
      }
      virtualCircuitList[vcIndex].activeCommand = CMD_LIST_SIZE;
      virtualCircuitList[vcIndex].commandTimer = 0;
    }
  }
  return false;
}

int getControllerNames(MYSQL * mysql, char ** names, char ** ids, int * zones, bool debug)
{
  char * id;
  int index;
  char * name;
  MYSQL_RES * results;
  MYSQL_ROW row;
  int status;
  unsigned int vcIndex;
  int zoneCount;

  do
  {
    for (vcIndex = 0; vcIndex < MAX_VC; vcIndex++)
    {
      //Free the names
      if (names[vcIndex])
      {
        free(names[vcIndex]);
        names[vcIndex] = NULL;
      }

      //Free the IDs
      if (ids[vcIndex])
      {
        free(ids[vcIndex]);
        ids[vcIndex] = NULL;
      }

      //Assume no zones
      controllerZones[vcIndex] = 0;
    }

    //Perform the query
    status = databaseQuery(mysql,
                           "SELECT VcNumber, SprinklerControllerName, RadioId, MaxZones FROM view_sprinkler_controller WHERE (VcNumber IS NOT NULL) ORDER BY VcNumber",
                           &results, NULL, NULL, NULL, debug);
    if (status)
      break;

    //Walk through the results
    while ((row = databaseGetNextRow(results)))
    {
      //Parse the data
      if ((databaseGetInteger(row[0], (int *)&vcIndex))
          && row[1]
          && row[2]
          && databaseGetInteger(row[3], &zoneCount)
          && (vcIndex < MAX_VC))
      {
        //Copy the name
        name = malloc(strlen(row[1]) + 1);
        if (!name)
          break;
        strcpy(name, row[1]);
        names[vcIndex] = name;

        //Copy the ID
        id = malloc(strlen(row[2]) + 1);
        if (!id)
          break;
        strcpy(id, row[2]);
        ids[vcIndex] = id;

        //Save the zone count
        zones[vcIndex] = zoneCount;
        if (DEBUG_SPRINKLER_CHANGES)
          printf("VC %d: %d zone controller near %s has ID: %s \n",
                 vcIndex, zoneCount, name, id);
        if (LOG_SPRINKLER_CHANGES)
        {
          sprintf(logBuffer, "VC %d: %d zone controller near %s has ID: %s \n",
                 vcIndex, zoneCount, name, id);
          logTimeStampAndData(vcIndex, logBuffer, strlen(logBuffer));
        }
      }
    }

    //Done with the results
    databaseFreeResults(results);
  } while (0);
  return status;
}

int getZoneConfiguration(MYSQL * mysql, ZONE_T * latching, ZONE_T * on,
                         uint32_t * millisPerInch, bool debug)
{
  uint32_t factor;
  int index;
  unsigned int onOff;
  MYSQL_RES * results;
  MYSQL_ROW row;
  int status;
  unsigned int type;
  unsigned int vc;
  int zone;

  do
  {
    //Assume all latching relays
    memset(latching, 0xff, MAX_VC * sizeof(*latchingSolenoid));

    //Perform the query
    status = databaseQuery(mysql,
                           "SELECT VcNumber, ZoneNumber, Latching, ManualOn, MillisecondsPerInch, SprinklerControllerName FROM view_sprinkler_valve_2 WHERE (VcNumber IS NOT NULL) ORDER BY VcNumber, ZoneNumber",
                           &results, NULL, NULL, NULL, debug);
    if (status)
      break;

    //Walk through the results
    while ((row = databaseGetNextRow(results)))
    {
      //Parse the data
      if ((databaseGetInteger(row[0], (int *)&vc))
          && (databaseGetInteger(row[1], &zone))
          && (databaseGetInteger(row[2], (int *)&type))
          && (databaseGetInteger(row[3], (int *)&onOff))
          && (databaseGetInteger(row[4], (int *)&factor))
          && (vc < MAX_VC)
          && (zone >= 1) && (zone <= ZONE_NUMBER_MAX)
          && (onOff <= 1))
      {
        index = zone - 1; //Zone numbers: 1 - max, bit map index starts at zero
        latching[vc] &= ~(1 << index);
        latching[vc] |= (type & 1) << index;
        on[vc] &= ~(1 << index);
        on[vc] |= (onOff & 1) << index;
        mSecPerInch[(vc * ZONE_NUMBER_MAX) + index] = factor;
        setMSecPerInch[(vc * ZONE_NUMBER_MAX) + index] = true;
        if (DEBUG_SPRINKLER_CHANGES)
          printf("VC: %s, Zone: %s, Latching: %d, On: %d\n", row[0], row[1], type, onOff);
        if (LOG_SPRINKLER_CHANGES)
        {
          sprintf(logBuffer, "VC: %s, Zone: %s, Latching: %d, On: %d\n", row[0], row[1], type, onOff);
          logTimeStampAndData(vc, logBuffer, strlen(logBuffer));
        }
      }
    }

    //Done with the results
    databaseFreeResults(results);
  } while (0);
  return status;
}

//Get the sprinkler controller start times
int getSprinklerControllerStartTimes(MYSQL * mysql, int32_t * startTimeArray, bool debug)
{
  int day;
  int dayOfWeek;
  int hours;
  int index;
  int milliseconds;
  int minutes;
  MYSQL_RES * results;
  MYSQL_ROW row;
  int seconds;
  int32_t startTime;
  int status;
  unsigned int value;
  unsigned int vc;

  do
  {
    //Assume start time is midnight
    memset(startTimeArray, 0, START_TIME_ENTRIES * sizeof(*startTimeArray));

    //Perform the query
    status = databaseQuery(mysql,
                           "SELECT VcNumber, DayOfWeek, StartTime, SprinklerControllerName FROM view_sprinkler_controller AS vsc, sprinkler_schedule AS ss WHERE ((vsc.SprinklerControllerId = ss.SprinklerControllerId) AND (StartTime IS NOT NULL)) ORDER BY VcNumber, DayOfWeek, StartTime",
                           &results, NULL, NULL, NULL, debug);
    if (status)
      break;

    //Collect data each morning
    for (vc = 1; vc < MAX_VC; vc++)
    {
      for (dayOfWeek = 0; dayOfWeek < DAYS_IN_WEEK; dayOfWeek++)
      {
        virtualCircuitList[vc].collectionTimeSec[dayOfWeek] = 0;
        virtualCircuitList[vc].collectData[dayOfWeek] = true;
        virtualCircuitList[vc].collectData5Am[dayOfWeek] = true;
      }
    }

    //Walk through the results
    while ((row = databaseGetNextRow(results)))
    {
      //Parse the data
      if ((databaseGetInteger(row[0], (int *)&vc))
          && (databaseGetInteger(row[1], &dayOfWeek))
          && row[2]
          && (databaseGetTime(row[2], &hours, &minutes, &seconds))
          && (vc < MAX_VC)
          && (dayOfWeek >= 0) && (dayOfWeek < DAYS_IN_WEEK)
          && (hours >= 0) && (hours <= 23)
          && (minutes >= 0) && (minutes <= 59)
          && (seconds >= 0) && (seconds <= 59))
      {
        if (DEBUG_SPRINKLER_CHANGES)
          printf("VC: %d, DayOfWeek: %s, StartTime: %d:%02d:%02d\n",
                 vc, dayName[dayOfWeek], hours, minutes, seconds);
        if (LOG_SPRINKLER_CHANGES)
        {
          sprintf(logBuffer, "VC: %d, DayOfWeek: %s, StartTime: %d:%02d:%02d\n",
                 vc, dayName[dayOfWeek], hours, minutes, seconds);
          logTimeStampAndData(vc, logBuffer, strlen(logBuffer));
        }
        milliseconds = (hours * MILLIS_IN_HOUR)
                     + (minutes * MILLIS_IN_MINUTE)
                     + (seconds * MILLIS_IN_SECOND);
        startTimeArray[(vc * DAYS_IN_WEEK) + dayOfWeek] = milliseconds;
        day = (dayOfWeek + 1) % DAYS_IN_WEEK;
        virtualCircuitList[vc].collectionTimeSec[day] = milliseconds;
      }
    }

    //Done with the results
    databaseFreeResults(results);
  } while (0);
  return status;
}

//Get the sprinkler controller start times
int getZoneDurations(MYSQL * mysql, int32_t * durationArray, bool debug)
{
  int day;
  int dayOfWeek;
  int32_t duration;
  int hours;
  int index;
  int milliseconds;
  int minutes;
  MYSQL_RES * results;
  MYSQL_ROW row;
  int seconds;
  int status;
  unsigned int value;
  unsigned int vc;
  int zone;

  do
  {
    //Assume the zone if off (duration = 0)
    memset(durationArray, 0, DURATION_ENTRIES * sizeof(*startTimeArray));

    //Perform the query
    status = databaseQuery(mysql,
                           "SELECT VcNumber, DayOfWeek, ZoneNumber, Duration, SprinklerControllerName FROM view_sprinkler_controller AS vsc, sprinkler_schedule AS ss, zone_schedule AS zs WHERE ((vsc.SprinklerControllerId = ss.SprinklerControllerId) AND (ss.SprinklerScheduleId = zs.SprinklerScheduleId) AND (StartTime IS NOT NULL) AND (Duration IS NOT NULL)) ORDER BY VcNumber, DayOfWeek, ZoneNumber, Duration",
                           &results, NULL, NULL, NULL, debug);
    if (status)
      break;

    //Walk through the results
    while ((row = databaseGetNextRow(results)))
    {
      //Parse the data
      if ((databaseGetInteger(row[0], (int *)&vc))
          && (databaseGetInteger(row[1], &dayOfWeek))
          && (databaseGetInteger(row[2], &zone))
          && row[3]
          && (databaseGetTime(row[3], &hours, &minutes, &seconds))
          && (vc < MAX_VC)
          && (dayOfWeek >= 0) && (dayOfWeek < DAYS_IN_WEEK)
          && (zone >= 1) && (zone <= ZONE_NUMBER_MAX)
          && (hours >= 0) && (hours <= 23)
          && (minutes >= 0) && (minutes <= 59)
          && (seconds >= 0) && (seconds <= 59))
      {
        if (DEBUG_SPRINKLER_CHANGES)
          printf("VC: %d, DayOfWeek: %s, Zone: %d, Duration: %d:%02d:%02d\n",
                 vc, dayName[dayOfWeek], zone, hours, minutes, seconds);
        if (LOG_SPRINKLER_CHANGES)
        {
          sprintf(logBuffer, "VC: %d, DayOfWeek: %s, Zone: %d, Duration: %d:%02d:%02d\n",
                 vc, dayName[dayOfWeek], zone, hours, minutes, seconds);
          logTimeStampAndData(vc, logBuffer, strlen(logBuffer));
        }
        milliseconds = (hours * MILLIS_IN_HOUR)
                     + (minutes * MILLIS_IN_MINUTE)
                     + (seconds * MILLIS_IN_SECOND);
        index = (((vc * DAYS_IN_WEEK) + dayOfWeek) * ZONE_NUMBER_MAX) + zone - 1;
        durationArray[index] = milliseconds;
        day = (dayOfWeek + 1) % DAYS_IN_WEEK;
        virtualCircuitList[vc].collectionTimeSec[day] += milliseconds;
      }
    }

    //Collect data each morning
    for (vc = 1; vc < MAX_VC; vc++)
    {
      for (dayOfWeek = 0; dayOfWeek < DAYS_IN_WEEK; dayOfWeek++)
      {
        //Determine the collection time
        day = (dayOfWeek + 1) % DAYS_IN_WEEK;
        virtualCircuitList[vc].collectionTimeSec[day] += 15 * MILLIS_IN_MINUTE;
        if (virtualCircuitList[vc].collectionTimeSec[day] >= MILLIS_IN_DAY)
        {
          virtualCircuitList[vc].collectionTimeSec[day] =
            ((virtualCircuitList[vc].collectionTimeSec[day] % MILLIS_IN_DAY)
            / 1000);
        }
        else
          virtualCircuitList[vc].collectionTimeSec[day] = vc * SECS_IN_MINUTE;

        //Display the collection time
        seconds = virtualCircuitList[vc].collectionTimeSec[day];
        hours = seconds / SECS_IN_HOUR;
        seconds -= hours * SECS_IN_HOUR;
        minutes = seconds / SECS_IN_MINUTE;
        seconds -= minutes * SECS_IN_MINUTE;
        if (DISPLAY_COLLECTION_TIME)
          printf("VC %d %s collection time: %d:%02d:%02d\n",
                 vc, dayName[day], hours, minutes, seconds);
      }
    }

    //Done with the results
    databaseFreeResults(results);
  } while (0);
  return status;
}

//Compare the manual valve control and issue commands as needed
void compareManualOn(ZONE_T * previous, ZONE_T * new)
{
  int delta;
  time_t now;
  struct tm * timeStruct;
  int vcIndex;
  int zoneBit;
  int zoneIndex;

  //Walk the list of VCs
  for (vcIndex = 0; vcIndex < MAX_VC; vcIndex++)
  {
    delta = previous[vcIndex] ^ new[vcIndex];
    if (delta)
    {
      //Set the zones that need updating
      manualZones[vcIndex] = delta;

      //Issue the commands to manually control the zone valve
      COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                       virtualCircuitList[vcIndex].commandTimer,
                       PROGRAMMING_COMPLETED);
      COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                       virtualCircuitList[vcIndex].commandTimer,
                       CMD_ATI12);
      COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                       virtualCircuitList[vcIndex].commandTimer,
                       CMD_SELECT_ZONE);

      //Mark this VC as needing updating
      virtualCircuitList[vcIndex].programUpdated = virtualCircuitList[vcIndex].runtime + 1;

      //Determine which manual controls have changed
      for (zoneIndex = 0; zoneIndex < manualZones[vcIndex]; zoneIndex++)
      {
        zoneBit = 1 << zoneIndex;
        if (delta & zoneBit)
        {
          //The manual control for this zone (valve) has changed
          time(&now);
          timeStruct = localtime(&now);
          printf ("%d-%02d-%02d %s %d:%02d:%02d: %s zone %d turning %s\n",
                  1900 + timeStruct->tm_year, 1 + timeStruct->tm_mon, timeStruct->tm_mday,
                  dayName[timeStruct->tm_wday],
                  timeStruct->tm_hour, timeStruct->tm_min, timeStruct->tm_sec,
                  controllerNames[vcIndex], zoneIndex + 1,
                  (new[vcIndex] & zoneBit) ? "ON" : "OFF");

          //Update the manual control
          previous[vcIndex] &= ~zoneBit;
          previous[vcIndex] |= new[vcIndex] & zoneBit;
        }
      }
    }
  }
}

//Compare the milliseconds per inch values and issue commands if they are different
void compareMSecPerInch(uint32_t * previous, uint32_t * new)
{
  uint32_t delta;
  int entry;
  time_t now;
  struct tm * timeStruct;
  int vcIndex;
  int zoneIndex;

  //Walk the list of VCs
  for (vcIndex = 0; vcIndex < MAX_VC; vcIndex++)
  {
    for (zoneIndex = 0; zoneIndex < ZONE_NUMBER_MAX; zoneIndex++)
    {
      entry = (vcIndex * ZONE_NUMBER_MAX) + zoneIndex;
      delta = previous[entry] - new[entry];
      if (delta)
      {
        //Issue the conversion factor command
        setMSecPerInch[(vcIndex * ZONE_NUMBER_MAX) + zoneIndex] = true;

        //Issue the commands to manually control the zone valve
        COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                         virtualCircuitList[vcIndex].commandTimer,
                         CMD_SELECT_ZONE);
        COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                         virtualCircuitList[vcIndex].commandTimer,
                         CMD_ATI12);
        COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                         virtualCircuitList[vcIndex].commandTimer,
                         PROGRAMMING_COMPLETED);

        //Mark this VC as needing updating
        virtualCircuitList[vcIndex].programUpdated = virtualCircuitList[vcIndex].runtime + 1;

        //Something caused the milliseconds per inch to change for this zone
        time(&now);
        timeStruct = localtime(&now);
        printf ("%d-%02d-%02d %s %d:%02d:%02d: %s zone %d mSecPerInch changed from %d to %d\n",
                1900 + timeStruct->tm_year, 1 + timeStruct->tm_mon, timeStruct->tm_mday,
                dayName[timeStruct->tm_wday],
                timeStruct->tm_hour, timeStruct->tm_min, timeStruct->tm_sec,
                controllerNames[vcIndex], zoneIndex + 1,
                previous[entry], new[entry]);

        //Update the milliseconds per inch value
        previous[entry] = new[entry];
      }
    }
  }
}

//Compare the solenoid types and issue commands as needed
void compareSolenoidTypes(ZONE_T * previous, ZONE_T * new)
{
  ZONE_T delta;
  time_t now;
  struct tm * timeStruct;
  int vcIndex;
  int zoneBit;
  int zoneIndex;

  //Walk the list of VCs
  for (vcIndex = 0; vcIndex < MAX_VC; vcIndex++)
  {
    delta = previous[vcIndex] ^ new[vcIndex];
    if (delta)
    {
      //Set the zones that need updating
      configureSolenoids[vcIndex] = delta;

      //Issue the commands to change the solenoid type
      COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                       virtualCircuitList[vcIndex].commandTimer,
                       PROGRAMMING_COMPLETED);
      COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                       virtualCircuitList[vcIndex].commandTimer,
                       CMD_ATI12);
      COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                       virtualCircuitList[vcIndex].commandTimer,
                       CMD_SELECT_ZONE);

      //Mark this VC as needing updating
      virtualCircuitList[vcIndex].programUpdated = virtualCircuitList[vcIndex].runtime + 1;

      //Determine which solenoids have changed
      for (zoneIndex = 0; zoneIndex < controllerZones[vcIndex]; zoneIndex++)
      {
        zoneBit = 1 << zoneIndex;
        if (delta & zoneBit)
        {
          //The solenoid for this zone (valve) has changed
          time(&now);
          timeStruct = localtime(&now);
          printf ("%d-%02d-%02d %s %d:%02d:%02d: %s zone %d switched from %s solenoid to %s solenoid\n",
                  1900 + timeStruct->tm_year, 1 + timeStruct->tm_mon, timeStruct->tm_mday,
                  dayName[timeStruct->tm_wday],
                  timeStruct->tm_hour, timeStruct->tm_min, timeStruct->tm_sec,
                  controllerNames[vcIndex], zoneIndex + 1,
                  (previous[vcIndex] & zoneBit) ? "9V DC latching" : "24V AC",
                  (new[vcIndex] & zoneBit) ? "9V DC latching" : "24V AC");

          //Update the solenoid type
          previous[vcIndex] &= ~zoneBit;
          previous[vcIndex] |= new[vcIndex] & zoneBit;
        }
      }
    }
  }
}

//Compare the start times and issue commands as needed
void compareSprinklerStartTimes(int32_t * previous, int32_t * new)
{
  int dayIndex;
  int dayOfWeek;
  int hours;
  int minutes;
  time_t now;
  int seconds;
  struct tm * timeStruct;
  int vcIndex;

  //Walk the list of VCs
  for (vcIndex = 0; vcIndex < MAX_VC; vcIndex++)
  {
    //Walk the list of days
    for (dayOfWeek = 0; dayOfWeek < DAYS_IN_WEEK; dayOfWeek++)
    {
      //Determine if the start time has changed
      dayIndex = (vcIndex * DAYS_IN_WEEK) + dayOfWeek;
      if (previous[dayIndex] != new[dayIndex])
      {
        //The start time for this day has changed
        if (DEBUG_SPRINKLER_CHANGES)
        {
          time(&now);
          timeStruct = localtime(&now);
          seconds = new[dayIndex];
          hours = seconds / MILLIS_IN_HOUR;
          seconds -= hours * MILLIS_IN_HOUR;
          minutes = seconds / MILLIS_IN_MINUTE;
          seconds -= minutes * MILLIS_IN_MINUTE;
          seconds = seconds / MILLIS_IN_SECOND;
          printf ("%d-%02d-%02d %s %d:%02d:%02d: %s start time for %s is: %d:%02d:%02d\n",
                  1900 + timeStruct->tm_year, 1 + timeStruct->tm_mon, timeStruct->tm_mday,
                  dayName[timeStruct->tm_wday],
                  timeStruct->tm_hour, timeStruct->tm_min, timeStruct->tm_sec,
                  controllerNames[vcIndex], dayName[dayOfWeek],
                  hours, minutes, seconds);
        }
        if (LOG_SPRINKLER_CHANGES)
        {
          time(&now);
          timeStruct = localtime(&now);
          seconds = new[dayIndex];
          hours = seconds / MILLIS_IN_HOUR;
          seconds -= hours * MILLIS_IN_HOUR;
          minutes = seconds / MILLIS_IN_MINUTE;
          seconds -= minutes * MILLIS_IN_MINUTE;
          seconds = seconds / MILLIS_IN_SECOND;
          sprintf (logBuffer, "%d-%02d-%02d %s %d:%02d:%02d: %s start time for %s is: %d:%02d:%02d\n",
                  1900 + timeStruct->tm_year, 1 + timeStruct->tm_mon, timeStruct->tm_mday,
                  dayName[timeStruct->tm_wday],
                  timeStruct->tm_hour, timeStruct->tm_min, timeStruct->tm_sec,
                  controllerNames[vcIndex], dayName[dayOfWeek],
                  hours, minutes, seconds);
          logTimeStampAndData(vcIndex, logBuffer, strlen(logBuffer));
        }

        //Update the start time
        previous[dayIndex] = new[dayIndex];

        //Set the zones that need updating
        setStartTimes[vcIndex] |= 1 << dayOfWeek;

        //Issue the commands to change the solenoid type
        COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                         virtualCircuitList[vcIndex].commandTimer,
                         PROGRAMMING_COMPLETED);
        COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                         virtualCircuitList[vcIndex].commandTimer,
                         CMD_ATI12);
        COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                         virtualCircuitList[vcIndex].commandTimer,
                         CMD_SET_DAY_OF_WEEK);

        //Mark this VC as needing updating
        virtualCircuitList[vcIndex].programUpdated = virtualCircuitList[vcIndex].runtime + 1;
      }
    }
  }
}

//Compare the zone durations and issue commands as needed
void compareZoneDurations(int32_t * previous, int32_t * new)
{
  int dayOfWeek;
  int durationBase;
  int entry;
  int hours;
  int index;
  int minutes;
  time_t now;
  int seconds;
  struct tm * timeStruct;
  int vcIndex;
  int zoneIndex;

  //Walk the list of VCs
  for (vcIndex = 0; vcIndex < MAX_VC; vcIndex++)
  {
    durationBase = vcIndex * DAYS_IN_WEEK * ZONE_NUMBER_MAX;
    memset(&setDurations[durationBase], 0, DAYS_IN_WEEK * ZONE_NUMBER_MAX);

    //Walk the list of days
    for (dayOfWeek = 0; dayOfWeek < DAYS_IN_WEEK; dayOfWeek++)
    {
      for (zoneIndex = 0; zoneIndex < ZONE_NUMBER_MAX; zoneIndex++)
      {
        //Determine if the duration has changed
        index = (((vcIndex * DAYS_IN_WEEK) + dayOfWeek) * ZONE_NUMBER_MAX) + zoneIndex;
        if (previous[index] != new[index])
        {
          //The duration for this zone has changed
          if (DEBUG_SPRINKLER_CHANGES)
          {
            time(&now);
            timeStruct = localtime(&now);
            seconds = new[index];
            hours = seconds / MILLIS_IN_HOUR;
            seconds -= hours * MILLIS_IN_HOUR;
            minutes = seconds / MILLIS_IN_MINUTE;
            seconds -= minutes * MILLIS_IN_MINUTE;
            seconds = seconds / MILLIS_IN_SECOND;
            printf ("%d-%02d-%02d %s %d:%02d:%02d: %s zone %d duration for %s is: %d:%02d:%02d\n",
                    1900 + timeStruct->tm_year, 1 + timeStruct->tm_mon, timeStruct->tm_mday,
                    dayName[timeStruct->tm_wday],
                    timeStruct->tm_hour, timeStruct->tm_min, timeStruct->tm_sec,
                    controllerNames[vcIndex], zoneIndex + 1, dayName[dayOfWeek],
                    hours, minutes, seconds);
          }
          if (LOG_SPRINKLER_CHANGES)
          {
            time(&now);
            timeStruct = localtime(&now);
            seconds = new[index];
            hours = seconds / MILLIS_IN_HOUR;
            seconds -= hours * MILLIS_IN_HOUR;
            minutes = seconds / MILLIS_IN_MINUTE;
            seconds -= minutes * MILLIS_IN_MINUTE;
            seconds = seconds / MILLIS_IN_SECOND;
            sprintf (logBuffer, "%d-%02d-%02d %s %d:%02d:%02d: %s zone %d duration for %s is: %d:%02d:%02d\n",
                    1900 + timeStruct->tm_year, 1 + timeStruct->tm_mon, timeStruct->tm_mday,
                    dayName[timeStruct->tm_wday],
                    timeStruct->tm_hour, timeStruct->tm_min, timeStruct->tm_sec,
                    controllerNames[vcIndex], zoneIndex + 1, dayName[dayOfWeek],
                    hours, minutes, seconds);
            logTimeStampAndData(vcIndex, logBuffer, strlen(logBuffer));
          }

          //Update the start time
          previous[index] = new[index];

          //Set the zones that has the duration update
          entry = (vcIndex * DAYS_IN_WEEK * ZONE_NUMBER_MAX)
                + (dayOfWeek * ZONE_NUMBER_MAX)
                + zoneIndex;
          setDurations[entry] = true;

          //Issue the commands to change the solenoid type
          COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                           virtualCircuitList[vcIndex].commandTimer,
                           PROGRAMMING_COMPLETED);
          COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                           virtualCircuitList[vcIndex].commandTimer,
                           CMD_ATI12);
          COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                           virtualCircuitList[vcIndex].commandTimer,
                           CMD_SET_DAY_OF_WEEK_2);

          //Mark this VC as needing updating
          virtualCircuitList[vcIndex].programUpdated = virtualCircuitList[vcIndex].runtime + 1;
        }
      }
    }
  }
}

void collectWaterData(int vcIndex)
{
  //Complete the programming
  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                   virtualCircuitList[vcIndex].commandTimer,
                   PROGRAMMING_COMPLETED);
  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                   virtualCircuitList[vcIndex].commandTimer,
                   CMD_ATI12);

  //Get the water use
  COMMAND_SCHEDULE(virtualCircuitList[vcIndex].commandQueue,
                   virtualCircuitList[vcIndex].commandTimer,
                   CMD_GET_WATER_USE);
}

int main(int argc, char **argv)
{
  bool breakLinks;
  ssize_t bytesRead;
  int cmd;
  fd_set currentfds;
  bool debug;
  bool displayTitle;
  int fdPipe;
  int maxfds;
  static char notification[256];
  int numfds;
  bool reset;
  int seconds;
  int status;
  char * terminal;
  struct timeval timeout;
  int vcIndex;

  maxfds = STDIN;
  status = 0;
  do
  {
    //Verify the command table length
    if (ARRAY_SIZE(commandName) != CMD_LIST_SIZE)
    {
      fprintf(stderr, "ERROR: Fix commandName length of %d != %d\n", (uint32_t)ARRAY_SIZE(commandName), CMD_LIST_SIZE);
      return -1;
    }

    //Display the help text if necessary
    if (argc < 6)
    {
      printf("%s   terminal   username   password   database   target_VC   [options]\n", argv[0]);
      printf("\n");
      printf("terminal - Name or path to the terminal device for the radio\n");
      printf("target_VC:\n");
      printf("    Client: 1 - %d\n", MAX_VC - 1);
      printf("    Loopback: my_VC\n");
      printf("    Broadcast: %d\n", VC_BROADCAST);
      printf("    Command: %d\n", VC_COMMAND);
      printf("Options:\n");
      printf("    --reset    Reset the LoRaSerial radio and break the links\n");
      printf("    --break    Use ATB command to break the links\n");
      status = -1;
      break;
    }

    //Get the path to the terminal
    terminal = argv[1];

    //Determine the remote VC address
    if ((sscanf(argv[5], "%d", &remoteVc) != 1)
      || ((remoteVc > PC_LINK_STATUS) && (remoteVc < VC_COMMAND)))
    {
      fprintf(stderr, "ERROR: Invalid target VC address, please use one of the following:\n");
      if (myVc)
        fprintf(stderr, "    Server: 0\n");
      fprintf(stderr, "    Client: 1 - %d\n", MAX_VC - 1);
      fprintf(stderr, "    Loopback: my_VC\n");
      fprintf(stderr, "    Broadcast: %d\n", VC_BROADCAST);
      fprintf(stderr, "    Command: %d\n", VC_COMMAND);
      status = -1;
      break;
    }

    //Get the database access
    username = argv[2];
    password = argv[3];
    database = argv[4];

    //Open the terminal
    status = openLoRaSerial(terminal);
    if (status)
      break;

    //Creating the named pipe (file)
    mkfifo(namedPipe, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    //Open the named pipe (file)
    fdPipe = open(namedPipe, O_RDONLY | O_NONBLOCK);
    if (fdPipe < 0)
    {
        status = errno;
        perror("ERROR: Failed to open named pipe");
        break;
    }

    //Determine the options
    reset = false;
    if ((argc == 7) && (strcmp("--reset", argv[7]) == 0))
      reset = true;

    breakLinks = reset;
    if ((argc == 7) && (strcmp("--break", argv[7]) == 0))
      breakLinks = true;

    //Reset the LoRaSerial radio if requested
    if (reset)
    {
      //Delay a while to let the radio complete its reset operation
      sleep(2);

      //Break the links to this node
      cmdToRadio((uint8_t *)LINK_RESET_COMMAND, strlen(LINK_RESET_COMMAND));

      //Allow the device to reset
      close(radio);
      do
      {
        sleep(1);

        //Open the terminal
        status = openLoRaSerial(terminal);
      } while (status);

      //Delay a while to let the radio complete its reset operation
      sleep(2);
    }

    //Initialize the virtual circuits
    for (vcIndex = 0; vcIndex < MAX_VC; vcIndex++)
      virtualCircuitList[vcIndex].activeCommand = CMD_LIST_SIZE;

    //Perform the initialization commands
    pcCommandTimer = 1;
    COMMAND_SCHEDULE(pcCommandQueue, pcCommandTimer, CMD_ATA);   //Get all the VC states
    COMMAND_SCHEDULE(pcCommandQueue, pcCommandTimer, CMD_ATI8);  //Get Radio unique ID
    COMMAND_SCHEDULE(pcCommandQueue, pcCommandTimer, CMD_ATI);   //Get Radio type
    COMMAND_SCHEDULE(pcCommandQueue, pcCommandTimer, CMD_GET_SERVER_MODEL);
    COMMAND_SCHEDULE(pcCommandQueue, pcCommandTimer, CMD_ATI30); //Get myVC

    //Break the links if requested
    if (breakLinks)
      COMMAND_SCHEDULE(pcCommandQueue, pcCommandTimer, CMD_ATB); //Break all the VC links

    //Initialize the weather station files
    status = initWeatherStation();
    if (status)
      break;

    //Initialize the MySql library
    debug = DATABASE_DEBUG;
    mysql = databaseInitialization(debug);
    if (!mysql)
    {
      status = -1;
      break;
    }

    //Create a connection to the database server
    host = "localhost";
    if (databaseConnect(mysql, debug))
    {
      status = -1;
      break;
    }

    //Get the controller names
    status = getControllerNames(mysql, controllerNames, controllerIds, controllerZones, debug);
    if (status)
      break;

    //Get the controller configurations
    status = getZoneConfiguration(mysql, latchingSolenoid, manualOn, mSecPerInch, debug);
    if (status)
      break;

    //Get the sprinkler controller schedules
    status = getSprinklerControllerStartTimes(mysql, startTimeArray, debug);
    if (status)
      break;

    //Get the zone durations
    status = getZoneDurations(mysql, durationArray, debug);
    if (status)
      break;

    //Initialize the fd_sets
    if (maxfds < radio)
      maxfds = radio;
    FD_ZERO(&readfds);
//    FD_SET(STDIN, &readfds);
    FD_SET(radio, &readfds);

    printf("Waiting for VC data...\n");
    while (1)
    {
      bool timeoutDetected;

      //Set the timeout
      timeout.tv_sec = 0;
      timeout.tv_usec = POLL_TIMEOUT_USEC;

      //Wait for receive data or timeout
      memcpy((void *)&currentfds, (void *)&readfds, sizeof(readfds));
      numfds = select(maxfds + 1, &currentfds, NULL, NULL, &timeout);
      if (numfds < 0)
      {
        status = errno;
        perror("ERROR: select call failed!");
        strcpy(logBuffer, "ERROR: select call failed!\n");
        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
        break;
      }

      //Check for timeout
      timeoutDetected = (numfds == 0);

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
        //Process the incoming data from the radio
        status = radioToHost();
        if (status)
          break;
      }

      //Read and discard the notification from the web-server
      bytesRead = read(fdPipe, notification, sizeof(notification));
      if (bytesRead < 0)
          break;
      if (bytesRead > 0)
      {
        //Get the latest sprinkler controller configurations
        status = getControllerNames(mysql, controllerNames, controllerIds, controllerZones, debug);
        if (status)
          break;
        status = getZoneConfiguration(mysql, tempLatching, tempManualOn, tempMSecPerInch, debug);
        if (status)
          break;
        status = getSprinklerControllerStartTimes(mysql, tempStartTimes, debug);
        if (status)
          break;
        status = getZoneDurations(mysql, tempZoneDuration, debug);
        if (status)
          break;

        //Compare the sprinkler controller configurations
        compareSolenoidTypes(latchingSolenoid, tempLatching);
        compareManualOn(manualOn, tempManualOn);
        compareMSecPerInch(mSecPerInch, tempMSecPerInch);
        compareSprinklerStartTimes(startTimeArray, tempStartTimes);
        compareZoneDurations(durationArray, tempZoneDuration);
      }

      //----------------------------------------
      //Update the files every second
      //----------------------------------------

      now = time(NULL);
      timeCurrent = localtime(&now);
      status = updateWeatherStation();
      if (status)
        break;

      //----------------------------------------
      // Collect the water after watering and at 5 AM
      //----------------------------------------

      seconds = timeCurrent->tm_sec + (timeCurrent->tm_min * SECS_IN_MINUTE)
              + (timeCurrent->tm_hour * SECS_IN_HOUR);
      for (vcIndex = 1; vcIndex < MAX_VC; vcIndex++)
      {
        if (virtualCircuitList[vcIndex].valid)
        {
          //Get water use after sprinklers finish watering
          if (virtualCircuitList[vcIndex].collectData[timeCurrent->tm_wday]
            && (virtualCircuitList[vcIndex].collectionTimeSec[timeCurrent->tm_wday] <= seconds))
          {
            virtualCircuitList[vcIndex].collectData[timeCurrent->tm_wday] = false;
            collectWaterData(vcIndex);
          }

          //Get water use at 5 AM
          if (virtualCircuitList[vcIndex].collectData5Am[timeCurrent->tm_wday]
            && (timeCurrent->tm_hour >= 5))
          {
            virtualCircuitList[vcIndex].collectData5Am[timeCurrent->tm_wday] = false;
            collectWaterData(vcIndex);
          }
        }
      }

      //----------------------------------------
      // Check for timeout
      //----------------------------------------

      if (timeoutDetected)
      {
        timeoutCount++;

        //Check for time to process PC commands
        if (pcCommandTimer && (pcActiveCommand >= CMD_LIST_SIZE)
          && ((timeoutCount - pcCommandTimer) >= COMMAND_POLL_COUNT))
        {
          pcCommandTimer = timeoutCount;
          if (!pcCommandTimer)
            pcCommandTimer = 1;
          issuePcCommands();
        }

        //Check for time to process VC commands
        for (vcIndex = 0; vcIndex < MAX_VC; vcIndex++)
        {
          if (virtualCircuitList[vcIndex].commandTimer
            && ((timeoutCount - virtualCircuitList[vcIndex].commandTimer) >= COMMAND_POLL_COUNT))
          {
            virtualCircuitList[vcIndex].commandTimer = timeoutCount;
            if (!virtualCircuitList[vcIndex].commandTimer)
              virtualCircuitList[vcIndex].commandTimer = 1;
            if (virtualCircuitList[vcIndex].activeCommand < CMD_LIST_SIZE)
              break;
            if (issueVcCommands(vcIndex))
            {
              if (ISSUE_COMMANDS_IN_PARALLEL)
                continue;
              else
                break;
            }
          }
        }

        //----------------------------------------
        // Check the resource usage
        //----------------------------------------

        if (DISPLAY_RESOURCE_USAGE && (!(timeoutCount % (ONE_SECOND_COUNT * 60))))
        {
          struct rusage usage;
          int s_days;
          int s_hours;
          int s_mins;
          int s_secs;
          uint64_t s_millis;
          int u_days;
          int u_hours;
          int u_mins;
          int u_secs;
          uint64_t u_millis;

          //Get the sprinkler server resource useage
          getrusage(RUSAGE_SELF, &usage);
          u_millis = usage.ru_utime.tv_usec / 1000;
          u_secs = usage.ru_utime.tv_sec;
          u_days = u_secs / SECS_IN_DAY;
          u_secs -= u_days * SECS_IN_DAY;
          u_hours = u_secs / SECS_IN_HOUR;
          u_secs -= u_hours * SECS_IN_HOUR;
          u_mins = u_secs / SECS_IN_MINUTE;
          u_secs -= u_mins * SECS_IN_MINUTE;

          s_millis = usage.ru_stime.tv_usec / 1000;
          s_secs = usage.ru_stime.tv_sec;
          s_days = s_secs / SECS_IN_DAY;
          s_secs -= s_days * SECS_IN_DAY;
          s_hours = s_secs / SECS_IN_HOUR;
          s_secs -= s_hours * SECS_IN_HOUR;
          s_mins = s_secs / SECS_IN_MINUTE;
          s_secs -= s_mins * SECS_IN_MINUTE;

          printf("K: %d:%02d:%02d.%03d, U: %d:%02d:%02d.%03d, Mem: %ld, PF: %ld: IN: %ld, OUT: %ld\n",
                  u_hours, u_mins, u_secs, (int)u_millis,
                  s_hours, s_mins, s_secs, (int)s_millis,
                  usage.ru_maxrss, usage.ru_majflt, usage.ru_inblock, usage.ru_oublock);

/*
          getrusage(RUSAGE_CHILDREN, &usage);
          u_millis = usage.ru_utime.tv_usec / 1000;
          u_secs = usage.ru_utime.tv_sec;
          u_days = u_secs / SECS_IN_DAY;
          u_secs -= u_days * SECS_IN_DAY;
          u_hours = u_secs / SECS_IN_HOUR;
          u_secs -= u_hours * SECS_IN_HOUR;
          u_mins = u_secs / SECS_IN_MINUTE;
          u_secs -= u_mins * SECS_IN_MINUTE;

          s_millis = usage.ru_stime.tv_usec / 1000;
          s_secs = usage.ru_stime.tv_sec;
          s_days = s_secs / SECS_IN_DAY;
          s_secs -= s_days * SECS_IN_DAY;
          s_hours = s_secs / SECS_IN_HOUR;
          s_secs -= s_hours * SECS_IN_HOUR;
          s_mins = s_secs / SECS_IN_MINUTE;
          s_secs -= s_mins * SECS_IN_MINUTE;

          printf("K: %d:%02d:%02d.%03d, U: %d:%02d:%02d.%03d, Mem: %ld, PF: %ld: IN: %ld, OUT: %ld\n",
                  u_hours, u_mins, u_secs, (int)u_millis,
                  s_hours, s_mins, s_secs, (int)s_millis,
                  usage.ru_maxrss, usage.ru_majflt, usage.ru_inblock, usage.ru_oublock);
*/
        }

        //----------------------------------------
        // Check for stalled commands
        //----------------------------------------

        //Determine if the command processor is running
        if (commandProcessorRunning)
        {
          //Determine if it is time to check for a command processor stall
          if (commandProcessorRunning)
            commandProcessorRunning--;
          if (!commandProcessorRunning)
          {
            //The command processor is either stalled or complete
            //Determine if there are any outsanding commands to be processed
            displayTitle = true;
            for (int cmdBlock = 0; cmdBlock < COMMAND_QUEUE_SIZE; cmdBlock++)
            {
              if (pcCommandQueue[cmdBlock])
              {
                //Display the next outstanding command
                int cmdBase = cmdBlock * QUEUE_T_BITS;
                for (cmd = cmdBase; cmd < (cmdBase + QUEUE_T_BITS); cmd++)
                {
                  if (COMMAND_PENDING(pcCommandQueue, cmd))
                  {
                    if (displayTitle)
                    {
                      displayTitle = false;
                      printf("Stalled commands:\n");
                      sprintf(logBuffer, "Stalled commands:\n");
                      logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
                    }
                    printf("    PC: %d (%s, %s)\n",
                           cmd,
                           commandName[cmd],
                           (pcActiveCommand < CMD_LIST_SIZE) ? "Active" : "Pending");
                    sprintf(logBuffer, "    PC: %d (%s, %s)\n",
                           cmd,
                           commandName[cmd],
                           (pcActiveCommand < CMD_LIST_SIZE) ? "Active" : "Pending");
                    logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
                    break;
                  }
                }

                //Check for a stalled command
                if (cmd < (cmdBase + QUEUE_T_BITS))
                  break;
              }
            }

            //Determine if the local radio command queue is idle
            if (displayTitle)
            {
              printf("PC: Idle\n");
              sprintf(logBuffer, "PC: Idle\n");
              logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
            }

            //Determine if there are any outstanding VC commands
            for (vcIndex = 0; vcIndex < MAX_VC; vcIndex++)
            {
              for (int cmdBlock = 0; cmdBlock < COMMAND_QUEUE_SIZE; cmdBlock++)
              {
                if (virtualCircuitList[vcIndex].commandQueue[cmdBlock])
                {
                  //Display the next outstanding command
                  int cmdBase = cmdBlock * QUEUE_T_BITS;
                  for (cmd = cmdBase; cmd < (cmdBase + QUEUE_T_BITS); cmd++)
                  {
                    if (COMMAND_PENDING(virtualCircuitList[vcIndex].commandQueue, cmd))
                    {
                      if (displayTitle)
                      {
                        displayTitle = false;
                        printf("Stalled commands:\n");
                      }
                      printf("    VC %d (%s): %d (%s, %s)\n",
                             vcIndex,
                             vcStateNames[virtualCircuitList[vcIndex].vcState],
                             cmd,
                             commandName[cmd],
                             (virtualCircuitList[vcIndex].activeCommand < CMD_LIST_SIZE) ? "Active" : "Pending");
                      if (LOG_CMD_SCHEDULE)
                      {
                        sprintf(logBuffer, "Stalled commands:\n");
                        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
                        logTimeStampAndData(vcIndex, logBuffer, strlen(logBuffer));
                        sprintf(logBuffer, "    VC %d (%s): %d (%s, %s)\n",
                               vcIndex,
                               vcStateNames[virtualCircuitList[vcIndex].vcState],
                               cmd,
                               commandName[cmd],
                               (virtualCircuitList[vcIndex].activeCommand < CMD_LIST_SIZE) ? "Active" : "Pending");
                        logTimeStampAndData(VC_PC, logBuffer, strlen(logBuffer));
                        logTimeStampAndData(vcIndex, logBuffer, strlen(logBuffer));
                      }
                      break;
                    }
                  }

                  //Check for a stalled command
                  if (cmd < (cmdBase + QUEUE_T_BITS))
                    break;
                }
              }
            }
          }
        }
      }
    }
  } while (0);

  //Close the weather station files
  closeWeatherStation();

  //Done with the radio
  close(radio);
  return status;
}
