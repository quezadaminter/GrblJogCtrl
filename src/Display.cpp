#include "Config.h"

bool CompareStrings(const char *sz1, const char *sz2)
{
  while (*sz2 != 0) {
    if (toupper(*sz1) != toupper(*sz2)) 
      return false;
    sz1++;
    sz2++;
  }
  return true; // end of string so show as match
}

void Text(uint16_t x, uint16_t y, const char *text)
{
   tft.setCursor(x, y);
   tft.print(text);
}

void Text(uint16_t x, uint16_t y, const char *text, size_t len)
{
   tft.setCursor(x, y);
   for(size_t x = 0; x <= len; ++x)
   {
      if(text[x] == '\0')
      {
         break;
      }
      tft.print(text[x]);
   }
}

void Blank()
{

   tft.fillScreen(ILI9488_BLACK);

   //int16_t x, y;//, x1, y1;
   //uint16_t w, h;
   //tft.setTextSize(1);
   //   tft.getTextBounds("H", 0, 0, &x, &y, &w, &h);
   //tft.setCursor(200, 160);
   //tft.print("H ");tft.print(w);tft.print("-");tft.print(h);
   //tft.setTextSize(2);
   //   tft.getTextBounds("H", 0, 0, &x, &y, &w, &h);
   //tft.setCursor(200, 176);
   //tft.print("H ");tft.print(w);tft.print("-");tft.print(h);
   //tft.setTextSize(3);
   //   tft.getTextBounds("8", 0, 0, &x, &y, &w, &h);
   //tft.setCursor(200, 208);
   //tft.print("H ");tft.print(w);tft.print("-");tft.print(h);

   tft.setTextSize(3);
   tft.setTextColor(ILI9488_WHITE, ILI9488_BLACK);
   Text(287, 30, "MPOS");
   Text(397, 30, "WPOS");
   tft.setTextSize(1);
   //Text(140, 0, VERSION_DATA[0]);
   //Text(140, 8, VERSION_DATA[1]);
   //Text(140, 16, VERSION_DATA[2]);
}

void UpdateLCDBrightnessIndication(bool selected)
{
   tft.fillRect(330, 144, 40, 16, ILI9488_BLACK);
   if(selected == true)
   {
      tft.setTextColor(ILI9488_LIGHTGREY, ILI9488_BLACK);
      tft.setTextSize(2);
      tft.setCursor(330, 144);
      tft.print(lcdBrightness);
   }
}

void DrawTestPattern()
{
  tft.setTextSize(1);
  for(int r = 0; r < tft.height(); r = r+ 16)
  {
     tft.drawFastHLine(0, r, tft.width(), ILI9488_YELLOW);
     tft.setCursor(0, r);
     tft.print(r);
  }
  for(int c = 0; c < tft.width(); c = c + 20)
  {
     tft.drawFastVLine(c, 0, tft.height(), ILI9488_YELLOW);
     tft.setCursor(c, 0);
     tft.print(c);
  }
}

void UpdateClockDisplay()
{
   tft.fillCircle(16, 16, 10, ILI9488_RED);
   if(heartBeatTimeout > 1000)
   {
      heartBeatTimeout -= 1000;
      tft.fillCircle(16, 16, 8, ILI9488_BLACK);
   }

   tft.fillCircle(46, 16, 10, ILI9488_BLUE);
   if(grblState.laserMode == false)
   {
      tft.fillCircle(46, 16, 8, ILI9488_BLACK);
   }
   else if(grblState.currentSpindleSpeed != 0 && grblState.spindleState != esM5)
   {
      // Laser is running.
      if(laserTimeout > 500)
      {
         laserTimeout -= 500;
         tft.fillCircle(46, 16, 8, ILI9488_GREEN);
      }
   }
   

   tft.fillRect(72, 20, 8, 8, btnShiftPressed ? ILI9488_WHITE : ILI9488_BLACK);
   tft.fillTriangle(66, 20, 76, 10, 86, 20, btnShiftPressed ? ILI9488_WHITE : ILI9488_BLACK);
}

