// GRBL controller interface. Works as a
// standalone device or man-in-the-middle mode.
// Provides a jog wheel and other inputs to
// manipulate GRBL's state. I use it with my
// 750mm X-Carve. The code in this file is
// mostly USB, communications and display
// management. The Encoder class contains
// the code that handles the operation of
// the jog wheel, which for now is limited
// to generating jog commands for Grbl.
//
// This code is written and intended for
// a Teensy 4.X board and the USB functionality
// is taken from the USBHost_t36 library
// examples.
#include <Arduino.h>
#include <SPI.h>
#include "Config.h"
#include "Encoder.h"
#include "ILI9488_t3.h"
#include "USBHost_t36.h"
#define USBBAUD 115200
uint32_t baud = USBBAUD;
uint32_t format = USBHOST_SERIAL_8N1;
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBHIDParser hid3(myusb);
USBSerial userial(myusb);

USBDriver *drivers[] = {&hub1, &hub2, &hid1, &hid2, &hid3, &userial};
#define CNT_DEVICES (sizeof(drivers)/sizeof(drivers[0]))
const char * driver_names[CNT_DEVICES] = {"Hub1", "Hub2",  "HID1", "HID2", "HID3", "USERIAL1" };
bool driver_active[CNT_DEVICES] = {false, false, false, false};

volatile uint8_t selector1Pos[12] = { HOME, AXIS_X, AXIS_Y, AXIS_Z, LOCKOUT, UNLOCK, RESET, 255, 255, 255, 255, 255 };
volatile uint8_t selector2Pos[12] = { JOG, XP1, X1, X10, X100, SET_AXIS_ZERO, MACRO1, MACRO2, 255, 255, 255, 255 };
volatile uint8_t *sel1Pos = NULL;
volatile uint8_t *sel2Pos = NULL;
volatile uint8_t emptySel = 255;
bool testPat = false;
uint8_t multiplierSet = 1;
bool jogMode = false;
void TestSelector1();
void TestSelector2();

// Encoder
#define ENC_A 15
#define ENC_B 16
#define ENC_SW 17
void EncoderInterrupt();

// Display
#define TFT_PWR 6
#define TFT_BL 14
#define TFT_RST 8
#define TFT_DC 9
#define TFT_CS 10
#define TFT_MOSI 11
#define TFT_SCLK 13
DMAMEM RAFB tftFB[ILI9488_TFTWIDTH * ILI9488_TFTHEIGHT];
ILI9488_t3 tft(ILI9488_t3(TFT_CS, TFT_DC, TFT_RST));
elapsedMillis displayRefresh;
void UpdateStatusDisplay();
void Blank();
#define tROWS 40
#define tCOLS 31
#define cROWS 28
char terminal[tROWS][tCOLS];
char cmdTerminal[cROWS][tCOLS];

void UpdateGrblTerminal();
void UpdateCommandTerminal();

// GRBL
#define GRBL_STATUS_TIMEOUT 1000
elapsedMillis grblStatusTimer;
char statusBuffer[300] = { '\0' };
char serialBuf[tCOLS] = { '\0' };
uint8_t serialBufPtr(0);
char UserialBuf[512] = { '\0' };
uint16_t UserialBufPtr(0);
void SendToGrbl(const char *msg);

#define GRBL_STATUS_ENUM \
ADD_ENUM(eStatus)\
ADD_ENUM(eMPosX)\
ADD_ENUM(eMPosY)\
ADD_ENUM(eMPosZ)\
ADD_ENUM(eFeed)\
ADD_ENUM(eSpindle)\
ADD_ENUM(ePlanner)\
ADD_ENUM(eRXBytes)\
ADD_ENUM(eLineNumber)\
ADD_ENUM(eOVFeed)\
ADD_ENUM(eOVRapid)\
ADD_ENUM(eOVSpindle)\
ADD_ENUM(eWCOX)\
ADD_ENUM(eWCOY)\
ADD_ENUM(eWCOZ)\
ADD_ENUM(ePins)

enum grblStatusT
{
#define ADD_ENUM(eVal) eVal,
   GRBL_STATUS_ENUM
#undef ADD_ENUM
   eStatusRange
};

const char *grblStatusImage[eStatusRange] =
{
#define ADD_ENUM(eVal) #eVal,
   GRBL_STATUS_ENUM
#undef ADD_ENUM
};

enum grblPushMsgT { eMSG, eGC, eHLP, eVER, eEcho, eOther, eG54, eG55, eG56, eG57, eG58, eG59,
                    eG28, eG30, eG92, eTLO, ePRB, ePushRange };


