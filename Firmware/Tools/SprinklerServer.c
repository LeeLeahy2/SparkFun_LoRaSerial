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
#include <sys/resource.h>
#include <sys/time.h>
#include "WeatherStation.h"

#ifndef POLL_TIMEOUT_USEC
#define POLL_TIMEOUT_USEC       1000
#endif  // POLL_TIMEOUT_USEC

#define ONE_SECOND_COUNT        20 // (1000000 / POLL_TIMEOUT_USEC)
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
#define LINK_RESET_COMMAND      "atz"
#define MY_VC_ADDRESS           "myVc: "
#define SET_COMMAND_DAY         "at-CommandDay="
#define SET_COMMAND_ZONE        "at-CommandZone="
#define SET_MANUAL_ON           "at-ZoneManualOn="
#define SET_PROGRAM_COMPLETE    "ati12"
#define SET_SOLENOID_TYPE       "at-LatchingSolenoid="
#define SET_START_TIME          "at-StartTime="
#define SET_ZONE_DURATION       "at-ZoneDuration="
#define START_3_WAY_HANDSHAKE   "atc"

#define DEBUG_DATABASE_UPDATES    0
#define DEBUG_LOCAL_COMMANDS      0
#define DEBUG_PC_CMD_ISSUE        0
#define DEBUG_PC_TO_RADIO         0
#define DEBUG_RADIO_TO_PC         0
#define DEBUG_SPRINKLER_CHANGES   1
#define DISPLAY_COMMAND_COMPLETE  0
#define DISPLAY_DATA_ACK          0
#define DISPLAY_DATA_NACK         1
#define DISPLAY_RESOURCE_USAGE    0
#define DISPLAY_RUNTIME           0
#define DISPLAY_STATE_TRANSITION  0
#define DISPLAY_UNKNOWN_COMMANDS  0
#define DISPLAY_VC_STATE          0
#define DUMP_RADIO_TO_PC          0

#define DATABASE_DEBUG        0

#define DAYS_IN_WEEK          7
#define START_TIME_ENTRIES    (MAX_VC * DAYS_IN_WEEK)
#define DURATION_ENTRIES      (START_TIME_ENTRIES * ZONE_NUMBER_MAX)

#define QUEUE_T                   uint32_t
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

#define COMMAND_COMPLETE(queue, active)                               \
{                                                                     \
  if (COMMAND_PENDING(queue, active))                                 \
  {                                                                   \
    queue[active / QUEUE_T_BITS] &= ~(1 << (active & QUEUE_T_MASK));  \
    active = CMD_LIST_SIZE;                                           \
  }                                                                   \
}

#define COMMAND_ISSUE(queue, pollCount, cmd)              \
{                                                         \
  /* Place the command in the queue */                    \
  queue[cmd / QUEUE_T_BITS] |= 1 << (cmd & QUEUE_T_MASK); \
                                                          \
  /* Timeout the command processor */                     \
  if (!commandProcessorRunning)                           \
    commandProcessorRunning = STALL_CHECK_COUNT;          \
                                                          \
  /* Remember when this command was issued */             \
  if (!pollCount)                                         \
  {                                                       \
    if (timeoutCount)                                     \
      pollCount = timeoutCount;                           \
    else                                                  \
      pollCount = 1;                                      \
  }                                                       \
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

  //Select the solenoids and manual control for each of the zone
  CMD_SELECT_ZONE,            //Select the zone number
  CMD_SELECT_SOLENOID,        //Select the solenoid for the zone
  CMD_SET_MANUAL_ON,          //Select the manual state of the zone valve
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
  "AT-CommandZone", "AT-LatchingSolenoid", "AT-ManualOn", "COMPLETE_ZONE_CONFIGURATION",
  "AT-CommandDay", "AT-StartTime", "CMD_SET_ALL_START_TIMES",
  "AT-CommandDay-2", "AT-CommandZone-2", "AT-ZoneDuration", "CMD_SET_ALL_DURATIONS",

/*
#define SET_MANUAL_ON           "AT-ZoneManualOn="
#define SET_ZONE_DURATION       "AT-ZoneDuration="
*/

  "AT-EnableController=1",
  "ATI12", "PROGRAMMING_COMPLETED",
};