void UpdateStatusDisplay()
{
   if(statusValues[eStatus] != NULL)
   {
      int16_t x, y;
      uint16_t w, h;
      tft.fillRect(340, 0, tft.width() - 340, 24, ILI9488_BLACK);
      tft.setTextSize(3);
      tft.getTextBounds(statusValues[eStatus], 0, 0, &x, &y, &w, &h);
      x = tft.width() - w;
      y = 0;
      if(CompareStrings(statusValues[eStatus], "Alarm"))
      {
         grblState.state = eAlarm;
         grblState.isHomed = false;
         tft.setTextColor(ILI9488_BLACK, ILI9488_RED);
      }
      else if(CompareStrings(statusValues[eStatus], "Door")  ||
              CompareStrings(statusValues[eStatus], "Door:1") ||
              CompareStrings(statusValues[eStatus], "Door:2"))
      {
         grblState.state = eDoor;
         tft.setTextColor(ILI9488_BLACK, ILI9488_RED);
      }
      else if(CompareStrings(statusValues[eStatus], "Idle"))
      {
         if(grblState.state == eHome)
         {
            // Just finished a successful homing cycle, reset the
            // distance tracker to the current machine position.
            grblState.cmdTravel[0] = GetMachinePosition('X'); //grblState.WCO[0];
            grblState.cmdTravel[1] = GetMachinePosition('Y');//grblState.WCO[1];
            grblState.cmdTravel[2] = GetMachinePosition('Z');//grblState.WCO[2];
            //if(negative space)
            //{
               //// If operating in negative space, then the max travel is
               //// redefined as the absolute value of the machine position at the pull-off point.
               grblState.axisMaxTravel[0] = fabs(GetMachinePosition('X'));
               grblState.axisMaxTravel[1] = fabs(GetMachinePosition('Y'));
               // Except for Z (on the X-Carve) since the limit switch is at the
               // machine origin instead of the machine limit.
               grblState.axisMaxTravel[2] = fabs(grblState.axisMaxTravel[2]);
            //}
         }
         // Consider if this is where we need to reset the machine position reference for
         // grblState.cmdTravel when entering the idle state from a Jog or Run state
         // in case the position change command was not sent by the handheld but instead
         // was sent by the g-code sender or a button matrix push to Go To 0 command for example.
         grblState.state = eIdle;
         tft.setTextColor(ILI9488_BLACK, ILI9488_YELLOW);
      }
      else if(CompareStrings(statusValues[eStatus], "Run"))
      {
         grblState.state = eRun;
         tft.setTextColor(ILI9488_BLACK, ILI9488_LIGHTGREEN);
      }
      else if(CompareStrings(statusValues[eStatus], "Check"))
      {
         grblState.state = eCheck;
         tft.setTextColor(ILI9488_BLACK, ILI9488_MAGENTA);
      }
      else if(CompareStrings(statusValues[eStatus], "Sleep"))
      {
         grblState.state = eSleep;
         tft.setTextColor(ILI9488_BLACK, ILI9488_DARKCYAN);
      }
      else if(CompareStrings(statusValues[eStatus], "Jog"))
      {
         grblState.state = eJog;
         tft.setTextColor(ILI9488_BLACK, ILI9488_GREEN);
      }
      else if(CompareStrings(statusValues[eStatus], "Home"))
      {
         grblState.state = eHome;
         grblState.isHomed = true;
         tft.setTextColor(ILI9488_BLACK, ILI9488_GREEN);
      }
      else if(CompareStrings(statusValues[eStatus], "Hold") ||
              CompareStrings(statusValues[eStatus], "Hold:0"))
      {
         grblState.state = eHold;
         tft.setTextColor(ILI9488_BLACK, ILI9488_ORANGE);
      }
      else if(CompareStrings(statusValues[eStatus], "Hold:1"))
      {
         grblState.state = eHold;
         tft.setTextColor(ILI9488_BLACK, ILI9488_MAROON);
      }
      else if(CompareStrings(statusValues[eStatus], "Queue") || // <--- What's the deal with this guy...
              CompareStrings(statusValues[eStatus], "Door:0") ||
              CompareStrings(statusValues[eStatus], "Door:3"))
      {
         grblState.state = eDoor;
         tft.setTextColor(ILI9488_BLACK, ILI9488_MAROON);
      }
      else
      {
         grblState.state = eUnknownState;
         tft.setTextColor(ILI9488_BLACK, ILI9488_WHITE);
      }
      Text(x, y, statusValues[eStatus]);

      char s[32] = { '\0' };
      x = 252;
      y = 52;
      
      tft.setTextSize(2);
      tft.setTextColor(ILI9488_BLACK, ILI9488_ORANGE);
      if(grblState.positionIsMPos == true)
      {
         sprintf(s, "X %8.3f %8.3f", grblState.RPOS[0], grblState.RPOS[0] - grblState.WCO[0]);
         Text(x, y, s);
         y = 68;
         sprintf(s, "Y %8.3f %8.3f", grblState.RPOS[1], grblState.RPOS[1] - grblState.WCO[1]);
         Text(x, y, s);
         y = 84;
         sprintf(s, "Z %8.3f %8.3f", grblState.RPOS[2], grblState.RPOS[2] - grblState.WCO[2]);
         Text(x, y, s);
      }
      else
      {
         sprintf(s, "X %8.3f %8.3f", grblState.RPOS[0] + grblState.WCO[0], grblState.RPOS[0]);
         Text(x, y, s);
         y = 68;
         sprintf(s, "Y %8.3f %8.3f", grblState.RPOS[1] + grblState.WCO[1], grblState.RPOS[1]);
         Text(x, y, s);
         y = 84;
         sprintf(s, "Z %8.3f %8.3f", grblState.RPOS[2] + grblState.WCO[2], grblState.RPOS[2]);
         Text(x, y, s);
      }
      


      tft.setTextSize(1);
      tft.setTextColor(ILI9488_WHITE, ILI9488_BLACK);

      x = 128;
      y = 8;
      if(grblState.plannerBlocksAvailable > 12)
      {
         tft.setTextColor(ILI9488_GREEN, ILI9488_BLACK);
      }
      else if(grblState.plannerBlocksAvailable > 6)
      {
         tft.setTextColor(ILI9488_YELLOW, ILI9488_BLACK);
      }
      else
      {
         tft.setTextColor(ILI9488_RED, ILI9488_BLACK);
      }
      sprintf(s, "BL %02d/%02d", grblState.plannerBlocksAvailable, grblState.plannerBlockCount);
      Text(x, y, s);

      y = 16;
      if(grblState.rxBufferBytesAvailable > 96)
      {
         tft.setTextColor(ILI9488_GREEN, ILI9488_BLACK);
      }
      else if(grblState.rxBufferBytesAvailable > 32)
      {
         tft.setTextColor(ILI9488_YELLOW, ILI9488_BLACK);
      }
      else
      {
         tft.setTextColor(ILI9488_RED, ILI9488_BLACK);
      }
      sprintf(s, "RX %03d/%03d", grblState.rxBufferBytesAvailable, grblState.rxBufferSize);
      Text(x, y, s);

      UpdateFeedRateDisplay();
      UpdateSpindleDisplay();

      //tft.setTextSize(1);
      //tft.setTextColor(ILI9488_WHITE, ILI9488_BLACK);
      //for(uint8_t i = 1; i < eStatusRange; ++i)
      //{
      //   tft.setCursor(0, 16 * (i));
      //   tft.print(&grblStatusImage[i][1]);
      //   if(statusValues[i] != NULL)
      //   {
      //      tft.setCursor(50, 16 * (i));
      //      tft.print(statusValues[i]);
      //   }
      //}
   }
}