uint8_t grblLinesSent = 0;

#define SFIELDS 20
char *statusValues[eStatusRange] = { '\0' };
char *statusFields[SFIELDS] = { '\0' };
char *pushMsgValues[ePushRange] = { '\0' };
grblStatesT grblState = eUnkownState;
const char *grblTestStatus = "<Idle|MPos:1.000,2.000,3.000|FS:4.0,5>";
//<Idle|MPos:0.000,0.000,0.000|FS:0.0,0|WCO:13.0,6.0,2.0>            
const char *grblAlarmDesc [] =
{
"1  Hard limit triggered. Machine position is likely lost due to sudden and immediate halt. Re-homing is highly recommended.",
"2	G-code motion target exceeds machine travel. Machine position safely retained. Alarm may be unlocked.",
"3	Reset while in motion. Grbl cannot guarantee position. Lost steps are likely. Re-homing is highly recommended.",
"4	Probe fail. The probe is not in the expected initial state before starting probe cycle, where G38.2 and G38.3 is not triggered and G38.4 and G38.5 is triggered.",
"5	Probe fail. Probe did not contact the workpiece within the programmed travel for G38.2 and G38.4.",
"6	Homing fail. Reset during active homing cycle.",
"7	Homing fail. Safety door was opened during active homing cycle.",
"8	Homing fail. Cycle failed to clear limit switch when pulling off. Try increasing pull-off setting or check wiring.",
"9	Homing fail. Could not find limit switch within search distance. Defined as 1.5 * max_travel on search and 5 * pulloff on locate phases."
};

const char *grblErrorDesc [] =
{
"1	 G-code words consist of a letter and a value. Letter was not found.",
"2	 Numeric value format is not valid or missing an expected value.",
"3	 Grbl '$' system command was not recognized or supported.",
"4	 Negative value received for an expected positive value.",
"5	 Homing cycle is not enabled via settings.",
"6	 Minimum step pulse time must be greater than 3usec",
"7	 EEPROM read failed. Reset and restored to default values.",
"8	 Grbl '$' command cannot be used unless Grbl is IDLE. Ensures smooth operation during a job.",
"9	 G-code locked out during alarm or jog state",
"10 Soft limits cannot be enabled without homing also enabled.",
"11 Max characters per line exceeded. Line was not processed and executed.",
"12 (Compile Option) Grbl '$' setting value exceeds the maximum step rate supported.",
"13 Safety door detected as opened and door state initiated.",
"14 (Grbl-Mega Only) Build info or startup line exceeded EEPROM line length limit.",
"15 Jog target exceeds machine travel. Command ignored.",
"16 Jog command with no '=' or contains prohibited g-code.",
"17 Laser mode requires PWM output.",
"20 Unsupported or invalid g-code command found in block.",
"21 More than one g-code command from same modal group found in block.",
"22 Feed rate has not yet been set or is undefined.",
"23 G-code command in block requires an integer value.",
"24 Two G-code commands that both require the use of the XYZ axis words were detected in the block.",
"25 A G-code word was repeated in the block.",
"26 A G-code command implicitly or explicitly requires XYZ axis words in the block, but none were detected.",
"27 N line number value is not within the valid range of 1 - 9,999,999.",
"28 A G-code command was sent, but is missing some required P or L value words in the line.",
"29 Grbl supports six work coordinate systems G54-G59. G59.1, G59.2, and G59.3 are not supported.",
"30 The G53 G-code command requires either a G0 seek or G1 feed motion mode to be active. A different motion was active.",
"31 There are unused axis words in the block and G80 motion mode cancel is active.",
"32 A G2 or G3 arc was commanded but there are no XYZ axis words in the selected plane to trace the arc.",
"33 The motion command has an invalid target. G2, G3, and G38.2 generates this error, if the arc is impossible to generate or if the probe target is the current position.",
"34 A G2 or G3 arc, traced with the radius definition, had a mathematical error when computing the arc geometry. Try either breaking up the arc into semi-circles or quadrants, or redefine them with the arc offset definition.",
"35 A G2 or G3 arc, traced with the offset definition, is missing the IJK offset word in the selected plane to trace the arc.",
"36 There are unused, leftover G-code words that aren't used by any command in the block.",
"37 The G43.1 dynamic tool length offset command cannot apply an offset to an axis other than its configured axis. The Grbl default axis is the Z-axis.",
"38 Tool number greater than max supported value."
};

