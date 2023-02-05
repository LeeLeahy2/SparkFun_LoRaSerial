<?php
/**********************************************************************
* Copyright 2022 Lee Leahy (lpleahyjr@gmail.com)
* All rights reserved
*
* Input_Sprinkler_Schedule.php
*
* Input and display the sprinkler schedule
**********************************************************************/

print ("<!DOCTYPE html>\n");
print ("<html>\n");
print ("  <head>\n");
print ("    <title>Sprinkler Sehedule</title>\n");
print ("  </head>\n");
print ("  <body>\n");

require ( "Variables.php" );

//----------------------------------------------------------------------
//  Get the parameters
//----------------------------------------------------------------------

//  Get the parameters
$ControllerIdSet = 0;
if (isset($_POST [ "ControllerIdSet" ]))
  $ControllerIdSet = $_POST [ "ControllerIdSet" ];

$SprinklerControllerId = 0;
if (isset($_POST [ "SprinklerControllerId" ]))
  $SprinklerControllerId = $_POST [ "SprinklerControllerId" ];

$SprinklerValveId = 0;
if (isset($_POST [ "SprinklerValveId" ]))
  $SprinklerValveId = $_POST [ "SprinklerValveId" ];

$StartTime = 0;
if (isset($_POST [ "StartTime" ]) && strtotime($_POST [ "StartTime" ]))
{
  $StartTimeArray = date_parse($_POST [ "StartTime" ]);
  $StartTime = $StartTimeArray["hour"];
  $StartTime .= ":" . (($StartTimeArray["minute"] < 10) ? "0" : "") . $StartTimeArray["minute"];
  $StartTime .= ":" . (($StartTimeArray["second"] < 10) ? "0" : "") . $StartTimeArray["second"];
}

$Duration = 0;
if (isset($_POST [ "Duration" ]) && strtotime($_POST [ "Duration" ]))
{
  $DurationArray = date_parse($_POST [ "Duration" ]);
  $Duration = $DurationArray["hour"];
  $Duration .= ":" . (($DurationArray["minute"] < 10) ? "0" : "") . $DurationArray["minute"];
  $Duration .= ":" . (($DurationArray["second"] < 10) ? "0" : "") . $DurationArray["second"];
}

$ManualOn = 0;
if (isset($_POST [ "ManualOn" ]) && is_numeric($_POST [ "ManualOn" ])
  && ($_POST [ "ManualOn" ] >= 0) && ($_POST [ "ManualOn" ] <= 2))
  $ManualOn = $_POST [ "ManualOn" ];

$Monday = 0;
if (isset($_POST [ "Monday" ]) && (strcmp($_POST [ "Monday" ], "on") == 0))
  $Monday = 1;

$Tuesday = 0;
if (isset($_POST [ "Tuesday" ]) && (strcmp($_POST [ "Tuesday" ], "on") == 0))
  $Tuesday = 1;

$Wednesday = 0;
if (isset($_POST [ "Wednesday" ]) && (strcmp($_POST [ "Wednesday" ], "on") == 0))
  $Wednesday = 1;

$Thursday = 0;
if (isset($_POST [ "Thursday" ]) && (strcmp($_POST [ "Thursday" ], "on") == 0))
  $Thursday = 1;

$Friday = 0;
if (isset($_POST [ "Friday" ]) && (strcmp($_POST [ "Friday" ], "on") == 0))
  $Friday = 1;

$Saturday = 0;
if (isset($_POST [ "Saturday" ]) && (strcmp($_POST [ "Saturday" ], "on") == 0))
  $Saturday = 1;

$Sunday = 0;
if (isset($_POST [ "Sunday" ]) && (strcmp($_POST [ "Sunday" ], "on") == 0))
  $Sunday = 1;

$UpdateControl = 0;
if (isset($_POST [ "UpdateControl" ]) && is_numeric($_POST [ "UpdateControl" ]))
  $UpdateControl = 1;

$UpdateSchedule = 0;
if (isset($_POST [ "UpdateSchedule" ]) && is_numeric($_POST [ "UpdateSchedule" ]))
  $UpdateSchedule = 1;