typedef struct _VIRTUAL_CIRCUIT
{
  int vcState;
  uint32_t activeCommand;
  QUEUE_T commandQueue[COMMAND_QUEUE_SIZE];
  uint32_t commandTimer;
  uint64_t programmed;
  uint64_t programUpdated;
  uint64_t runtime;
  uint8_t uniqueId[UNIQUE_ID_BYTES];
  bool valid;
} VIRTUAL_CIRCUIT;

uint32_t commandProcessorRunning;
bool commandStatus;
ZONE_T configureZones[MAX_VC];
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
uint8_t inputBuffer[INPUT_BUFFER_SIZE];
ZONE_T latchingSolenoid[MAX_VC];
int manualControl[MAX_VC];
ZONE_T manualOn[MAX_VC];
ZONE_T manualZones[MAX_VC];
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
int setStartTimes[START_TIME_ENTRIES];
int solenoidType[MAX_VC];
int32_t startTimeArray[START_TIME_ENTRIES];
ZONE_T tempLatching[MAX_VC];
ZONE_T tempManualOn[MAX_VC];
int32_t tempStartTimes[START_TIME_ENTRIES];
int32_t tempZoneDuration[DURATION_ENTRIES];
uint32_t timeoutCount;
const char * username;
char vcCommandBuffer[MAX_VC][128];
VIRTUAL_CIRCUIT virtualCircuitList[MAX_VC];
volatile bool waitingForCommandComplete;
int zoneNumber[MAX_VC];

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
      printf("VC %d unique ID: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
             srcVc,
             vcMsg->uniqueId[0], vcMsg->uniqueId[1], vcMsg->uniqueId[2], vcMsg->uniqueId[3],
             vcMsg->uniqueId[4], vcMsg->uniqueId[5], vcMsg->uniqueId[6], vcMsg->uniqueId[7],
             vcMsg->uniqueId[8], vcMsg->uniqueId[9], vcMsg->uniqueId[10], vcMsg->uniqueId[11],
             vcMsg->uniqueId[12], vcMsg->uniqueId[13], vcMsg->uniqueId[14], vcMsg->uniqueId[15]);

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
      printf("VC %d unique ID: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
             srcVc,
             vcMsg->uniqueId[0], vcMsg->uniqueId[1], vcMsg->uniqueId[2], vcMsg->uniqueId[3],
             vcMsg->uniqueId[4], vcMsg->uniqueId[5], vcMsg->uniqueId[6], vcMsg->uniqueId[7],
             vcMsg->uniqueId[8], vcMsg->uniqueId[9], vcMsg->uniqueId[10], vcMsg->uniqueId[11],
             vcMsg->uniqueId[12], vcMsg->uniqueId[13], vcMsg->uniqueId[14], vcMsg->uniqueId[15]);

      //Update the database
      databaseUpdateVc(mysql, srcVc, vcMsg->uniqueId, 1);
    }
  }

  //Display the state if requested
  if (DISPLAY_STATE_TRANSITION)
    printf("VC%d: %s --> %s\n", srcVc, vcStateNames[previousState], vcStateNames[newState]);
  switch (newState)
  {
  default:
    if (DEBUG_PC_CMD_ISSUE)
      printf("VC %d unknown state!\n", srcVc);
    if (DISPLAY_VC_STATE)
      printf("------- VC %d State %3d ------\n", srcVc, vcMsg->vcState);
    break;

  case VC_STATE_LINK_DOWN:
    if (DEBUG_PC_CMD_ISSUE)
      printf("VC %d DOWN\n", srcVc);
    if (DISPLAY_VC_STATE)
      printf("--------- VC %d DOWN ---------\n", srcVc);
    break;

  case VC_STATE_LINK_ALIVE:
    //Upon transition to ALIVE, if is the server or the source VC matches the
    //target VC or myVc, bring up the connection
    if ((previousState != newState) && (myVc == VC_SERVER))
    {
      if (DEBUG_PC_CMD_ISSUE)
        printf("VC %d ALIVE\n", srcVc);
      COMMAND_ISSUE(virtualCircuitList[srcVc].commandQueue,
                    virtualCircuitList[srcVc].commandTimer,
                    CMD_AT_CMDVC);
      COMMAND_ISSUE(virtualCircuitList[srcVc].commandQueue,
                    virtualCircuitList[srcVc].commandTimer,
                    CMD_ATC);
      COMMAND_ISSUE(virtualCircuitList[srcVc].commandQueue,
                    virtualCircuitList[srcVc].commandTimer,
                    CMD_WAIT_CONNECTED);
    }

    if (DISPLAY_VC_STATE)
      printf("-=--=--=- VC %d ALIVE =--=--=-\n", srcVc);
    break;

  case VC_STATE_SEND_UNKNOWN_ACKS:
    if (DEBUG_PC_CMD_ISSUE)
      printf("VC %d SEND_UNKNOWN_ACKS\n", srcVc);
    if (DISPLAY_VC_STATE)
      printf("-=--=-- VC %d ALIVE UA --=--=-\n", srcVc);
    break;

  case VC_STATE_WAIT_SYNC_ACKS:
    if (DEBUG_PC_CMD_ISSUE)
      printf("VC %d WAIT_SYNC_ACKS\n", srcVc);
    if (DISPLAY_VC_STATE)
      printf("-=--=-- VC %d ALIVE SA --=--=-\n", srcVc);
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
    break;

  case VC_STATE_CONNECTED:
    if (DEBUG_PC_CMD_ISSUE)
      printf("VC %d CONNECTED\n", srcVc);
    if (COMMAND_PENDING(pcCommandQueue, CMD_ATC)
      && (pcActiveCommand == CMD_ATC) && (srcVc == pcCommandVc))
    {
      if (virtualCircuitList[srcVc].activeCommand == CMD_ATC)
        COMMAND_COMPLETE(virtualCircuitList[srcVc].commandQueue, virtualCircuitList[srcVc].activeCommand);
      COMMAND_COMPLETE(pcCommandQueue, pcActiveCommand);
    }
    if (DISPLAY_VC_STATE)
      printf("======= VC %d CONNECTED ======\n", srcVc);
    if (srcVc != myVc)
    {
      COMMAND_ISSUE(virtualCircuitList[srcVc].commandQueue,
                    virtualCircuitList[srcVc].commandTimer,
                    CMD_AT_CMDVC_2);
      COMMAND_ISSUE(virtualCircuitList[srcVc].commandQueue,
                    virtualCircuitList[srcVc].commandTimer,
                    CMD_ATI31);
      COMMAND_ISSUE(virtualCircuitList[srcVc].commandQueue,
                    virtualCircuitList[srcVc].commandTimer,
                    CMD_GET_CLIENT_MODEL);
      COMMAND_ISSUE(virtualCircuitList[srcVc].commandQueue,
                    virtualCircuitList[srcVc].commandTimer,
                    CMD_ATI_2);
      COMMAND_ISSUE(virtualCircuitList[srcVc].commandQueue,
                    virtualCircuitList[srcVc].commandTimer,
                    CMD_ATI8_2);
      COMMAND_ISSUE(virtualCircuitList[srcVc].commandQueue,
                    virtualCircuitList[srcVc].commandTimer,
                    CMD_ATI11);
      COMMAND_ISSUE(virtualCircuitList[srcVc].commandQueue,
                    virtualCircuitList[srcVc].commandTimer,
                    CHECK_FOR_UPDATE);
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

  //Set the time values
  memcpy(&virtualCircuitList[vcIndex].runtime, &vcMsg->runtime, sizeof(vcMsg->runtime));
  memcpy(&virtualCircuitList[vcIndex].programmed, &vcMsg->programmed, sizeof(vcMsg->programmed));
}

void radioCommandComplete(VC_SERIAL_MESSAGE_HEADER * header, uint8_t * data, uint8_t length)
{
  VC_COMMAND_COMPLETE_MESSAGE * vcMsg;
  uint8_t srcVc;

  //The command processor is still running
  commandProcessorRunning = STALL_CHECK_COUNT;

  //Done with this command
  srcVc = header->radio.srcVc;
  if (srcVc == myVc)
  {
    if (pcActiveCommand < CMD_LIST_SIZE)
    {
      if (pcCommandVc < MAX_VC)
      {
        COMMAND_COMPLETE(virtualCircuitList[pcCommandVc].commandQueue,
                         virtualCircuitList[pcCommandVc].activeCommand);
      }
      COMMAND_COMPLETE(pcCommandQueue, pcActiveCommand);
    }
    else if (virtualCircuitList[pcCommandVc].activeCommand < CMD_LIST_SIZE)
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

    //------------------------------
    //Process the message
    //------------------------------

    //Display radio runtime
    if (header->radio.destVc == PC_RUNTIME)
      radioRuntime(header, dataStart, length);

    //Dump the unknown VC message
    else
      dumpBuffer((uint8_t*)header, length + VC_SERIAL_HEADER_BYTES);
  } while (data < dataEnd);
  return status;
}

int radioToHost()
{
  int bytesRead;
  int bytesSent;
  int bytesToRead;
  int bytesToSend;
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
      //Output the debug data
      hostToStdout(NULL, dataStart, length);

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

    //------------------------------
    //Process the message
    //------------------------------

    //Display link status
    if (header->radio.destVc == PC_LINK_STATUS)
      radioToPcLinkStatus(header, data, VC_SERIAL_HEADER_BYTES + length);

    else if (header->radio.destVc == PC_RAIN_STATUS)
      rainStatus(header, VC_SERIAL_HEADER_BYTES + length);

    else if (header->radio.destVc == PC_WIND_STATUS)
      windStatus(header, VC_SERIAL_HEADER_BYTES + length);

    //Display remote command response
    else if (header->radio.destVc == (PC_REMOTE_RESPONSE | myVc))
      status = commandResponse(data, length);

    //Display command completion status
    else if (header->radio.destVc == PC_COMMAND_COMPLETE)
      radioCommandComplete(header, data, length);

    //Display ACKs for transmitted messages
    else if (header->radio.destVc == PC_DATA_ACK)
      radioDataAck(header, data, length);

    //Display NACKs for transmitted messages
    else if (header->radio.destVc == PC_DATA_NACK)
      radioDataNack(header, data, length);

    //Display received messages
    else if ((header->radio.destVc == myVc) || (header->radio.destVc == VC_BROADCAST))
    {
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
                  findMyVc = true;
                  cmdToRadio((uint8_t *)GET_MY_VC_ADDRESS, strlen(GET_MY_VC_ADDRESS));
                  return;
                }
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Skipping ATI30 command, myVC already known\n");
                COMMAND_COMPLETE(pcCommandQueue, pcActiveCommand);
                break;

              case CMD_ATB: //Break all of the VC links
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Issuing ATB command\n");
                cmdToRadio((uint8_t *)BREAK_LINKS_COMMAND, strlen(BREAK_LINKS_COMMAND));
                return;

              case CMD_GET_SERVER_MODEL:
                //Get the server model name
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Issuing %s command\n", GET_SERVER_MODEL);
                cmdToRadio((uint8_t *)GET_SERVER_MODEL, strlen(GET_SERVER_MODEL));
                return;

              case CMD_ATI:
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Issuing ATI command\n");
                cmdToRadio((uint8_t *)GET_DEVICE_INFO, strlen(GET_DEVICE_INFO));
                return;

              case CMD_ATI8:
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Issuing ATI8 command\n");
                cmdToRadio((uint8_t *)GET_UNIQUE_ID, strlen(GET_UNIQUE_ID));
                return;

              case CMD_ATA: //Get all the VC states
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Issuing ATA command\n");
                cmdToRadio((uint8_t *)GET_VC_STATUS, strlen(GET_VC_STATUS));
                return;

              case CMD_AT_CMDVC: //Select the VC to target
              case CMD_AT_CMDVC_2:
                //Select the VC to use
                sprintf(pcCommandBuffer, "at-CmdVc=%d", pcCommandVc);
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Issuing %s command\n", pcCommandBuffer);
                cmdToRadio((uint8_t *)pcCommandBuffer, strlen(pcCommandBuffer));
                return;

              case CMD_ATC: //Perform the 3-way handshake
                //Bring up the VC connection to this remote system
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Issuing ATC command\n");
                cmdToRadio((uint8_t *)START_3_WAY_HANDSHAKE, strlen(START_3_WAY_HANDSHAKE));
                return;

              case CMD_ATI31:
                //Get the VC state
                if (DEBUG_PC_CMD_ISSUE)
                  printf("Issuing ATI31 command\n");
                cmdToRadio((uint8_t *)GET_VC_STATE, strlen(GET_VC_STATE));
                return;
            }
          }
        }
      }
    }
    pcActiveCommand = CMD_LIST_SIZE;
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
  int entry;
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
                  COMMAND_ISSUE(pcCommandQueue, pcCommandTimer, CMD_AT_CMDVC);
                  if (COMMAND_PENDING(virtualCircuitList[vcIndex].commandQueue, CMD_ATC))
                    COMMAND_ISSUE(pcCommandQueue, pcCommandTimer, CMD_ATC);
                  return true;
                }
                virtualCircuitList[vcIndex].activeCommand = CMD_LIST_SIZE;
                return true;

              case CMD_ATC:
                return true;

              case CMD_WAIT_CONNECTED:
                if ((virtualCircuitList[vcIndex].vcState != VC_STATE_CONNECTED)
                    || commandProcessorBusy(vcIndex))
                {
                  //Mark the list as empty to allow this entry to be executed again
                  virtualCircuitList[vcIndex].activeCommand = CMD_LIST_SIZE;
                  return true;
                }

                //The wait is complete, this VC is now connected to the sprinkler server
                COMMAND_COMPLETE(virtualCircuitList[vcIndex].commandQueue,
                                 virtualCircuitList[vcIndex].activeCommand);

                //Get the VC state
                COMMAND_ISSUE(pcCommandQueue, pcCommandTimer, CMD_AT_CMDVC_2);
                COMMAND_ISSUE(pcCommandQueue, pcCommandTimer, CMD_ATI31);
                break;

              case CMD_AT_CMDVC_2:
                //Determine if the local command processor is idle
                if (commandProcessorIdle(vcIndex))
                {
                  if (DEBUG_PC_CMD_ISSUE)
                    printf("Migrating AT-CMDVC_2 and ATI31 commands to PC command queue\n");
                  COMMAND_ISSUE(pcCommandQueue, pcCommandTimer, CMD_AT_CMDVC_2);
                  if (COMMAND_PENDING(virtualCircuitList[vcIndex].commandQueue, CMD_ATI31))
                    COMMAND_ISSUE(pcCommandQueue, pcCommandTimer, CMD_ATI31);
                  return true;
                }
                virtualCircuitList[vcIndex].activeCommand = CMD_LIST_SIZE;
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
                if ((!virtualCircuitList[vcIndex].programUpdated)
                  || (virtualCircuitList[vcIndex].programUpdated > virtualCircuitList[vcIndex].programmed))
                {
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                CMD_AT_DISABLE_CONTROLLER);
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                CMD_AT_DAY_OF_WEEK);
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                CMD_AT_TIME_OF_DAY);
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                CMD_ATI89);

                  //Configure the sprinkler controller to properly drive the zone solenoids
                  configureZones[vcIndex] = ZONE_MASK;
                  manualZones[vcIndex] = ZONE_MASK;
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                CMD_SELECT_ZONE);

                  //Set the start times for each day of the week
                  setStartTimes[vcIndex] = (1 << DAYS_IN_WEEK) - 1;
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                CMD_SET_DAY_OF_WEEK);

                  //Set the duration for all of the zones for each day of the week
                  durationBase = vcIndex * DAYS_IN_WEEK * ZONE_NUMBER_MAX;
                  memset(&setDurations[durationBase], 0xff, DAYS_IN_WEEK * ZONE_NUMBER_MAX);
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                CMD_SET_DAY_OF_WEEK_2);

                  //Make sure the sprinkler controller is enabled
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                CMD_AT_ENABLE_CONTROLLER);

                  //Complete the programming
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                CMD_ATI12);
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                PROGRAMMING_COMPLETED);
                }

                //Done with the CHECK_FOR_UPDATE command
                COMMAND_COMPLETE(virtualCircuitList[vcIndex].commandQueue,
                                 virtualCircuitList[vcIndex].activeCommand);
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

              //Select the zone
              case CMD_SELECT_ZONE:
                //Locate the next zone to configure
                for (zoneIndex = 0; zoneIndex < ZONE_NUMBER_MAX; zoneIndex++)
                {
                  //Determine if this zone needs configuration
                  zoneBit = 1 << zoneIndex;
                  if ((configureZones[vcIndex] | manualZones[vcIndex]) & zoneBit)
                  {
                    //Send the command to select the zone
                    sprintf(vcCommandBuffer[vcIndex], "%s%d", SET_COMMAND_ZONE, zoneIndex + 1);
                    sendVcCommand(vcCommandBuffer[vcIndex], vcIndex);

                    //Issue the command to select the solenoid type
                    if (configureZones[vcIndex] & zoneBit)
                    {
                      configureZones[vcIndex] &= ~zoneBit;
                      solenoidType[vcIndex] = (latchingSolenoid[vcIndex] >> zoneIndex) & 1;
                      COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                    virtualCircuitList[vcIndex].commandTimer,
                                    CMD_SELECT_SOLENOID);
                    }

                    //Issue the command to manually control the zone
                    if (manualZones[vcIndex] & zoneBit)
                    {
                      manualZones[vcIndex] &= ~zoneBit;
                      manualControl[vcIndex] = (manualOn[vcIndex] >> zoneIndex) & 1;
                      COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                    virtualCircuitList[vcIndex].commandTimer,
                                    CMD_SET_MANUAL_ON);
                    }

                    //Complete the sequence for this zone
                    COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                  virtualCircuitList[vcIndex].commandTimer,
                                  COMPLETE_ZONE_CONFIGURATION);
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

              //Determine if all of the zones are configured
              case COMPLETE_ZONE_CONFIGURATION:
                COMMAND_COMPLETE(virtualCircuitList[vcIndex].commandQueue,
                                 virtualCircuitList[vcIndex].activeCommand);
                if (configureZones[vcIndex] | manualZones[vcIndex])
                {
                  //Configure the next zone
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
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
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                CMD_SET_START_TIME);
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                CMD_SET_ALL_START_TIMES);
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
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
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
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
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
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                CMD_SET_ZONE_DURATION);
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                CMD_SET_ALL_DURATIONS);
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
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                CMD_SELECT_ZONE_2);
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                CMD_SET_ALL_DURATIONS);
                  return true;
                }
                if (dayNumber[vcIndex] < (DAYS_IN_WEEK- 1))
                {
                  //Configure the next zone
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                CMD_SET_DAY_OF_WEEK_2);
                  COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                                virtualCircuitList[vcIndex].commandTimer,
                                CMD_SET_ALL_DURATIONS);
                  return true;
                }
                break;