const char *grblSettings [] =
{
"0	   Step pulse time, microseconds",
"1	   Step idle delay, milliseconds",
"2	   Step pulse invert, mask",
"3	   Step direction invert, mask",
"4	   Invert step enable pin, boolean",
"5	   Invert limit pins, boolean",
"6	   Invert probe pin, boolean",
"10	Status report options, mask",
"11	Junction deviation, millimeters",
"12	Arc tolerance, millimeters",
"13	Report in inches, boolean",
"20	Soft limits enable, boolean",
"21	Hard limits enable, boolean",
"22	Homing cycle enable, boolean",
"23	Homing direction invert, mask",
"24	Homing locate feed rate, mm/min",
"25	Homing search seek rate, mm/min",
"26	Homing switch debounce delay, milliseconds",
"27	Homing switch pull-off distance, millimeters",
"30	Maximum spindle speed, RPM",
"31	Minimum spindle speed, RPM",
"32	Laser-mode enable, boolean",
"100	X-axis steps per millimeter",
"101	Y-axis steps per millimeter",
"102	Z-axis steps per millimeter",
"110	X-axis maximum rate, mm/min",
"111	Y-axis maximum rate, mm/min",
"112	Z-axis maximum rate, mm/min",
"120	X-axis acceleration, mm/sec^2",
"121	Y-axis acceleration, mm/sec^2",
"122	Z-axis acceleration, mm/sec^2",
"130	X-axis maximum travel, millimeters",
"131	Y-axis maximum travel, millimeters",
"132	Z-axis maximum travel, millimeters"
};

bool CompareStrings(const char *sz1, const char *sz2) {
  while (*sz2 != 0) {
    if (toupper(*sz1) != toupper(*sz2)) 
      return false;
    sz1++;
    sz2++;
  }
  return true; // end of string so show as match
}

void AddLineToGrblTerminal(const char *line)
{
   for(uint8_t r = 1; r < tROWS; ++r)
   {
      strncpy(terminal[r - 1], terminal[r], tCOLS - 1);
   }
   size_t l(strnlen(line, tCOLS - 1));
   strncpy(terminal[tROWS - 1], line, l);
}

void AddLineToCommandTerminal(const char *line)
{
   for(uint8_t r = 1; r < cROWS; ++r)
   {
      strncpy(cmdTerminal[r - 1], cmdTerminal[r], tCOLS - 1);
   }
   size_t l(strnlen(line, tCOLS - 1));
   strncpy(cmdTerminal[cROWS - 1], line, l);
}

void setup()
{
  // Display
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, 128);
  tft.begin();
  tft.setRotation(1);
  tft.setFrameBuffer(tftFB);
  memset(terminal, '\0', sizeof(terminal[0][0]) * tROWS * tCOLS);
  memset(cmdTerminal, '\0', sizeof(cmdTerminal[0][0]) * cROWS * tCOLS);
  //tft.fillScreen(ILI9488_BLACK);
  Blank();
  tft.updateScreenAsync();

  // IO
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  for (int i = 0; i < 5; i++) {
    digitalWrite(2, HIGH);
    delayMicroseconds(50);
    digitalWrite(2, LOW);
    delayMicroseconds(50);
  }
  while (!Serial && (millis() < 5000)) ; // wait for Arduino Serial Monitor
  //Serial.println("\n\nUSB Host Testing - Serial");
  myusb.begin();
  //Serial1.begin(115200);  // We will echo stuff Through Serial1...

   pinMode(HOME, INPUT_PULLUP);
   pinMode(AXIS_X, INPUT_PULLUP);
   pinMode(AXIS_Y, INPUT_PULLUP);
   pinMode(AXIS_Z, INPUT_PULLUP);
   pinMode(LOCKOUT, INPUT_PULLUP);
   pinMode(UNLOCK, INPUT_PULLUP);
   pinMode(RESET, INPUT_PULLUP);

   pinMode(JOG, INPUT_PULLUP);
   pinMode(XP1, INPUT_PULLUP);
   pinMode(X1, INPUT_PULLUP);
   pinMode(X10, INPUT_PULLUP);
   pinMode(X100, INPUT_PULLUP);
   pinMode(SET_AXIS_ZERO, INPUT_PULLUP);
   pinMode(MACRO1, INPUT_PULLUP);
   pinMode(MACRO2, INPUT_PULLUP);
