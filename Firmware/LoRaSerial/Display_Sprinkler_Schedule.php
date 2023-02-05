<?php
/**********************************************************************
* Copyright 2022 Lee Leahy (lpleahyjr@gmail.com)
* All rights reserved
*
* Input_Sprinkler_Schedule.php
*
* Input and display the sprinkler schedule
**********************************************************************/

//----------------------------------------------------------------------
//  Display the schedule by location
//----------------------------------------------------------------------

function displaySchedule($ControllerId)
{
  global $db;

  $Query = "SELECT * FROM view_sprinkler_schedule";
  if ($ControllerId)
    $Query .= " WHERE (SprinklerControllerId = " . $ControllerId . ")";
  $Query .= " ORDER BY SprinklerControllerName, ZoneNumber";
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
    //Add table header rows
    print ( "<table border=1>\n" );
    print ( "  <tr bgcolor=\"#c0ffff\">\n");
    print ("    <th>Location</th>\n");
    print ("    <th>Zone</th>\n");
    print ("    <th>Manual</th>\n");
    print ("    <th>Start Time</th>\n");
    print ("    <th>Duration</th>\n");
    print ("    <th>Sun</th>\n");
    print ("    <th>Mon</th>\n");
    print ("    <th>Tue</th>\n");
    print ("    <th>Wed</th>\n");
    print ("    <th>Thu</th>\n");
    print ("    <th>Fri</th>\n");
    print ("    <th>Sat</th>\n");
    print ("    <th>Updated</th>\n");
    print ("  </tr>\n");

    //Walk the rows
    $Color = 0;
    $PreviousName = 0;
    while ( $db_row = $Results->fetch_assoc ( ))
    {
      $Name = $db_row [ 'SprinklerControllerName' ];
      $Zone = $db_row [ 'ZoneNumber' ];
      $StartTime = $db_row [ 'StartTime' ];
      $Duration = $db_row [ 'Duration' ];
      $ManualOn = $db_row [ 'ManualOn' ];
      $Monday = $db_row [ 'Monday' ];
      $Tuesday = $db_row [ 'Tuesday' ];
      $Wednesday = $db_row [ 'Wednesday' ];
      $Thursday = $db_row [ 'Thursday' ];
      $Friday = $db_row [ 'Friday' ];
      $Saturday = $db_row [ 'Saturday' ];
      $Sunday = $db_row [ 'Sunday' ];
      $Created = $db_row [ 'Created' ];
      $LastUpdated = $db_row [ 'LastUpdated' ];
      if ($TestCode)
      {
        print ("Name: " . $Name . "<br>\n");
        print ("Zone: " . $Zone . "<br>\n");
        print ("StartTime: " . $StartTime . "<br>\n");
        print ("Duration: " . $Duration . "<br>\n");
        print ("ManualOn: " . $ManualOn . "<br>\n");
        print ("Monday: " . $Monday . "<br>\n");
        print ("Tuesday: " . $Tuesday . "<br>\n");
        print ("Wednesday: " . $Wednesday . "<br>\n");
        print ("Thursday: " . $Thursday . "<br>\n");
        print ("Friday: " . $Friday . "<br>\n");
        print ("Saturday: " . $Saturday . "<br>\n");
        print ("Sunday: " . $Sunday . "<br>\n");
        print ("Created: " . $Created . "<br>\n");
        print ("LastUpdated: " . $LastUpdated . "<br>\n");
      }

      //Select the location color
      if (!$PreviousName)
        $PreviousName = $Name;
      else if (strcmp($PreviousName, $Name) != 0)
        $Color += 1;
      $PreviousName = $Name;
      $BgColor = " bgcolor=\"#" . (($Color & 1) ? "ffffc0" : "ffffff") . "\"";

      //Add the table row
      print ( "  <tr>\n");
      print ("    <td" . $BgColor . ">" . $Name . "</td>\n");
      print ("    <td" . $BgColor . " align=\"center\">" . $Zone . "</td>\n");
      if ($ManualOn == 0)
        print ("    <td align=\"center\" bgcolor=\"#c0ffff\">Off</td>\n");
      else if ($ManualOn == 1)
        print ("    <td align=\"center\" bgcolor=\"#ffc0c0\">On</td>\n");
      else
        print ("    <td align=\"center\" bgcolor=\"#c0ffc0\">Sched</td>\n");
      print ("    <td align=\"right\"" . $BgColor . ">" . $StartTime . "</td>\n");
      print ("    <td align=\"right\"" . $BgColor . ">" . $Duration . "</td>\n");
      print ("    <td align=\"center\" bgcolor=\"#" . ($Sunday ? "c0ffc0\">X" : "ffc0c0\">&nbsp;") . "</td>\n");
      print ("    <td align=\"center\" bgcolor=\"#" . ($Monday ? "c0ffc0\">X" : "ffc0c0\">&nbsp;") . "</td>\n");
      print ("    <td align=\"center\" bgcolor=\"#" . ($Tuesday ? "c0ffc0\">X" : "ffc0c0\">&nbsp;") . "</td>\n");
      print ("    <td align=\"center\" bgcolor=\"#" . ($Wednesday ? "c0ffc0\">X" : "ffc0c0\">&nbsp;") . "</td>\n");
      print ("    <td align=\"center\" bgcolor=\"#" . ($Thursday ? "c0ffc0\">X" : "ffc0c0\">&nbsp;") . "</td>\n");
      print ("    <td align=\"center\" bgcolor=\"#" . ($Friday ? "c0ffc0\">X" : "ffc0c0\">&nbsp;") . "</td>\n");
      print ("    <td align=\"center\" bgcolor=\"#" . ($Saturday ? "c0ffc0\">X" : "ffc0c0\">&nbsp;") . "</td>\n");
      print ("    <td" . $BgColor . ">" . ($LastUpdated ? $LastUpdated : $Created) . "</td>\n");
      print ("  </tr>\n");
    }
    print ("</table>\n");
    $Results->free ( );
  }
}