if ( $TestCode )
{
  print ( "ControllerIdSet: " . $ControllerIdSet . "<br>\n");
  print ( "SprinklerControllerId: " . $SprinklerControllerId . "<br>\n");
  print ( "SprinklerValveId: " . $SprinklerValveId . "<br>\n" );
  print ( "StartTime: " . $StartTime . "<br>\n" );
  print ( "Duration: " . $Duration . "<br>\n" );
  print ( "ManualOn: " . $ManualOn . "<br>\n" );
  print ( "Monday: " . $Monday . "<br>\n" );
  print ( "Tuesday: " . $Tuesday . "<br>\n" );
  print ( "Wednesday: " . $Wednesday . "<br>\n" );
  print ( "Thursday: " . $Thursday . "<br>\n" );
  print ( "Friday: " . $Friday . "<br>\n" );
  print ( "Saturday: " . $Saturday . "<br>\n" );
  print ( "Sunday: " . $Sunday . "<br>\n" );
  print ( "UpdateControl: " . $UpdateControl . "<br>\n" );
  print ( "UpdateSchedule: " . $UpdateSchedule . "<br>\n" );
}

//----------------------------------------------------------------------
//  Connect to the database on the MySQL server
//----------------------------------------------------------------------

$db = new mysqli ( $DatabaseServer, $DatabaseUser, $DatabasePassword, $Database );
if ( $db->connect_errno )
{
  print ( "connect_errno: " . $db->connect_errno . "<br>\n" );
  exit;
}

//----------------------------------------------------------------------
// Update the sprinkler schedule
//----------------------------------------------------------------------

//  Only perform the action once, when the session variable does not
//  equal the value passed in.
if ($AllowFormUpdate && $SprinklerValveId && $UpdateSchedule)
{
  //Determine the operation to use
  $Query = "SELECT * FROM sprinkler_schedule WHERE (SprinklerValveId = " . $SprinklerValveId . ")";
  if ( $TestCode )
    print ( "Query: " . $Query . "<br>\n" );

  //  Get the results
  $Results = $db->query ( $Query );
  if ( NULL == $Results )
  {
    print ("<table>\n");
    print ("  <tr><td bgcolor=\"#ffc0c0\"><h1>ERROR: Bad query!</h1></td></tr>\n");
    print ("  <tr><td bgcolor=\"#ffc0c0\"><h3>Error: " . mysqli_error($db) . "</h3></td></tr>\n");
    print ("  <tr><td bgcolor=\"#ffc0c0\">" . $Query . "</td></tr>\n");
    print ("</table>\n");
  }
  else
  {
    if ($Results->num_rows)
    {
      //Update the schedule for this valve
      $Update = "UPDATE sprinkler_schedule";
      $Update .= " SET StartTime = " . ($StartTime ? "\"" . $StartTime . "\"" : "NULL");
      $Update .= ", Duration = " . ($Duration ? "\"" . $Duration . "\"" : "NULL");
      $Update .= ", Monday = " . $Monday;
      $Update .= ", Tuesday = " . $Tuesday;
      $Update .= ", Wednesday = " . $Wednesday;
      $Update .= ", Thursday = " . $Thursday;
      $Update .= ", Friday = " . $Friday;
      $Update .= ", Saturday = " . $Saturday;
      $Update .= ", Sunday = " . $Sunday;
      $Update .= " WHERE (SprinklerValveId = " . $SprinklerValveId . ")";
      if ( $TestCode )
        print ( "Update: " . $Update . "<br>\n" );
      if ( ! $db->query ( $Update )) {

        //  Update failed
        print ( "<table>\n" );
        print ( "  <tr bgcolor=\"#ff8080\">\n" );
        print ( "    <td><h1>ERROR: Failed to update the sprinkler schedule!</h1></td>\n" );
        print ( "  <tr><td bgcolor=\"#ff8080\"><h3>Error: " . mysqli_error($db) . "</h3></td></tr>\n");
        print ( "  <tr><td bgcolor=\"#ff8080\">" . $Insert . "</td></tr>\n");
        print ( "  </tr>\n" );
        print ( "</table>\n" );
      } else {

        //  Update successful
        print ( "<table>\n" );
        print ( "  <tr bgcolor=\"#80ff80\">\n" );
        print ( "    <td><h1>Sprinkler schedule updated successfully!</h1></td>\n" );
        print ( "  </tr>\n" );
        print ( "</table>\n" );
      }
      print ( "<br><hr>\n" );
    }
    else
    {
      //Insert the schedule for this valve
      $Insert = "INSERT INTO sprinkler_schedule ( SprinklerValveId, ManualOn, StartTime,";
      $Insert .= " Duration, Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday )";
      $Insert .= " VALUES ( ";
      $Insert .= $SprinklerValveId;
      $Insert .= ", 2";
      $Insert .= ", " . ($StartTime ? "\"" . $StartTime . "\"" : NULL);
      $Insert .= ", " . ($Duration ? "\"" . $Duration . "\"" : NULL);
      $Insert .= ", " . $Monday;
      $Insert .= ", " . $Tuesday;
      $Insert .= ", " . $Wednesday;
      $Insert .= ", " . $Thursday;
      $Insert .= ", " . $Friday;
      $Insert .= ", " . $Saturday;
      $Insert .= ", " . $Sunday;
      $Insert .= " )";
      if ( $TestCode )
        print ( "Insert: " . $Insert . "<br>\n" );
      if ( ! $db->query ( $Insert )) {

        //  Insert failed
        print ( "<table>\n" );
        print ( "  <tr bgcolor=\"#ff8080\">\n" );
        print ( "    <td><h1>ERROR: Failed to add the sprinkler schedule!</h1></td>\n" );
        print ( "  <tr><td bgcolor=\"#ff8080\"><h3>Error: " . mysqli_error($db) . "</h3></td></tr>\n");
        print ( "  <tr><td bgcolor=\"#ff8080\">" . $Insert . "</td></tr>\n");
        print ( "  </tr>\n" );
        print ( "</table>\n" );
      } else {

        //  Insert successful
        print ( "<table>\n" );
        print ( "  <tr bgcolor=\"#80ff80\">\n" );
        print ( "    <td><h1>Sprinkler schedule added successfully!</h1></td>\n" );
        print ( "  </tr>\n" );
        print ( "</table>\n" );
      }
      print ( "<br><hr>\n" );
    }
    $Results->free ( );
  }
}

