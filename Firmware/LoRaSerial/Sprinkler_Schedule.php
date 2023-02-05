<?php
/**********************************************************************
* Copyright 2022 Lee Leahy (lpleahyjr@gmail.com)
* All rights reserved
*
* Sprinkler_Schedule.php
*
* Display the sprinkler schedule
**********************************************************************/

print ("<!DOCTYPE html>\n");
print ("<html>\n");
print ("  <head>\n");
print ("    <title>Sprinkler Sehedule</title>\n");
print ("  </head>\n");
print ("  <body>\n");

require ( "Variables.php" );

//----------------------------------------------------------------------
//  Connect to the database on the MySQL server
//----------------------------------------------------------------------

$db = new mysqli ( $DatabaseServer, $DatabaseUser, $DatabasePassword, $Database );
if ( $db->connect_errno ) {
  print ( "connect_errno: " . $db->connect_errno . "<br>\n" );
  exit;
}

//----------------------------------------------------------------------
//  Display the schedule by location
//----------------------------------------------------------------------

require ( "Display_Sprinkler_Schedule.php" );

//----------------------------------------------------------------------
//  Close the database
//----------------------------------------------------------------------

$db->close ( );

?>
  </body>
</html>
