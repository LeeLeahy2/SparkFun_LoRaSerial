//----------------------------------------
// Locals
//----------------------------------------

static QwiicMicroOLED oled;
static uint32_t displayTimer;

//Fonts
#include <res/qw_fnt_5x7.h>
#include <res/qw_fnt_8x16.h>
#include <res/qw_fnt_largenum.h>

//Icons
#include "icons.h"

//----------------------------------------
// Routines
//----------------------------------------

void beginDisplay()
{
  if (oled.begin() == true)
  {
    online.display = true;
    if (settings.splashScreenDelay)
      displaySplash();
  }
}

void displaySplash()
{
  int yPos;
  int fontHeight;

  if (online.display == true)
  {
    //Erase the buffer
    oled.erase();

    //Add the splash screen text to the buffer
    fontHeight = 8;
    yPos = 0;
    printTextCenter("Sprinkler", yPos, QW_FONT_5X7, 1, false); //text, y, font type, kerning, inverted
    yPos += fontHeight + 1;
    printTextCenter("Controller", yPos, QW_FONT_5X7, 1, false); //text, y, font type, kerning, inverted
    yPos += fontHeight + 1;
    printTextCenter("By", yPos, QW_FONT_5X7, 1, false); //text, y, font type, kerning, inverted
    yPos += fontHeight + 1;
    printTextCenter("Lee Leahy", yPos, QW_FONT_5X7, 1, false); //text, y, font type, kerning, inverted

    //Display the buffer
    oled.display();
    displayTimer = millis();
  }
}

void updateDisplay()
{
  uint32_t currentTime;
  uint8_t days;
  int fontHeight;
  uint8_t hours;
  int index;
  static uint32_t lastDisplayTime = 0;
  uint8_t minutes;
  uint8_t seconds;
  char * string;
  char timeString[16];
  int xPos;
  int yPos;

  //Don't do anything if the display is not attached
  currentTime = millis();
  if (online.display == false)
    return;

  //Turn off the splash screen
  if (displayTimer)
  {
    if ((currentTime - displayTimer) >= settings.splashScreenDelay)
    {
      //Erase the screen
      oled.erase();
      oled.display();
      displayTimer = 0;
    }
  }
  else
  {
    //Update the display on a periodic basis
    if ((currentTime - lastDisplayTime) >= settings.displayUpdate)
    {
      lastDisplayTime = currentTime;

      //----------------------------------------
      //Top line: Time
      //----------------------------------------

      //Compute the time
      getTime(timeOfDay, NULL, &hours, &minutes, &seconds, NULL);

      //Erase the buffer
      oled.erase();

      //Display the time
      string = timeString;
      *string++ = dayLetter[dayOfWeek];
      *string++ = ' ';
      *string++ = (hours > 9) ? '0' + (hours / 10) : ' ';
      *string++ = '0' + (hours % 10);
      *string++ = ':';
      *string++ = (minutes > 9) ? '0' + (minutes / 10) : '0';
      *string++ = '0' + (minutes % 10);
      *string++ = ':';
      *string++ = (seconds > 9) ? '0' + (seconds / 10) : '0';
      *string++ = '0' + (seconds % 10);
      *string++ = 0;

      //Add the time to the buffer
      fontHeight = 8;
      yPos = 0;
      printTextCenter(timeString, yPos, QW_FONT_5X7, 1, false); //text, y, font type, kerning, inverted

      //Skip to the next line
      yPos += fontHeight + 2;

      //----------------------------------------
      //2nd line: Zone numbers
      //----------------------------------------

      //Display the zone numbers
      xPos = online.quadRelay1 ? 3 : 8;
      for (index = 0; index < ZONE_NUMBER_MAX; index++)
      {
        printChar(xPos, yPos, '1' + index);
        xPos += Drop_Width + (online.quadRelay1 ? 0 : 6);
      }

      //----------------------------------------
      //3nd line: Icons
      //----------------------------------------

      //Display the drops
      yPos += fontHeight + 2;
      xPos = online.quadRelay1 ? 0 : 6;
      for (index = 0; index < ZONE_NUMBER_MAX; index++)
      {
        if (relayOn & (1 << index))
        {
          if (zoneOn & (1 << index))
            displayBitmap(xPos, yPos, UpArrow_Width, UpArrow_Height, UpArrow);
          else
            displayBitmap(xPos, yPos, DownArrow_Width, DownArrow_Height, DownArrow);
        }
        else if (zoneOn & (1 << index))
          displayBitmap(xPos, yPos, Drop_Width, Drop_Height, Drop);
        xPos += Drop_Width + (online.quadRelay1 ? 0 : 6);
      }

      //Skip to the next line
      yPos += Drop_Height + 2;

      //----------------------------------------
      //4nd line: Manual / Zone On Time
      //----------------------------------------

      //Display the duration
      if (zoneManualOn)
        printTextCenter("Manual", yPos, QW_FONT_5X7, 1, false); //text, y, font type, kerning, inverted
      else if (zoneOn)
      {
        lastDisplayTime = currentTime;

        //Compute the time
        getTime(currentTime - onTime + MILLISECONDS_IN_A_SECOND, &days, &hours, &minutes, &seconds, NULL);

        //Build the time string
        string = timeString;
        if (days)
        {
          *string++ = '0' + days;
          *string++ = ' ';
        }
        if (hours > 9)
          *string++ = '0' + (hours / 10);
        if (hours)
        {
          *string++ = '0' + (hours % 10);
          *string++ = ':';
        }
        if ((minutes > 9) || hours)
          *string++ = '0' + (minutes / 10);
        if (minutes)
        {
          *string++ = '0' + (minutes % 10);
          *string++ = ':';
        }
        if ((seconds > 9) || minutes)
          *string++ = '0' + (seconds / 10);
        *string++ = '0' + (seconds % 10);
        *string++ = 0;

        //Display the time the valve is on
        printTextCenter(timeString, yPos, QW_FONT_5X7, 1, false); //text, y, font type, kerning, inverted
      }

      //Skip to the next line
      yPos += fontHeight + 2;

      //----------------------------------------
      //5th line: Milliseconds / Manual time
      //----------------------------------------

      //Display the milliseconds
      if (settings.displayMilliseconds && (zoneOn | relayOn))
      {
        string = timeString;
        seconds = (((currentTime - onTime) / settings.displayUpdate) * settings.displayUpdate) % 1000;
        if (seconds > 99)
          *string++ = '0' + (seconds / 100);
        if (seconds > 9)
          *string++ = '0' + ((seconds % 100) / 10);
        *string++ = '0' + (seconds % 10);
        *string++ = 0;
        printTextCenter(timeString, yPos, QW_FONT_5X7, 1, false); //text, y, font type, kerning, inverted
      }
      else if (zoneManualOn)
      {
        //Compute the time
        getTime(currentTime - onTime + MILLISECONDS_IN_A_SECOND, &days, &hours, &minutes, &seconds, NULL);

        //Build the time string
        string = timeString;
        if (days)
        {
          *string++ = '0' + days;
          *string++ = ' ';
        }
        if (hours > 9)
          *string++ = '0' + (hours / 10);
        if (hours)
        {
          *string++ = '0' + (hours % 10);
          *string++ = ':';
        }
        if ((minutes > 9) || hours)
          *string++ = '0' + (minutes / 10);
        if (minutes)
        {
          *string++ = '0' + (minutes % 10);
          *string++ = ':';
        }
        if ((seconds > 9) || minutes)
          *string++ = '0' + (seconds / 10);
        *string++ = '0' + (seconds % 10);
        *string++ = 0;

        //Display the time the valve is on
        printTextCenter(timeString, yPos, QW_FONT_5X7, 1, false); //text, y, font type, kerning, inverted
      }

      //Display the buffer
      oled.display();
    }
  }
}

