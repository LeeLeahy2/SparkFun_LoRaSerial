/**********************************************************************
* Copyright 2022 Lee Leahy (lpleahyjr@gmail.com)
* All rights reserved
*
* SprinklerServer.c
*
* Program to log data from the SparkFun weather station.
**********************************************************************/

#include <errno.h>
#include <mysql/mysql.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

char query[4096];

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

int main(int argc, char **argv)
{
  int columns;
  MYSQL * con;
  char * database;
  MYSQL_FIELD * fields;
  char * host;
  char * password;
  MYSQL_RES * result;
  MYSQL_ROW row;
  int rowCount;
  int rows;
  int status;
  char * username;

  //Display the help if necessary
  if (argc != 4)
  {
    fprintf(stderr, "%s   username   password   database\n", argv[0]);
    return -1;
  }
  printf("MySQL client version: %s\n", mysql_get_client_info());

  //Initialize the MySql library
  con = NULL;
  status = -1;
  do
  {
    con = mysql_init(NULL);
    if (con == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(con));
        break;
    }
printf("mysql_init successful\n");

    //Create a connection to the database server
    database = argv[3];
    host = "localhost";
    password = argv[2];
    username = argv[1];
printf ("host: %s\n", host);
printf("username: %s\n", username);
printf("password: %s\n", password);
printf("database: %s\n", database);
    
    if (mysql_real_connect(con, host, username, password,
            database, 0, NULL, 0) == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(con));
        break;
    }
printf("mysql_real_connect successful\n");

    //Build the query
    strcpy(query, "SELECT * FROM view_sprinkler_schedule");

    //Perform the query
    status = mysql_query(con, query);
    if (status)
    {
      status = mysql_errno(con);
      fprintf(stderr, "ERROR: mysql_query failed, status: %d\n", status);
      break;
    }
printf("mysql_query successful\n");

    //Get the results
    result = mysql_store_result(con);
    if (result == NULL)
    {
      status = mysql_errno(con);
      fprintf(stderr, "ERROR: mysql_store_result failed, status: %d\n", status);
      break;
    }
printf("mysql_store_result successful\n");

    //Get the number of fields and rows
    columns = mysql_num_fields(result);
printf("columns: %d\n", columns);
    fields = mysql_fetch_fields(result);
    rows = mysql_num_rows(result);
printf("rows: %d\n", rows);

    //Walk through the results
    rowCount = 0;
    while ((row = mysql_fetch_row(result)))
    {
      printf("Row %d\n", rowCount++);
      for(int i = 0; i < columns; i++)
        printf("    %s: %s\n", fields[i].name, row[i] ? row[i] : "NULL");
      printf("\n");
    }

    //Done with the results
    mysql_free_result(result);

    //The program executed successfully
    status = 0;
  } while (0);

  if (con)
    mysql_close(con);
  return status;
}