//----------------------------------------------------------------------
// Manually turn on or off the zone
//----------------------------------------------------------------------

//  Only perform the action once, when the session variable does not
//  equal the value passed in.
if ($AllowFormUpdate && $SprinklerValveId && $UpdateControl && ($ManualOn >= 0) && ($ManualOn <= 2))
{
  //Determine the operation to use
  $Query = "SELECT * FROM sprinkler_schedule WHERE (SprinklerValveId = " . $SprinklerValveId . ")";
  if ( $TestCode )
    print ( "Query: " . $Query . "<br>\n" );

  //  Get the results
  $Results = $db->query ( $Query );
  if ( NULL == $Results )
  {
    print ("<table>\n");
    print ("  <tr><td bgcolor=\"#ffc0c0\"><h1>ERROR: Bad query!</h1></td></tr>\n");
    print ("  <tr><td bgcolor=\"#ffc0c0\"><h3>Error: " . mysqli_error($db) . "</h3></td></tr>\n");
    print ("  <tr><td bgcolor=\"#ffc0c0\">" . $Query . "</td></tr>\n");
    print ("</table>\n");
  }
  else
  {
    if ($Results->num_rows)
    {
      //Update the schedule for this valve
      $Update = "UPDATE sprinkler_schedule";
      $Update .= " SET ManualOn = " . $ManualOn;
      $Update .= " WHERE (SprinklerValveId = " . $SprinklerValveId . ")";
      if ( $TestCode )
        print ( "Update: " . $Update . "<br>\n" );
      if ( ! $db->query ( $Update )) {

        //  Update failed
        print ( "<table>\n" );
        print ( "  <tr bgcolor=\"#ff8080\">\n" );
        print ( "    <td><h1>ERROR: Failed to update the zone state!</h1></td>\n" );
        print ( "  <tr><td bgcolor=\"#ff8080\"><h3>Error: " . mysqli_error($db) . "</h3></td></tr>\n");
        print ( "  <tr><td bgcolor=\"#ff8080\">" . $Insert . "</td></tr>\n");
        print ( "  </tr>\n" );
        print ( "</table>\n" );
      } else {

        //  Update successful
        print ( "<table>\n" );
        print ( "  <tr bgcolor=\"#80ff80\">\n" );
        print ( "    <td><h1>Zone updated successfully!</h1></td>\n" );
        print ( "  </tr>\n" );
        print ( "</table>\n" );
      }
      print ( "<br><hr>\n" );
    }
    else
    {
      //Insert the schedule for this valve
      $Insert = "INSERT INTO sprinkler_schedule ( SprinklerValveId, ManualOn )";
      $Insert .= " VALUES ( ";
      $Insert .= $SprinklerValveId;
      $Insert .= ", " . $ManualOn;
      $Insert .= " )";
      if ( $TestCode )
        print ( "Insert: " . $Insert . "<br>\n" );
      if ( ! $db->query ( $Insert )) {

        //  Insert failed
        print ( "<table>\n" );
        print ( "  <tr bgcolor=\"#ff8080\">\n" );
        print ( "    <td><h1>ERROR: Failed to update the zone state!</h1></td>\n" );
        print ( "  <tr><td bgcolor=\"#ff8080\"><h3>Error: " . mysqli_error($db) . "</h3></td></tr>\n");
        print ( "  <tr><td bgcolor=\"#ff8080\">" . $Insert . "</td></tr>\n");
        print ( "  </tr>\n" );
        print ( "</table>\n" );
      } else {

        //  Insert successful
        print ( "<table>\n" );
        print ( "  <tr bgcolor=\"#80ff80\">\n" );
        print ( "    <td><h1>Zone updated successfully!</h1></td>\n" );
        print ( "  </tr>\n" );
        print ( "</table>\n" );
      }
      print ( "<br><hr>\n" );
    }
    $Results->free ( );
  }
}