print ("<h1>Sprinkler Schedule by Location</h1>\n");
displaySchedule(0);

//----------------------------------------------------------------------
//  Display the schedule by time
//----------------------------------------------------------------------

print ("<h1>Water Pressure: Schedule by Time</h1>\n");

$Query = "SELECT * FROM view_sprinkler_schedule";
$Query .= " WHERE ((ManualOn > 0)";
$Query .= " AND (Duration IS NOT NULL)";
$Query .= " AND (Duration != \"0:00:00\")";
$Query .= ") ORDER BY StartTime, SprinklerControllerName, ZoneNumber";
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
  //Add table header rows
  print ( "<table border=1>\n" );
  print ( "  <tr bgcolor=\"#c0ffff\">\n");
  print ("    <th>Location</th>\n");
  print ("    <th>Zone</th>\n");
  print ("    <th>Manual</th>\n");
  print ("    <th>Start Time</th>\n");
  print ("    <th>Duration</th>\n");
  print ("    <th>Sun</th>\n");
  print ("    <th>Mon</th>\n");
  print ("    <th>Tue</th>\n");
  print ("    <th>Wed</th>\n");
  print ("    <th>Thu</th>\n");
  print ("    <th>Fri</th>\n");
  print ("    <th>Sat</th>\n");
  print ("    <th>Water Meter</th>\n");
  print ("  </tr>\n");

  //Walk the rows
  $Color = 0;
  $PreviousTime = 0;
  while ( $db_row = $Results->fetch_assoc ( ))
  {
    $Name = $db_row [ 'SprinklerControllerName' ];
    $Zone = $db_row [ 'ZoneNumber' ];
    $StartTime = $db_row [ 'StartTime' ];
    $Duration = $db_row [ 'Duration' ];
    $ManualOn = $db_row [ 'ManualOn' ];
    $Monday = $db_row [ 'Monday' ];
    $Tuesday = $db_row [ 'Tuesday' ];
    $Wednesday = $db_row [ 'Wednesday' ];
    $Thursday = $db_row [ 'Thursday' ];
    $Friday = $db_row [ 'Friday' ];
    $Saturday = $db_row [ 'Saturday' ];
    $Sunday = $db_row [ 'Sunday' ];
    $MeterLocation = $db_row [ 'MeterLocation' ];
    if ($TestCode)
    {
      print ("Name: " . $Name . "<br>\n");
      print ("Zone: " . $Zone . "<br>\n");
      print ("StartTime: " . $StartTime . "<br>\n");
      print ("Duration: " . $Duration . "<br>\n");
      print ("ManualOn: " . $ManualOn . "<br>\n");
      print ("Monday: " . $Monday . "<br>\n");
      print ("Tuesday: " . $Tuesday . "<br>\n");
      print ("Wednesday: " . $Wednesday . "<br>\n");
      print ("Thursday: " . $Thursday . "<br>\n");
      print ("Friday: " . $Friday . "<br>\n");
      print ("Saturday: " . $Saturday . "<br>\n");
      print ("Sunday: " . $Sunday . "<br>\n");
      print ("MeterLocation: " . $MeterLocation . "<br>\n");
    }

    //Select the start time color
    if (!$PreviousTime)
      $PreviousTime = $StartTime;
    else if (strcmp($PreviousTime, $StartTime) != 0)
      $Color += 1;
    $PreviousTime = $StartTime;
    $BgColor = " bgcolor=\"#" . (($Color & 1) ? "ffffc0" : "ffffff") . "\"";

    //Add the table row
    print ( "  <tr>\n");
    print ("    <td" . $BgColor . ">" . $Name . "</td>\n");
    print ("    <td align=\"center\"" . $BgColor . ">" . $Zone . "</td>\n");
    if ($ManualOn == 0)
      print ("    <td align=\"center\" bgcolor=\"#ffc0c0\">Off</td>\n");
    else if ($ManualOn == 1)
      print ("    <td align=\"center\" bgcolor=\"#ffc0c0\">On</td>\n");
    else
      print ("    <td align=\"center\" bgcolor=\"#c0ffc0\">Sched</td>\n");
    print ("    <td align=\"right\"" . $BgColor . ">" . $StartTime . "</td>\n");
    print ("    <td align=\"right\"" . $BgColor . ">" . $Duration . "</td>\n");
    print ("    <td align=\"center\" bgcolor=\"#" . ($Sunday ? "c0ffc0\">X" : "ffc0c0\">&nbsp;") . "</td>\n");
    print ("    <td align=\"center\" bgcolor=\"#" . ($Monday ? "c0ffc0\">X" : "ffc0c0\">&nbsp;") . "</td>\n");
    print ("    <td align=\"center\" bgcolor=\"#" . ($Tuesday ? "c0ffc0\">X" : "ffc0c0\">&nbsp;") . "</td>\n");
    print ("    <td align=\"center\" bgcolor=\"#" . ($Wednesday ? "c0ffc0\">X" : "ffc0c0\">&nbsp;") . "</td>\n");
    print ("    <td align=\"center\" bgcolor=\"#" . ($Thursday ? "c0ffc0\">X" : "ffc0c0\">&nbsp;") . "</td>\n");
    print ("    <td align=\"center\" bgcolor=\"#" . ($Friday ? "c0ffc0\">X" : "ffc0c0\">&nbsp;") . "</td>\n");
    print ("    <td align=\"center\" bgcolor=\"#" . ($Saturday ? "c0ffc0\">X" : "ffc0c0\">&nbsp;") . "</td>\n");
    print ("    <td" . $BgColor . ">" . $MeterLocation . "</td>\n");
    print ("  </tr>\n");
  }
  print ("</table>\n");
  $Results->free ( );
}

?>