void UpdateSelectorStates()
{
   tft.setTextColor(ILI9488_WHITE, ILI9488_BLACK);
   tft.setTextSize(2);
   uint16_t x = 300;
   uint16_t y = 160;
   tft.fillRect(x, y, tft.width() - x, 20, ILI9488_BLACK);
   tft.drawRect(x, y, 102, 20, ILI9488_CYAN);
   x = 305;
   y = 162;

   if(Selector1.Is() == SYSTEM)
   {
      tft.setTextColor(ILI9488_BLACK, ILI9488_RED);
   }
   Text(x, y, Selector1.Active()->label);

   tft.setTextColor(ILI9488_WHITE, ILI9488_BLACK);

   x = 415;
   y = 160;
   tft.drawRect(x, y, tft.width() - x, 20, ILI9488_PURPLE);
   x = 420;
   y = 162;
   Text(x, y, Selector2.Active()->label);
}

void UpdateFeedRateDisplay()
{
   tft.setTextColor(ILI9488_WHITE, ILI9488_BLACK);
   tft.setTextSize(1);
   uint16_t x = 332;
   uint16_t y = 112;
   char s[32] = { '\0' };
   sprintf(s, "F:%04ld,%7.2f,%7.2f", grblState.cmdFeedRate, grblState.programmedFeedRate, grblState.currentFeedRate);
   Text(x, y, s);
}

void UpdateSpindleDisplay()
{
   tft.setTextColor(ILI9488_WHITE, ILI9488_BLACK);
   tft.setTextSize(1);
   uint16_t x = 332;
   uint16_t y = 122;
   char s[32] = { '\0' };
   sprintf(s, "S:%05ld,%7.1f,%7.1f", grblState.cmdSpindleSpeed, grblState.programmedSpindleSpeed, grblState.currentSpindleSpeed);
   Text(x, y, s);
}

void UpdateParameterDisplay()
{
   tft.setTextColor(ILI9488_WHITE, ILI9488_BLACK);
   uint16_t x = 240;
   uint16_t y = 0;
   if(grblState.activeWorkspace != nullptr && grblState.activeWorkspace->id[0] != '\0')
   {
      tft.setTextSize(3);
      Text(x, y, grblState.activeWorkspace->id);
   }
   
   //tft.setTextSize(1);
   //y = 8;
   ////tft.fillRect(x, y, 100, 8, ILI9488_BLACK);
   //sprintf(s, "%s:%.3f,%.3f,%.3f", grblState.park28.id, grblState.park28.val3[0], grblState.park28.val3[1], grblState.park28.val3[2]);
   //Text(x, y, s);
   //y = 16;
   ////tft.fillRect(x, y, 100, 8, ILI9488_BLACK);
   //sprintf(s, "%s:%.3f,%.3f,%.3f", grblState.park30.id, grblState.park30.val3[0], grblState.park30.val3[1], grblState.park30.val3[2]);
   //Text(x, y, s);
}