//   pinMode(MACRO3, INPUT_PULLUP);
//   pinMode(CYCLE_START, INPUT_PULLUP);
   //pinMode(ENC_SW, INPUT_PULLUP);

   attachInterrupt(digitalPinToInterrupt(HOME), TestSelector1, CHANGE);
   attachInterrupt(digitalPinToInterrupt(AXIS_X), TestSelector1, CHANGE);
   attachInterrupt(digitalPinToInterrupt(AXIS_Y), TestSelector1, CHANGE);
   attachInterrupt(digitalPinToInterrupt(AXIS_Z), TestSelector1, CHANGE);
   attachInterrupt(digitalPinToInterrupt(LOCKOUT), TestSelector1, CHANGE);
   attachInterrupt(digitalPinToInterrupt(UNLOCK), TestSelector1, CHANGE);
   attachInterrupt(digitalPinToInterrupt(RESET), TestSelector1, CHANGE);

   attachInterrupt(digitalPinToInterrupt(JOG), TestSelector2, CHANGE);
   attachInterrupt(digitalPinToInterrupt(XP1), TestSelector2, CHANGE);
   attachInterrupt(digitalPinToInterrupt(X1), TestSelector2, CHANGE);
   attachInterrupt(digitalPinToInterrupt(X10), TestSelector2, CHANGE);
   attachInterrupt(digitalPinToInterrupt(X100), TestSelector2, CHANGE);
   attachInterrupt(digitalPinToInterrupt(SET_AXIS_ZERO), TestSelector2, CHANGE);
   attachInterrupt(digitalPinToInterrupt(MACRO1), TestSelector2, CHANGE);
   attachInterrupt(digitalPinToInterrupt(MACRO2), TestSelector2, CHANGE);

   for(uint8_t s = 0; s < 12; ++s)
   {
      if(selector1Pos[s] != 255 && digitalReadFast(selector1Pos[s]) == LOW)
      {
         sel1Pos = &selector1Pos[s];
         Serial.print("sel1 pos:");Serial.println(s);
      }
      if(selector2Pos[s] != 255 && digitalReadFast(selector2Pos[s]) == LOW)
      {
         sel2Pos = &selector2Pos[s];
         Serial.print("sel2 pos:");Serial.println(s);
      }
   }

   if(sel1Pos == NULL)
   {
      AddLineToCommandTerminal("Error: Failed to identify SEL1 position.");
   }

   if(sel2Pos == NULL)
   {
      AddLineToCommandTerminal("Error: Failed to identify SEL2 position.");
   }

   
  // Encoder pins
  Enc.begin(ENC_A, ENC_B, ENC_SW);
  attachInterrupt(digitalPinToInterrupt(ENC_A), EncoderInterrupt, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), EncoderInterrupt, CHANGE);
}

void EncoderInterrupt()
{
   Enc.HandleInterrupt();
}

void TestSelector1()
{
   sel1Pos = &emptySel;
   for(uint8_t s = 0; s < 12; ++s)
   {
      if(selector1Pos[s] != 255 && digitalReadFast(selector1Pos[s]) == LOW)
      {
         sel1Pos = &selector1Pos[s];
         break;
      }
   }
}

void TestSelector2()
{
   sel2Pos = &emptySel;
   for(uint8_t s = 0; s < 12; ++s)
   {
      if(selector2Pos[s] != 255 && digitalReadFast(selector2Pos[s]) == LOW)
      {
         sel2Pos = &selector2Pos[s];
         break;
      }
   }
}

void MonitorUSBDevices()
{
   // Print out information about different devices.
   for (uint8_t i = 0; i < CNT_DEVICES; i++)
   {
      if (*drivers[i] != driver_active[i])
      {
         char buf[128] = {'\0'};
         if (driver_active[i])
         {
            sprintf(buf, "*** Device %s - disconnected ***\n", driver_names[i]);
            driver_active[i] = false;
            //Serial.print(buf);
            grblState = eUnkownState;
            AddLineToCommandTerminal(buf);
         }
         else
         {
            sprintf(buf, "*** Device %s %x:%x - connected ***\n", driver_names[i], drivers[i]->idVendor(), drivers[i]->idProduct());
            driver_active[i] = true;
            //Serial.print(buf);
            AddLineToCommandTerminal(buf);

            const uint8_t *psz = drivers[i]->manufacturer();
            if (psz && *psz)
            {
               sprintf(buf, "  manufacturer: %s\n", psz);
               //Serial.print(buf);
               AddLineToCommandTerminal(buf);
            }
            psz = drivers[i]->product();
            if (psz && *psz)
            {
               sprintf(buf, "  product: %s\n", psz);
               //Serial.print(buf);
               AddLineToCommandTerminal(buf);
            }
            psz = drivers[i]->serialNumber();
            if (psz && *psz)
            {
               sprintf(buf, "  Serial: %s\n", psz);
               //Serial.print(buf);
               AddLineToCommandTerminal(buf);
            }

            // If this is a new Serial device.
            if (drivers[i] == &userial)
            {
               // Lets try first outputting something to our USerial to see if it will go out...
               userial.begin(baud);
            }
         }
         UpdateCommandTerminal();
      }
   }
}