//----------------------------------------------------------------------
//  Display the schedule by location
//----------------------------------------------------------------------

require ( "Display_Sprinkler_Schedule.php" );

//----------------------------------------------------------------------
//  Display the controller selection form
//----------------------------------------------------------------------

if ( $MH2_All_Privilege )
{
  //  Build the form
  print ("<br><hr>\n");
  print ( "<h1>Select Controller</h1>\n" );
  print ( "<form action=\"" . $WebPage . "\" method=\"post\">\n" );

  //  Add the hidden values
  print ( $FormValue_OneTimeValue );
  print ( $FormValue_TestCode );
  print ( "  <input type=\"hidden\" name=\"ControllerIdSet\" value=\"1\"></td>\n" );

  //  Display the form
  print ( "  <table>\n" );

  //--------------------
  //  SprinklerControllerId
  //--------------------

  $Query = "SELECT * FROM view_sprinkler_valve GROUP BY SprinklerControllerName";
  $Query .= " ORDER BY SprinklerControllerName";
  if ( $TestCode )
    print ( "Query: " . $Query . "<br>\n" );

  //  Start the row
  print ( "    <tr>\n" );
  print ( "      <th>Location</th>\n" );
  print ( "      <td width=10><br></td>\n" );
  print ( "      <td>\n" );
  print ( "        <select name=\"SprinklerControllerId\">\n" );

  //  Walk the results
  $Results = $db->query ( $Query );
  if ( NULL != $Results )
  {
    while ( $db_row = $Results->fetch_assoc ( ))
    {
      $Id = $db_row [ 'SprinklerControllerId' ];
      $Name = $db_row [ 'SprinklerControllerName' ];

      //Add the ID
      print ( "<option value=\"" . $Id . "\"" );
      if ($id == $SprinklerControllerId)
        print ( " default");
      print ( ">" );
      print ( $Name );
      print ( "</option>\n" );
    }
    $Results->free ( );
  }

  // Finish the row
  print ( "        </select>\n" );
  print ( "      </td>\n" );
  print ( "    </tr>\n" );

  //--------------------
  //  Select button
  //--------------------

  print ( "    <tr>\n" );
  print ( "      <td><br></td>\n" );
  print ( "      <td><br></td>\n" );
  print ( "      <td align=center><input type=\"submit\" value=\"Select\"></td>\n" );
  print ( "    </tr>\n" );

  print ( "  </table>\n" );

  print ( "</form>\n" );
}

//----------------------------------------------------------------------
//  Display the zone control form
//----------------------------------------------------------------------