/*
#define SET_MANUAL_ON           "AT-ZoneManualOn="
#define SET_ZONE_DURATION       "AT-ZoneDuration="
*/

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
        virtualCircuitList[vcIndex].activeCommand = CMD_LIST_SIZE;
      }
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
      }
    }

    //Done with the results
    databaseFreeResults(results);
  } while (0);
  return status;
}

int getZoneConfiguration(MYSQL * mysql, ZONE_T * latching, ZONE_T * on, bool debug)
{
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
                           "SELECT VcNumber, ZoneNumber, Latching, ManualOn, SprinklerControllerName FROM view_sprinkler_valve WHERE (VcNumber IS NOT NULL) ORDER BY VcNumber, ZoneNumber",
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
          && (vc < MAX_VC)
          && (zone >= 1) && (zone <= ZONE_NUMBER_MAX)
          && (onOff <= 1))
      {
        index = zone - 1; //Zone numbers: 1 - max, bit map index starts at zero
        latching[vc] &= ~(1 << index);
        latching[vc] |= (type & 1) << index;
        on[vc] &= ~(1 << index);
        on[vc] |= (onOff & 1) << index;
        if (DEBUG_SPRINKLER_CHANGES)
          printf("VC: %s, Zone: %s, Latching: %d, On: %d\n", row[0], row[1], type, onOff);
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
        milliseconds = (hours * MILLIS_IN_HOUR)
                     + (minutes * MILLIS_IN_MINUTE)
                     + (seconds * MILLIS_IN_SECOND);
        startTimeArray[(vc * DAYS_IN_WEEK) + dayOfWeek] = milliseconds;
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
        milliseconds = (hours * MILLIS_IN_HOUR)
                     + (minutes * MILLIS_IN_MINUTE)
                     + (seconds * MILLIS_IN_SECOND);
        index = (((vc * DAYS_IN_WEEK) + dayOfWeek) * ZONE_NUMBER_MAX) + zone - 1;
        durationArray[index] = milliseconds;
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
printf("delta: 0x%02x\n", delta);
    if (delta)
    {
      //Set the zones that need updating
      manualZones[vcIndex] = delta;

      //Issue the commands to manually control the zone valve
      COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                    virtualCircuitList[vcIndex].commandTimer,
                    CMD_SELECT_ZONE);
      COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                    virtualCircuitList[vcIndex].commandTimer,
                    CMD_ATI12);
      COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                    virtualCircuitList[vcIndex].commandTimer,
                    PROGRAMMING_COMPLETED);

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
      configureZones[vcIndex] = delta;

      //Issue the commands to change the solenoid type
      COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                    virtualCircuitList[vcIndex].commandTimer,
                    CMD_SELECT_ZONE);
      COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                    virtualCircuitList[vcIndex].commandTimer,
                    CMD_ATI12);
      COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                    virtualCircuitList[vcIndex].commandTimer,
                    PROGRAMMING_COMPLETED);

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

        //Update the start time
        previous[dayIndex] = new[dayIndex];

        //Set the zones that need updating
        setStartTimes[vcIndex] |= 1 << dayOfWeek;

        //Issue the commands to change the solenoid type
        COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                      virtualCircuitList[vcIndex].commandTimer,
                      CMD_SET_DAY_OF_WEEK);
        COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                      virtualCircuitList[vcIndex].commandTimer,
                      CMD_ATI12);
        COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                      virtualCircuitList[vcIndex].commandTimer,
                      PROGRAMMING_COMPLETED);

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

          //Update the start time
          previous[index] = new[index];

          //Set the zones that has the duration update
          entry = (vcIndex * DAYS_IN_WEEK * ZONE_NUMBER_MAX)
                + (dayOfWeek * ZONE_NUMBER_MAX)
                + zoneIndex;
          setDurations[entry] = true;

          //Issue the commands to change the solenoid type
          COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                        virtualCircuitList[vcIndex].commandTimer,
                        CMD_SET_DAY_OF_WEEK_2);
          COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                        virtualCircuitList[vcIndex].commandTimer,
                        CMD_ATI12);
          COMMAND_ISSUE(virtualCircuitList[vcIndex].commandQueue,
                        virtualCircuitList[vcIndex].commandTimer,
                        PROGRAMMING_COMPLETED);

          //Mark this VC as needing updating
          virtualCircuitList[vcIndex].programUpdated = virtualCircuitList[vcIndex].runtime + 1;
        }
      }
    }
  }
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
    COMMAND_ISSUE(pcCommandQueue, pcCommandTimer, CMD_ATI30); //Get myVC
    COMMAND_ISSUE(pcCommandQueue, pcCommandTimer, CMD_GET_SERVER_MODEL);
    COMMAND_ISSUE(pcCommandQueue, pcCommandTimer, CMD_ATI);   //Get Radio type
    COMMAND_ISSUE(pcCommandQueue, pcCommandTimer, CMD_ATI8);  //Get Radio unique ID
    COMMAND_ISSUE(pcCommandQueue, pcCommandTimer, CMD_ATA);   //Get all the VC states

    //Break the links if requested
    if (breakLinks)
      COMMAND_ISSUE(pcCommandQueue, pcCommandTimer, CMD_ATB); //Break all the VC links
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
    status = getZoneConfiguration(mysql, latchingSolenoid, manualOn, debug);
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
    FD_SET(STDIN, &readfds);
    FD_SET(radio, &readfds);

    printf("Waiting for VC data...\n");
    while (1)
    {
      //Set the timeout
      timeout.tv_sec = 0;
      timeout.tv_usec = POLL_TIMEOUT_USEC;

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
        status = getZoneConfiguration(mysql, tempLatching, tempManualOn, debug);
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
        compareSprinklerStartTimes(startTimeArray, tempStartTimes);
        compareZoneDurations(durationArray, tempZoneDuration);
      }

      //Update the files every second
      status = updateWeatherStation();
      if (status)
        break;

      //----------------------------------------
      // Check for timeout
      //----------------------------------------

      if (numfds == 0)
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
  continue;
//              break;
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
      }

      //----------------------------------------
      // Check for stalled commands
      //----------------------------------------

      //Deterine if the command processor is running
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
                  }
                  printf("    PC: %d (%s, %s)\n",
                         cmd,
                         commandName[cmd],
                         (pcActiveCommand < CMD_LIST_SIZE) ? "Active" : "Pending");
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
            printf("PC: Idle\n");

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
  } while (0);

  //Close the weather station files
  closeWeatherStation();

  //Done with the radio
  close(radio);
  return status;
}