char ReadUSBSerial()
{
   // Read from Grbl and forward to computer.
   char c(userial.read());
   Serial.write(c);
   return(c);
}

void ParseConnected(char *line)
{
   grblState = eConnected;
   AddLineToGrblTerminal(line);
   // Request current state and settings from Grbl.
   SendToGrbl(GRBL_VIEWSETTIMGS);
   SendToGrbl(GRBL_PARAMETERS);
   SendToGrbl(GRBL_GCODEPARSER);
   userial.write(GRBL_STATUSREPORT);
}

uint16_t ParseStatusMessage(char *msg)
{
   uint16_t p = 1;// Skip the '<' character.
   uint16_t f = 1;
   bool msgOK = false;
   // Pull the message off the ring buffer.
   statusFields[0] = &statusBuffer[0];
   while(p < 300 && f < SFIELDS)
   {
      statusBuffer[p - 1] = msg[p];
      if(statusBuffer[p - 1] == '>')
      {
         statusBuffer[p - 1] = '\0';
         msgOK = true;
         if(p < 299 && msg[p + 1] == '\n')
         {
            // Consume the included new line character from this status message.
            ++p;
         }
         break;
      }
      else if(statusBuffer[p - 1] == '|')
      {
         statusFields[f++] = &statusBuffer[p];
         statusBuffer[p - 1] = '\0';
      }
      ++p;
   }

   if(msgOK == true)
   {
      statusValues[eStatus] = statusFields[0];

      for(uint8_t i = 0; i < f; ++i)
      {
         if(strstr(statusFields[i], "MPos") != NULL)
         {
            char *ch = strtok(statusFields[i], ":,");
            
            ch = strtok(NULL, ":,");
            statusValues[eMPosX] = ch;
            ch = strtok(NULL, ":,");
            statusValues[eMPosY] = ch;
            ch = strtok(NULL, ":,");
            statusValues[eMPosZ] = ch;
         }
         else if(strstr(statusFields[i], "F") != NULL)
         {
            char *ch = strtok(statusFields[i], ":,");
            ch = strtok(NULL, ":,");
            statusValues[eFeed] = ch;
            
            if(strstr(statusFields[i], "FS") != NULL)
            {
               ch = strtok(NULL, ":,");
               statusValues[eSpindle] = ch;
            }
         }
         else if(strstr(statusFields[i], "Bf") != NULL)
         {
            char *ch = strtok(statusFields[i], ":,");
            
            ch = strtok(NULL, ":,");
            statusValues[ePlanner] = ch;
            ch = strtok(NULL, ":,");
            statusValues[eRXBytes] = ch;
         }
         else if(strstr(statusFields[i], "Ln") != NULL)
         {
            char *ch = strtok(statusFields[i], ":,");
            
            ch = strtok(NULL, ":,");
            statusValues[eLineNumber] = ch;
         }
         else if(strstr(statusFields[i], "Ov") != NULL)
         {
            char *ch = strtok(statusFields[i], ":,");
            
            ch = strtok(NULL, ":,");
            statusValues[eOVFeed] = ch;
            ch = strtok(NULL, ":,");
            statusValues[eOVRapid] = ch;
            ch = strtok(NULL, ":,");
            statusValues[eOVSpindle] = ch;
         }
         else if(strstr(statusFields[i], "WCO") != NULL)
         {
            char *ch = strtok(statusFields[i], ":,");

            ch = strtok(NULL, ":,");
            statusValues[eWCOX] = ch;
            ch = strtok(NULL, ":,");
            statusValues[eWCOY] = ch;
            ch = strtok(NULL, ":,");
            statusValues[eWCOZ] = ch;
         }
         else if(strstr(statusFields[i], "Pn") != NULL)
         {
            char *ch = strtok(statusFields[i], ":,");
            ch = strtok(NULL, ":,");
            
            ch = strtok(NULL, ":,");
            statusValues[ePins] = ch;
         }
      }

      UpdateStatusDisplay();
   }

   return(p);
}