uint8_t getFontWidth(QwiicFont & fontType)
{
  uint8_t fontWidth = fontType.width;
  if (fontWidth == 8) fontWidth = 7; //8x16, but widest character is only 7 pixels.
  return fontWidth;
}

//Given text, and location, print text center of the screen
void printTextCenter(const char *text, uint8_t yPos, QwiicFont & fontType, uint8_t kerning, bool highlight) //text, y, font type, kearning, inverted
{
  oled.setFont(fontType);
  uint8_t fontWidth = getFontWidth(fontType);
  uint8_t xPos = (oled.getWidth() / 2) - ((strlen(text) * (fontWidth + kerning)) / 2) + 1;
  printText(text, xPos, yPos, fontType, kerning, highlight);
}

//Given text, a position, and kerning, print text to display
//This is helpful for squishing or stretching a string to appropriately fill the display
void printText(const char *text, uint8_t xPos, uint8_t yPos, QwiicFont & fontType, uint8_t kerning, bool highlight)
{
  oled.setFont(fontType);
  oled.setDrawMode(grROPXOR);
  uint8_t fontWidth = getFontWidth(fontType);
  int8_t xStart = xPos;
  for (int x = 0 ; x < strlen(text) ; x++)
  {
    printChar(xPos, yPos, text[x]);
    xPos += fontWidth + kerning;
  }
  if (highlight) //Draw a box, inverted over text
  {
    uint8_t textPixelWidth = strlen(text) * (fontWidth + kerning);

    //Error check
    int xBoxStart = xStart - 5;
    if (xBoxStart < 0) xBoxStart = 0;
    int xBoxEnd = textPixelWidth + 9;
    if (xBoxEnd > oled.getWidth() - 1) xBoxEnd = oled.getWidth() - 1;

    oled.rectangleFill(xBoxStart, yPos, xBoxEnd, 12, 1); //x, y, width, height, color
  }
}

void printChar(uint8_t xPos, uint8_t yPos, char text)
{
  oled.setCursor(xPos, yPos);
  oled.print(text);
}

void displayBitmap(uint8_t x, uint8_t y, uint8_t imageWidth, uint8_t imageHeight, const uint8_t *imageData)
{
  oled.bitmap(x, y, x + imageWidth, y + imageHeight, (uint8_t *)imageData, imageWidth, imageHeight);
}