if ( $MH2_All_Privilege && $ControllerIdSet )
{
  //  Build the form
  print ("<br><hr>\n");
  print ( "<h1>Zone Control</h1>\n" );
  print ( "<form action=\"" . $WebPage . "\" method=\"post\">\n" );

  //  Add the hidden values
  print ( $FormValue_OneTimeValue );
  print ( $FormValue_TestCode );
  print ( "  <input type=\"hidden\" name=\"ControllerIdSet\" value=\"1\"></td>\n" );
  print ( "  <input type=\"hidden\" name=\"SprinklerControllerId\" value=\"" . $SprinklerControllerId . "\"></td>\n" );
  print ( "  <input type=\"hidden\" name=\"UpdateControl\" value=\"1\"></td>\n" );

  //  Display the form
  print ( "  <table>\n" );

  //--------------------
  //  SprinklerValveId
  //--------------------

  $Query = "SELECT * FROM view_sprinkler_valve";
  $Query .= " WHERE (SprinklerControllerId = " . $SprinklerControllerId . ")";
  $Query .= " ORDER BY ZoneNumber";
  if ( $TestCode )
    print ( "Query: " . $Query . "<br>\n" );

  //  Start the row
  print ( "    <tr>\n" );
  print ( "      <th>Zone</th>\n" );
  print ( "      <td width=10><br></td>\n" );
  print ( "      <td>\n" );
  print ( "        <select name=\"SprinklerValveId\">\n" );

  //  Walk the results
  $Results = $db->query ( $Query );
  if ( NULL != $Results )
  {
    while ( $db_row = $Results->fetch_assoc ( ))
    {
      $Id = $db_row [ 'SprinklerValveId' ];
      $Zone = $db_row [ 'ZoneNumber' ];

      //Add the ID
      print ( "<option value=\"" . $Id . "\">" );
      print ( $Zone );
      print ( "</option>\n" );
    }
    $Results->free ( );
  }

  // Finish the row
  print ( "        </select>\n" );
  print ( "      </td>\n" );
  print ( "    </tr>\n" );

  //--------------------
  //  ManualOn
  //--------------------

  print ( "    <tr>\n" );
  print ( "      <th>Manual On</th>\n" );
  print ( "      <td><br></td>\n" );
  print ( "      <td>\n" );
  print ( "        <select name=\"ManualOn\">\n" );
  print ( "          <option value=\"2\">Schedule</option>\n" );
  print ( "          <option value=\"1\">On\n" );
  print ( "          <option value=\"0\">Off\n" );
  print ( "        </select>\n" );
  print ( "      </td>\n" );
  print ( "    </tr>\n" );

  //--------------------
  //  Update button
  //--------------------

  print ( "    <tr>\n" );
  print ( "      <td><br></td>\n" );
  print ( "      <td><br></td>\n" );
  print ( "      <td align=center><input type=\"submit\" value=\"Update\"></td>\n" );
  print ( "    </tr>\n" );

  print ( "  </table>\n" );

  print ( "</form>\n" );
}

//----------------------------------------------------------------------
//  Display the sprinkler schedule form
//----------------------------------------------------------------------