void ParseBracketMessage(char *line)
{
   uint8_t breaks((strlen(line) / (tCOLS - 1)) + 1);
   for(uint8_t l = 0; l < breaks; ++l)
   {
      AddLineToGrblTerminal(&line[l * (tCOLS - 1)]);
   }
}

void ParseEEPROMData(char *line)
{
   
}

uint16_t ParseOther(char *msg)
{
   if(strlen(msg) > 0)
   {
      uint16_t p = 0;
      uint16_t q = 0;
      char buf[tCOLS] = { '\0' };
      bool redraw(false);
      while(msg[q] != '\0' || msg[q] == '>')
      {
         char c(msg[p]);
         if(c == '\n')
         {
            AddLineToGrblTerminal(buf);
            memset(buf, '\0', tCOLS);
            redraw = true;
            break;
         }
         else
         {
            buf[p] = c;
            p++;
            if(p >= tCOLS - 1)
            {
               p = 0;
               AddLineToGrblTerminal(buf);
               memset(buf, '\0', tCOLS);
               redraw = true;
            }
         }
         ++q;
      }

      if(redraw == true)
      {
         UpdateGrblTerminal();
      }
      return(q);
   }
   return(0);
}

void UpdateClockDisplay()
{
   tft.fillRect(380, 0, 100, 15, ILI9488_BLACK);
   tft.setCursor(350, 0);
   tft.setTextColor(ILI9488_WHITE);
   tft.setTextSize(2);
   tft.print("Hello:");tft.print((time_t)(millis() / 1000));
}

void Text(uint16_t x, uint16_t y, const char *text)
{
   tft.setCursor(x, y);
   tft.print(text);
}

void Blank()
{
   tft.fillScreen(ILI9488_BLACK);
   tft.setTextSize(2);
   tft.setCursor(300, 16);
   tft.print("Status");

   Text(240, 32, "Machine Position");
   Text(240, 64, "Work Position");
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

void UpdateStatusDisplay()
{
   if(statusValues[eStatus] != NULL)
   {
      tft.setTextColor(ILI9488_WHITE);

      int16_t x, y, x1, y1;
      uint16_t w, h;
      tft.fillRect(tft.width() - 100, 16, 100, 16, ILI9488_BLACK);
      tft.setTextSize(2);
      tft.getTextBounds(statusValues[eStatus], 0, 0, &x, &y, &w, &h);
      tft.setCursor(tft.width() - w, 16);
      if(CompareStrings(statusValues[eStatus], "Alarm"))
      {
         grblState = eAlarm;
         tft.setTextColor(ILI9488_BLACK, ILI9488_RED);
      }
      else if(CompareStrings(statusValues[eStatus], "Door")  ||
              CompareStrings(statusValues[eStatus], "Door:1") ||
              CompareStrings(statusValues[eStatus], "Door:2"))
      {
         grblState = eDoor;
         tft.setTextColor(ILI9488_BLACK, ILI9488_RED);
      }
      else if(CompareStrings(statusValues[eStatus], "Idle"))
      {
         grblState = eIdle;
         tft.setTextColor(ILI9488_BLACK, ILI9488_YELLOW);
      }
      else if(CompareStrings(statusValues[eStatus], "Run"))
      {
         grblState = eRun;
         tft.setTextColor(ILI9488_BLACK, ILI9488_LIGHTGREEN);
      }
      else if(CompareStrings(statusValues[eStatus], "Check"))
      {
         grblState = eCheck;
         tft.setTextColor(ILI9488_BLACK, ILI9488_MAGENTA);
      }
      else if(CompareStrings(statusValues[eStatus], "Sleep"))
      {
         grblState = eSleep;
         tft.setTextColor(ILI9488_BLACK, ILI9488_DARKCYAN);
      }
      else if(CompareStrings(statusValues[eStatus], "Jog"))
      {
         grblState = eJog;
         tft.setTextColor(ILI9488_BLACK, ILI9488_GREEN);
      }
      else if(CompareStrings(statusValues[eStatus], "Home"))
      {
         grblState = eHome;
         tft.setTextColor(ILI9488_BLACK, ILI9488_GREEN);
      }
      else if(CompareStrings(statusValues[eStatus], "Hold") ||
              CompareStrings(statusValues[eStatus], "Hold:0"))
      {
         grblState = eHold;
         tft.setTextColor(ILI9488_BLACK, ILI9488_ORANGE);
      }
      else if(CompareStrings(statusValues[eStatus], "Hold:1"))
      {
         grblState = eHold;
         tft.setTextColor(ILI9488_BLACK, ILI9488_MAROON);
      }
      else if(CompareStrings(statusValues[eStatus], "Queue") || // <--- What's the deal with this guy...
              CompareStrings(statusValues[eStatus], "Door:0") ||
              CompareStrings(statusValues[eStatus], "Door:3"))
      {
         grblState = eDoor;
         tft.setTextColor(ILI9488_BLACK, ILI9488_MAROON);
      }
      else
      {
         grblState = eUnkownState;
         tft.setTextColor(ILI9488_BLACK, ILI9488_WHITE);
      }
      tft.print(statusValues[eStatus]);

      if(statusValues[eMPosX] != NULL)
      {
         y = 48;
         tft.fillRect(140, y, tft.width() - 140, 16, ILI9488_BLACK);
         
         tft.setTextColor(ILI9488_BLACK, ILI9488_ORANGE);
         char s[32] = { '\0' };
         sprintf(s, "%s, %s, %s", statusValues[eMPosX], statusValues[eMPosY], statusValues[eMPosZ]);
         tft.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
         x = tft.width() - w;
         Text(x, y, s);
         
         if(statusValues[eWCOX] != NULL)
         {
            y = 80;
            tft.fillRect(140, y, tft.width() - 140, 16, ILI9488_BLACK);
            sprintf(s, "%.3f, %.3f, %.3f", atof(statusValues[eMPosX]) - atof(statusValues[eWCOX]), atof(statusValues[eMPosY]) - atof(statusValues[eWCOY]), atof(statusValues[eMPosZ]) - atof(statusValues[eWCOZ]));
            tft.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
            x = tft.width() - w;
            Text(x, y, s);
         }
      }

      if(statusValues[eFeed] != NULL)
      {
         x = 180;
         y = 0;
         tft.fillRect(x, y, 100, 16, ILI9488_BLACK);
         Text(x, y, statusValues[eFeed]);
      }
   
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

void UpdateGrblTerminal()
{
   tft.setTextSize(1);

   int16_t x, y;
   uint16_t cw, ch;
   tft.getTextBounds("0", 0, 0, &x, &y, &cw, &ch);
   tft.fillRect(0, 0, cw * tCOLS - 1, tft.height(), ILI9488_OLIVE);

   int8_t r = tROWS - 1;
   while(r >= 0)
   {
      if(terminal[r] != NULL)
      {
         if(strcmp(terminal[r], "ok") == 0)
         {
            --grblLinesSent;
            tft.setTextColor(ILI9488_BLACK, ILI9488_GREEN);
         }
         else if(strstr(terminal[r], "error") != NULL)
         {
            tft.setTextColor(ILI9488_BLACK, ILI9488_RED);
            if(strlen(terminal[r]) > 6)
            {
               uint8_t e(atoi(&terminal[r][6]));
               const char *ec(grblErrorDesc[e]);
               AddLineToCommandTerminal(ec);
            }
         }
         else if(strstr(terminal[r], "ALARM") != NULL)
         {
            tft.setTextColor(ILI9488_BLACK, ILI9488_ORANGE);
            if(strlen(terminal[r]) > 6)
            {
               uint8_t e(atoi(&terminal[r][6]));
               const char *ec(grblAlarmDesc[e]);
               AddLineToCommandTerminal(ec);
            }
         }
         else
         {
            tft.setTextColor(ILI9488_WHITE, ILI9488_BLACK);
         }
         Text(0,(r * ch) , terminal[r]);
      }
      --r;
   }
   tft.setTextColor(ILI9488_WHITE, ILI9488_BLACK);
}

void UpdateCommandTerminal()
{
   tft.setTextSize(1);

   int16_t x(280), y(96);
   uint16_t cw, ch;
   uint8_t rmax((tft.height() - y) / 8);

   tft.getTextBounds("0", 0, 0, &x, &y, &cw, &ch);
   tft.fillRect(280, 96, cw * tCOLS - 1, tft.height(), ILI9488_OLIVE);
   int8_t r = rmax - 1;
   y = (tft.height() - ch);
   while(r >= 0)
   {
      if(cmdTerminal[r] != NULL)
      {
         Text(280, y, cmdTerminal[r]);
      }
      --r;
      y -= ch;
   }
}

void ProcessLineFromGrbl()
{
   if(strncasecmp(UserialBuf, "ok", 2) == 0)
   {
      --grblLinesSent;
      ParseOther(UserialBuf);
   }
   else if(UserialBuf[0] == '<')
   {
      ParseStatusMessage(UserialBuf);
   }
   else if(strncasecmp(UserialBuf, "error:", 6) == 0)
   {
      ParseOther(UserialBuf);
   }
   else if(strncasecmp(UserialBuf, "ALARM:", 6) == 0)
   {
      ParseOther(UserialBuf);
   }
   else if(UserialBuf[0] == '[')
   {
      ParseBracketMessage(UserialBuf);
   }
   else if(UserialBuf[0] == '$')
   {
      ParseEEPROMData(UserialBuf);
   }
   else if(strncmp(UserialBuf, "Grbl ", 5) == 0)
   {
      ParseConnected(UserialBuf);
   }
}

void SendToGrbl(const char *msg)
{
   // The Arduino's println method in the Stream class
   // seems to line endings with \r\n. This causes GRBL
   // to interpret them as 2 separate lines which adds
   // unnecessary overhead. Instead, use print() and
   // send a single \n byte separately.
   userial.print(msg);
   userial.print('\n');
   ++grblLinesSent;
}

void loop()
{
   if(displayRefresh >= 500)
   {
       displayRefresh -= 500;
       UpdateClockDisplay();
       tft.updateScreenAsync();
   }

   myusb.Task();
   MonitorUSBDevices();
 
   Enc.Update(&userial);

   // Request an update of the status in GRBL
  if(grblStatusTimer >= GRBL_STATUS_TIMEOUT)
  {
     grblStatusTimer -= GRBL_STATUS_TIMEOUT;
     // Get a status update
     userial.write('?');
     //SendToGrbl(grblTestStatus);
  }

  // Pass the serial data from computer to Grbl via userial.
  if(Serial.available())
  {
      char c(Serial.read());
      // Forward from computer to GRBL
      userial.write(c);
      
      // This was not sent by us.
      if(c == '?')
      {
         grblStatusTimer -= (GRBL_STATUS_TIMEOUT * 2);
      }
      // Process locally for display
      else if(c == '\n')
      {
         AddLineToCommandTerminal(serialBuf);
         serialBufPtr = 0;
         memset(serialBuf, '\0', tCOLS);
         UpdateCommandTerminal();
      }
      else
      {
         serialBuf[serialBufPtr] = c;
         serialBufPtr++;
         if(serialBufPtr >= tCOLS - 1)
         {
            serialBufPtr = 0;
            AddLineToCommandTerminal(serialBuf);
            memset(serialBuf, '\0', tCOLS);
         }
      }
  }
  
  // Grab the messages from GRBL,
  // forward them to the computer,
  // then parse them here to
  // display the state on the pendant.
  if(userial.available())
  {
      char c(ReadUSBSerial());
      if(c == '\n')
      {
         UserialBuf[UserialBufPtr] = c;
         ProcessLineFromGrbl();
         UserialBufPtr = 0;
         memset(UserialBuf, '\0', 512);
      }
      else if(c != '\r') // Grbl sends back both \n and \r at the end of a line. We ignore the \r to make things easier for us when splitting lines in this code.
      {
         UserialBuf[UserialBufPtr] = c;
         UserialBufPtr++;
         if(UserialBufPtr >= 510)
         {
            UserialBufPtr = 0;
            ProcessLineFromGrbl();
            memset(UserialBuf, '\0', 512);
         }
      }
  }

  if(digitalReadFast(ENC_SW) == LOW && testPat == false)
  {
     testPat = true;
     if(*sel1Pos == 255 && *sel2Pos == 255)
     {
        DrawTestPattern();
     }
     else if(grblState == eJog && *sel2Pos != JOG)
     {
        // Stop the jog command imediately.
        userial.write(0x85);
        SendToGrbl("G4P0");
     }
  }
  else if(digitalReadFast(ENC_SW) == HIGH && testPat == true)
  {
     testPat = false;
     if(*sel1Pos == 255 && *sel2Pos == 255)
     {
        Blank();
        UpdateCommandTerminal();
        UpdateGrblTerminal();
        UpdateStatusDisplay();
     }
     else if(*sel1Pos == HOME && (grblState == eIdle || grblState == eAlarm))
     {
        SendToGrbl("$H");
     }
     else if(*sel1Pos == RESET)
     {
        userial.write(0x18);
     }
     else if(*sel1Pos == LOCKOUT)
     {
        SendToGrbl("$$");
     }
     else if(*sel1Pos == UNLOCK && (grblState == eIdle || grblState == eAlarm))
     {
        SendToGrbl("$X");
     }
  }
}