if ( $MH2_All_Privilege && $ControllerIdSet )
{
  //Display the schedule for this controller
  print ( "<br><hr>\n" );
  print ( "<h1>Schedule Update</h1>\n" );
  displaySchedule($SprinklerControllerId);

  //  Build the form
  print ( "<br>\n" );
  print ( "<form action=\"" . $WebPage . "\" method=\"post\">\n" );

  //  Add the hidden values
  print ( $FormValue_OneTimeValue );
  print ( $FormValue_TestCode );
  print ( "  <input type=\"hidden\" name=\"ControllerIdSet\" value=\"1\"></td>\n" );
  print ( "  <input type=\"hidden\" name=\"SprinklerControllerId\" value=\"" . $SprinklerControllerId . "\"></td>\n" );
  print ( "  <input type=\"hidden\" name=\"UpdateSchedule\" value=\"1\"></td>\n" );

  //  Display the form
  print ( "  <table>\n" );

  //--------------------
  //  SprinklerValveId
  //--------------------

  $Query = "SELECT * FROM view_sprinkler_valve";
  $Query .= " WHERE (SprinklerControllerId = " . $SprinklerControllerId . ")";
  $Query .= " ORDER BY ZoneNumber";
  if ( $TestCode )
    print ( "Query: " . $Query . "<br>\n" );

  //  Start the row
  print ( "    <tr>\n" );
  print ( "      <th>Zone</th>\n" );
  print ( "      <td width=10><br></td>\n" );
  print ( "      <td>\n" );
  print ( "        <select name=\"SprinklerValveId\">\n" );

  //  Walk the results
  $Results = $db->query ( $Query );
  if ( NULL != $Results )
  {
    while ( $db_row = $Results->fetch_assoc ( ))
    {
      $Id = $db_row [ 'SprinklerValveId' ];
      $Zone = $db_row [ 'ZoneNumber' ];

      //Add the ID
      print ( "<option value=\"" . $Id . "\">" );
      print ( $Zone );
      print ( "</option>\n" );
    }
    $Results->free ( );
  }

  // Finish the row
  print ( "        </select>\n" );
  print ( "      </td>\n" );
  print ( "    </tr>\n" );

  //--------------------
  //  Start Time
  //--------------------

  print ( "    <tr>\n" );
  print ( "      <th>Start Time</th>\n" );
  print ( "      <td><br></td>\n" );
  print ( "      <td><input type=\"text\" name=\"StartTime\"></td>\n" );
  print ( "    </tr>\n" );

  //--------------------
  //  Duration
  //--------------------

  print ( "    <tr>\n" );
  print ( "      <th>Duration</th>\n" );
  print ( "      <td><br></td>\n" );
  print ( "      <td><input type=\"text\" name=\"Duration\"></td>\n" );
  print ( "    </tr>\n" );

  //--------------------
  //  Sunday
  //--------------------

  print ( "    <tr>\n" );
  print ( "      <th>Sunday</th>\n" );
  print ( "      <td><br></td>\n" );
  print ( "      <td><input type=\"checkbox\" name=\"Sunday\"></td>\n" );
  print ( "    </tr>\n" );

  //--------------------
  //  Monday
  //--------------------

  print ( "    <tr>\n" );
  print ( "      <th>Monday</th>\n" );
  print ( "      <td><br></td>\n" );
  print ( "      <td><input type=\"checkbox\" name=\"Monday\"></td>\n" );
  print ( "    </tr>\n" );

  //--------------------
  //  Tuesday
  //--------------------

  print ( "    <tr>\n" );
  print ( "      <th>Tuesday</th>\n" );
  print ( "      <td><br></td>\n" );
  print ( "      <td><input type=\"checkbox\" name=\"Tuesday\"></td>\n" );
  print ( "    </tr>\n" );

  //--------------------
  //  Wednesday
  //--------------------

  print ( "    <tr>\n" );
  print ( "      <th>Wednesday</th>\n" );
  print ( "      <td><br></td>\n" );
  print ( "      <td><input type=\"checkbox\" name=\"Wednesday\"></td>\n" );
  print ( "    </tr>\n" );

  //--------------------
  //  Thursday
  //--------------------

  print ( "    <tr>\n" );
  print ( "      <th>Thursday</th>\n" );
  print ( "      <td><br></td>\n" );
  print ( "      <td><input type=\"checkbox\" name=\"Thursday\"></td>\n" );
  print ( "    </tr>\n" );

  //--------------------
  //  Friday
  //--------------------

  print ( "    <tr>\n" );
  print ( "      <th>Friday</th>\n" );
  print ( "      <td><br></td>\n" );
  print ( "      <td><input type=\"checkbox\" name=\"Friday\"></td>\n" );
  print ( "    </tr>\n" );

  //--------------------
  //  Saturday
  //--------------------

  print ( "    <tr>\n" );
  print ( "      <th>Saturday</th>\n" );
  print ( "      <td><br></td>\n" );
  print ( "      <td><input type=\"checkbox\" name=\"Saturday\"></td>\n" );
  print ( "    </tr>\n" );

  //--------------------
  //  Update button
  //--------------------

  print ( "    <tr>\n" );
  print ( "      <td><br></td>\n" );
  print ( "      <td><br></td>\n" );
  print ( "      <td align=center><input type=\"submit\" value=\"Update\"></td>\n" );
  print ( "    </tr>\n" );

  print ( "  </table>\n" );

  print ( "</form>\n" );
  print ( "<br><hr>\n" );
}

//----------------------------------------------------------------------
//  Close the database
//----------------------------------------------------------------------

$db->close ( );

?>
  </body>
</html>
