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
#include "ButtonMatrix.h"
#include "GcodeStreaming.h"
#include "ILI9488_t3.h"
#include "USBHost_t36.h"
#include "Version.h"

#ifdef USE_GDB
#include "TeensyDebug.h"
#pragma GCC optimize ("O0")
#endif

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

volatile uintCharStruct selector1Pos[12] =
{
   { SYSTEM, "SYSTEM" }, { AXIS_X, "X AXIS" }, { AXIS_Y, "Y AXIS" }, { AXIS_Z, "Z AXIS" },
   { SPINDLE, "SPINDLE" }, { FEEDRATE, "FEEDRATE" }, { LCDBRT, "LCD BRT" }, { FILES, "FILES" },
   {SEL_NONE, "\0" }, { SEL_NONE, "\0" }, { SEL_NONE, "\0" }, { SEL_NONE, "\0" }
};

volatile uintCharStruct selector2Pos[12] =
{
   { JOG, "JOG" }, { XP1, "0.1X" }, { X1, "1X" }, { X10, "10X" },
   { X100, "100X" }, { F1, "F1" }, { F2, "F2" }, { DEBUG, "DEBUG" },
   { SEL_NONE, "\0" }, {SEL_NONE, "\0" }, { SEL_NONE, "\0" }, { SEL_NONE, "\0" }
};
volatile uintCharStruct emptySel;// = { SEL_NONE, "NONE" };
volatile SelectorT Sel1Pos(&emptySel);
volatile SelectorT Sel2Pos(&emptySel);

bool pastEncoderSwitchPos = false;
uint8_t multiplierSet = 1;
bool jogMode = false;
void TestSelector1();
void TestSelector2();

// Button Matrix
#define BMSOL 20
#define BMCLK 19
#define BMQH  18
void HandleButtonChange(ButtonMatrix::ButtonMasksType button, ButtonMatrix::ButtonStateType state);
// Map matrix inputs to Grbl signals or commands

// Permanently assigned.
#define BTN_FEEDHOLD              ButtonMatrix::eR4C1
#define BTN_CYCLE_START_RESUME    ButtonMatrix::eR4C2
#define BTN_SHIFT                 ButtonMatrix::eR4C3

// Default mode
#define BTN_SetZ0     ButtonMatrix::eR1C1
#define BTN_GotoZ0    ButtonMatrix::eR1C2
#define BTN_LaserMode ButtonMatrix::eR1C3

#define BTN_SetY0   ButtonMatrix::eR2C1
#define BTN_GotoY0    ButtonMatrix::eR2C2
#define BTN_GotoX0Y0 ButtonMatrix::eR2C3

#define BTN_SetX0    ButtonMatrix::eR3C1
#define BTN_GotoX0    ButtonMatrix::eR3C2
#define BTN_M3M5    ButtonMatrix::eR3C3

bool btnShiftPressed = false;
// F1 mode
#define BTN_ProbeZ     ButtonMatrix::eR1C1
#define BTN_G56     ButtonMatrix::eR1C2
#define BTN_G59 ButtonMatrix::eR1C3

#define BTN_ProbeY   ButtonMatrix::eR2C1
#define BTN_G55    ButtonMatrix::eR2C2
#define BTN_G58 ButtonMatrix::eR2C3

#define BTN_ProbeX    ButtonMatrix::eR3C1
#define BTN_G54    ButtonMatrix::eR3C2
#define BTN_G57    ButtonMatrix::eR3C3

// System Mode
#define BTN_HOME     ButtonMatrix::eR1C1
#define BTN_RUN_TOOL_CHANGE_MACRO  ButtonMatrix::eR1C2
#define BTN_TOGGLE_LASER_TEST_MODE ButtonMatrix::eR1C3

//#define BTN_NA   ButtonMatrix::eR2C1
//#define BTN_NA    ButtonMatrix::eR2C2
#define BTN_FORCE_RESYNC ButtonMatrix::eR2C3

#define BTN_SOFTRESET    ButtonMatrix::eR3C1
#define BTN_UNLOCK    ButtonMatrix::eR3C2
#define BTN_SLEEP    ButtonMatrix::eR3C3

//////////////////////////////////////////////////////////////

// Encoder
#define ENC_A 15
#define ENC_B 16
#define ENC_SW 17
void EncoderInterrupt();
uint16_t JOG_MIN_SPEED = 1000; // mm/min (Don't make it too slow).
uint16_t JOG_MAX_SPEED = 8000; // mm/min (Needs to be set from the GRBL eeprom settings $110-$112
time_t _commandIntervalDelay = 100; // mSecs
float _resolution = 10; // Inverse of distance travelled per pulse, for ex, 10 = 1 / 0.1mm/pulse 
uint16_t AXES_ACCEL [3] = { 500, 500, 50 }; // mm/s/s <-- Need to fill these from the GRBL eeprom values $120-$122

// Display
#define TFT_PWR 6
#define TFT_BL 14
#define TFT_RST 8
#define TFT_DC 9
#define TFT_CS 10
#define TFT_MOSI 11
#define TFT_SCLK 13
DMAMEM RAFB tftFB[ILI9488_TFTWIDTH * ILI9488_TFTHEIGHT];
ILI9488_t3 tft(TFT_CS, TFT_DC, TFT_RST);
elapsedMillis displayRefresh;
elapsedMillis heartBeatTimeout;
elapsedMillis laserTimeout;
uint8_t lcdBrightness = 128;
bool forceLCDRedraw = false;
char terminal[tROWS][tCOLS];
char cmdTerminal[cROWS][tCOLS];

void UpdateGrblTerminal();
void UpdateCommandTerminal();
void UpdateSpindleDisplay();
void UpdateFeedRateDisplay();
void UpdateSelectorStates();
void UpdateStatusDisplay();
void UpdateParameterDisplay();
void UpdateLCDBrightnessIndication(bool clear = false);
void Blank();

// GRBL
enum gCodeSenderType { eNoSender, eSelfSender, eExternalSender };
#define GRBL_STATUS_TIMEOUT 1000
#define GRBL_STATUS_SELF_SENDER_TIMEOUT 200
uint16_t grblStatusTimeout(GRBL_STATUS_TIMEOUT);
elapsedMillis grblStatusTimer;
gCodeSenderType gCodeSender = eNoSender;
char statusBuffer[300] = { '\0' };
char serialBuf[tCOLS] = { '\0' };
uint8_t serialBufPtr(0);
char UserialBuf[512] = { '\0' };
uint16_t UserialBufPtr(0);
void SendToGrbl(const char *msg);
// For realtime / single byte commands
void SendToGrbl(uint8_t b);
char grblJogCommand[128] = { '\0' };
int8_t axisProbeDirectionX = 0;
int8_t axisProbeDirectionY = 0;
int8_t axisProbeDirectionZ = 0;

#define CONTROLLER_NORMAL 0
#define CONTROLLER_PROBEX 1
#define CONTROLLER_PROBEY 2
#define CONTROLLER_PROBEZ 3

uint8_t controllerState = CONTROLLER_NORMAL;

// Request bitmasks
#define EEREQ (1 << 0)
#define WSREQ (1 << 1)
#define GCREQ (1 << 2)
#define BDREQ (1 << 3)
#define ALLREQ (EEREQ | WSREQ | GCREQ | BDREQ)
#define SETREQ(REQ, TYPE) (REQ |= TYPE)
#define CLRREQ(REQ, TYPE) (REQ &= (~TYPE))
void RequestGrblStateUpdate(uint8_t type);
void SynchronizeWithGrbl();

#define GRBL_STATUS_ENUM \
ADD_ENUM(eStatus)\
ADD_ENUM(eRPosX)\
ADD_ENUM(eRPosY)\
ADD_ENUM(eRPosZ)\
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



#define SFIELDS 20
grblStateStruct grblState;

char *statusValues[eStatusRange] = { '\0' };
char *statusFields[SFIELDS] = { '\0' };
char *pushMsgValues[ePushRange] = { '\0' };
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
   if(Sel2Pos.k() == DEBUG)
   {
      // Wrap long lines.
      uint8_t breaks((strlen(line) / (tCOLS - 1)) + 1);

      for(uint8_t r = breaks; r < tROWS; ++r)
      {
         strncpy(terminal[r - breaks], terminal[r], tCOLS - 1);
      }
   
      for(uint8_t r = 0; r < breaks; ++r)
      {
         strncpy(terminal[(tROWS - breaks) + r], &line[r * (tCOLS - 1)], tCOLS);
      }
   }
}

void AddLineToCommandTerminal(const char *line)
{
   // Wrap long lines.
   uint8_t breaks((strlen(line) / (tCOLS - 1)) + 1);

   for(uint8_t r = breaks; r < cROWS; ++r)
   {
      strncpy(cmdTerminal[r - breaks], cmdTerminal[r], tCOLS - 1);
   }
   
   for(uint8_t r = 0; r < breaks; ++r)
   {
      strncpy(cmdTerminal[(cROWS - breaks) + r], &line[r * (tCOLS - 1)], tCOLS);
   }
}



void setup()
{
  // Display
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, lcdBrightness);
  tft.begin();
  tft.setRotation(1);
  tft.setFrameBuffer(tftFB);
  tft.useFrameBuffer(true);
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

#ifdef USE_GDB
   Serial.begin(115200);
   debug.begin(Serial);
#else
  while (!Serial && (millis() < 5000)) ; // wait for Arduino Serial Monitor
#endif

  myusb.begin();

   pinMode(SYSTEM, INPUT_PULLUP);
   pinMode(AXIS_X, INPUT_PULLUP);
   pinMode(AXIS_Y, INPUT_PULLUP);
   pinMode(AXIS_Z, INPUT_PULLUP);
   pinMode(SPINDLE, INPUT_PULLUP);
   pinMode(FEEDRATE, INPUT_PULLUP);
   pinMode(LCDBRT, INPUT_PULLUP);
   pinMode(FILES, INPUT_PULLUP);

   pinMode(JOG, INPUT_PULLUP);
   pinMode(XP1, INPUT_PULLUP);
   pinMode(X1, INPUT_PULLUP);
   pinMode(X10, INPUT_PULLUP);
   pinMode(X100, INPUT_PULLUP);
   pinMode(F1, INPUT_PULLUP);
   pinMode(F2, INPUT_PULLUP);
   pinMode(DEBUG, INPUT_PULLUP);

   attachInterrupt(digitalPinToInterrupt(SYSTEM), TestSelector1, CHANGE);
   attachInterrupt(digitalPinToInterrupt(AXIS_X), TestSelector1, CHANGE);
   attachInterrupt(digitalPinToInterrupt(AXIS_Y), TestSelector1, CHANGE);
   attachInterrupt(digitalPinToInterrupt(AXIS_Z), TestSelector1, CHANGE);
   attachInterrupt(digitalPinToInterrupt(SPINDLE), TestSelector1, CHANGE);
   attachInterrupt(digitalPinToInterrupt(FEEDRATE), TestSelector1, CHANGE);
   attachInterrupt(digitalPinToInterrupt(LCDBRT), TestSelector1, CHANGE);
   attachInterrupt(digitalPinToInterrupt(FILES), TestSelector1, CHANGE);

   attachInterrupt(digitalPinToInterrupt(JOG), TestSelector2, CHANGE);
   attachInterrupt(digitalPinToInterrupt(XP1), TestSelector2, CHANGE);
   attachInterrupt(digitalPinToInterrupt(X1), TestSelector2, CHANGE);
   attachInterrupt(digitalPinToInterrupt(X10), TestSelector2, CHANGE);
   attachInterrupt(digitalPinToInterrupt(X100), TestSelector2, CHANGE);
   attachInterrupt(digitalPinToInterrupt(F1), TestSelector2, CHANGE);
   attachInterrupt(digitalPinToInterrupt(F2), TestSelector2, CHANGE);
   attachInterrupt(digitalPinToInterrupt(DEBUG), TestSelector2, CHANGE);

   for(uint8_t s = 0; s < 12; ++s)
   {
      if(selector1Pos[s].k != SEL_NONE && digitalReadFast(selector1Pos[s].k) == LOW)
      {
         Sel1Pos.Now(&selector1Pos[s]);
         Serial.print("sel1 pos:");Serial.println(Sel1Pos.val());
      }
      if(selector2Pos[s].k != SEL_NONE && digitalReadFast(selector2Pos[s].k) == LOW)
      {
         Sel2Pos.Now(&selector2Pos[s]);
         Serial.print("sel2 pos:");Serial.println(Sel2Pos.val());
      }
   }
   UpdateSelectorStates();

  // Encoder pins
  Enc.begin(ENC_A, ENC_B, ENC_SW);
  attachInterrupt(digitalPinToInterrupt(ENC_A), EncoderInterrupt, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), EncoderInterrupt, CHANGE);

  // Button Matrix
  BM.begin(BMCLK, BMSOL, BMQH);
  BM.onChangeConnect(HandleButtonChange);

#ifdef GIT_BRANCH
  AddLineToCommandTerminal(GIT_BRANCH);
  AddLineToCommandTerminal(GIT_DATE);
  AddLineToCommandTerminal(GIT_V);
#endif

#ifdef USE_GDB
   halt();
#endif
  // TODO: Wait for Grbl to connect
  //SynchronizeWithGrbl();
}

void HandleButtonChange(ButtonMatrix::ButtonMasksType button, ButtonMatrix::ButtonStateType state)
{
   if(state == ButtonMatrix::ePressed)
   {
      //Serial.print(", P: ");Serial.print(button);Serial.print(", ");Serial.println(state);
      switch(button)
      {
         case(BTN_SHIFT):
            btnShiftPressed = true;
            break;

         case(BTN_FEEDHOLD):
            break;

         case(BTN_CYCLE_START_RESUME):
            break;

         default:
            if(Sel1Pos.k() == SYSTEM)
            {
               // System Mode
               switch(button)
               {
                  default:
                     break;
               }
            }
            else
            {
               if(Sel2Pos.k() == F1)
               {
                  // F1 mode
                  switch(button)
                  {
                     default:
                        break;
                  }
               }
               else if(Sel2Pos.k() == F2)
               {
                  // F2 Mode
                  switch(button)
                  {
                     default:
                        break;
                  }
               }
               else
               {
                  // Default mode
                  switch(button)
                  {
                     default:
                        break;
                  }
               }
            }

            break;
      }
   }
   else if(state == ButtonMatrix::eReleased)
   {
      switch(button)
      {
         case(BTN_SHIFT):
            btnShiftPressed = false;
            break;

         case(BTN_FEEDHOLD):
            SendToGrbl(GRBL_FEEDHOLD);
            break;

         case(BTN_CYCLE_START_RESUME):
            SendToGrbl(GRBL_CYCLESTARTRESUME);
            break;

         default:
            if(Sel1Pos.k() == SYSTEM)
            {
               // System mode
               switch(button)
               {
                  case(BTN_HOME):
                     SendToGrbl(GRBL_HOMING);
                     break;
                  case(BTN_UNLOCK):
                     if(grblState.state == eIdle || grblState.state == eAlarm)
                     {
                        SendToGrbl(GRBL_UNLOCK);
                     }
                     break;
                  case(BTN_SOFTRESET):
                     SendToGrbl(GRBL_SOFTRESET);
                     break;
                  case(BTN_SLEEP):
                     SendToGrbl(GRBL_SLEEP);
                     break;
                  case(BTN_FORCE_RESYNC):
                     RequestGrblStateUpdate(ALLREQ);
                     break;
                  case(BTN_TOGGLE_LASER_TEST_MODE):
                     if(grblState.laserMode == true)
                     {
                        if(grblState.spindleState == esM5)
                        {
                           SendToGrbl(GRBL_LASER_TESTMODE_ON);
                        }
                        else
                        {
                           SendToGrbl(GRBL_LASER_TESTMODE_OFF);
                        }
                     }
                     break;
                  default:
                     break;
               }
            }
            else
            {
               if(Sel2Pos.k() == F1)
               {
                  // F1 Mode
                  switch(button)
                  {
                     case(BTN_ProbeX):
                     // For the probe commands, it would be nice to let
                     // the user pick the direction of travel before probing,
                     // the distance to probe and the offset to set the
                     // position to.
                        controllerState = CONTROLLER_PROBEX;
                        SendToGrbl(GRBL_PROBE_X);
                        SendToGrbl(GRBL_SET_X_ZERO);
                        break;
                     case(BTN_ProbeY):
                        controllerState = CONTROLLER_PROBEY;
                        SendToGrbl(GRBL_PROBE_Y);
                        SendToGrbl(GRBL_SET_Y_ZERO);
                        break;
                     case(BTN_ProbeZ):
                        controllerState = CONTROLLER_PROBEZ;
                        SendToGrbl(GRBL_PROBE_Z);
                        SendToGrbl(GRBL_SET_Z_ZERO);
                        break;

                     case(BTN_G54):
                        SendToGrbl("G54");
                        break;
                     case(BTN_G55):
                        SendToGrbl("G55");
                        break;
                     case(BTN_G56):
                        SendToGrbl("G56");
                        break;
                     case(BTN_G57):
                        SendToGrbl("G57");
                        break;
                     case(BTN_G58):
                        SendToGrbl("G58");
                        break;
                     case(BTN_G59):
                        SendToGrbl("G59");
                        break;

                     default:
                        break;
                  }
               }
               else if(Sel2Pos.k() == F2)
               {
                  // F2 Mode
               }
               else
               {
                  // Default Mode
                  switch(button)
                  {            
                     case(BTN_SetX0):
                        SendToGrbl(GRBL_SET_X_ZERO);
                        break;
                     case(BTN_SetY0):
                        SendToGrbl(GRBL_SET_Y_ZERO);
                        break;
                     case(BTN_SetZ0):
                        SendToGrbl(GRBL_SET_Z_ZERO);
                        break;

                     case(BTN_GotoX0):
                        if(grblState.isHomed == true)
                        {
                           grblState.cmdTravel[0] = grblState.WCO[0];
                        }
                        SendToGrbl(GRBL_JOG_TO_X0);
                        break;
                     case(BTN_GotoY0):
                        if(grblState.isHomed == true)
                        {
                           grblState.cmdTravel[1] = grblState.WCO[1];
                        }
                        SendToGrbl(GRBL_JOG_TO_Y0);
                        break;
                     case(BTN_GotoZ0):
                        if(grblState.isHomed == true)
                        {
                           grblState.cmdTravel[2] = grblState.WCO[2];
                        }
                        SendToGrbl(GRBL_JOG_TO_Z0);
                        break;
                     case(BTN_GotoX0Y0):
                        SendToGrbl(GRBL_JOG_TO_X0Y0);
                        break;

                     case(BTN_M3M5):
                        if(grblState.spindleState == esM5)
                        {
                           SendToGrbl("M3");
                        }
                        else
                        {
                           SendToGrbl("M5");
                        }
                        break;

                     case(BTN_LaserMode):
                        // Toggle laser mode
                        SendToGrbl("G4P0");
                        if(grblState.laserMode == false)
                        {
                           SendToGrbl(GRBL_LASER_MODE_ON);
                        }
                        else
                        {
                           SendToGrbl(GRBL_LASER_MODE_OFF);
                        }

                        RequestGrblStateUpdate(EEREQ);
                        break;
                     default:
                        break;
                  }
               }
               
               
            }

            break;
      }
   }
}

void EncoderInterrupt()
{
   Enc.HandleInterrupt();
}

void TestSelector1()
{
   Sel1Pos.Reset(emptySel.k);
   Sel1Pos.Interrupt = true;
   for(uint8_t s = 0; s < 12; ++s)
   {
      if(selector1Pos[s].k != SEL_NONE && digitalReadFast(selector1Pos[s].k) == LOW)
      {
         Sel1Pos.Reset(s);
         break;
      }
   }
   // Reset debounce timer
   Sel1Pos.Debounce = millis();
}

void TestSelector2()
{
   Sel2Pos.Reset(emptySel.k);
   Sel2Pos.Interrupt = true;
   for(uint8_t s = 0; s < 12; ++s)
   {
      if(selector2Pos[s].k != SEL_NONE && digitalReadFast(selector2Pos[s].k) == LOW)
      {
         Sel2Pos.Reset(s);
         break;
      }
   }
   // Reset debounce timer
   Sel2Pos.Debounce = millis();
}

float GetMachinePosition(char axis)
{
   float pos(0.f);
   if(axis == 'X')
   {
      pos = grblState.positionIsMPos ? grblState.RPOS[0] : grblState.RPOS[0] + grblState.WCO[0];
   }
   else if(axis == 'Y')
   {
      pos = grblState.positionIsMPos ? grblState.RPOS[1] : grblState.RPOS[1] + grblState.WCO[1];
   }
   else if(axis == 'Z')
   {
      pos = grblState.positionIsMPos ? grblState.RPOS[2] : grblState.RPOS[2] + grblState.WCO[2];
   }
   return(pos);
}

float LimitTravelRequest(float s, char XYZ)
{
   // If the machine has been homed then protect the travel, else
   // allow any input to allow motion after unlocking GRBL following
   // an alarm state.

   // Available travel distance within limits.
   if(grblState.isHomed == true)
   {
      //char buf[20] = { '\0' };
      //char *x = "1";
      uint8_t a(XYZ == 'X' ? 0 : XYZ == 'Y' ? 1 : 2);

      // TODO: Currently assumes that the machine is working in negative machine coordinate space, as
      // the convention requires. Eventually add support for positive space by looking at Grbl's state
      // from the OPT string (see note in ParseBuildOptions()).
      // if(negative space)
      //{
         if(s > 0)// && (grblState.cmdTravel[a] + s) > 0)
         {
            if((a == 0 || a == 1) && (grblState.cmdTravel[a] + s) > 0)
            {
               // On the X-Carve, X and Y don't have limit switches at the
               // 0,0 machine position when operating in negative space.
               s = grblState.state == eIdle ? 0 - grblState.cmdTravel[a] : 0;
            }
            else if((grblState.cmdTravel[a] + s) > grblState.zAxisTopLimit)
            {
               // On the X-Carve the Z axis max travel to the top is where the pull-off position
               // lies after the homing cycle completes. Letting it go to 0 will trigger the
               // limit switch.
               s = grblState.state == eIdle ? grblState.zAxisTopLimit - grblState.cmdTravel[a] : 0;
            }
            //x = "2";
         }
         else if(s < 0 && (grblState.cmdTravel[a] + s) < -grblState.axisMaxTravel[a])
         {
            s = grblState.state == eIdle ? -grblState.axisMaxTravel[a] - grblState.cmdTravel[a] : 0;
            //x = "3";
         }
      //}
      //else
      //{
         // Positive space...
         //if(s > 0 && (grblState.cmdTravel[a] + s) > grblState.axisMaxTravel[a])
         //    s = grblState.state == eIdle ? grblState.axisMaxTravel[a] - grblState.cmdTravel[a] : 0;
         //    //x = "2";
         // }
         // else if(s < 0 && (grblState.cmdTravel[a] + s) < 0)
         // {
         //    s = grblState.state == eIdle ? grblState.cmdTravel[a] : 0;
         //    //x = "3";
         // }
      //}

      // Track the amount of distance travel requested
      grblState.cmdTravel[a] += s;
      //sprintf(buf, "%.1f, %.1f, %.1f, %s, %d", grblState.cmdTravel[a], s, grblState.axisMaxTravel[a], x, grblState.state);
      //AddLineToCommandTerminal(buf);
      return(s);
   }
   return(s);
}

uint16_t interpolateFeedRateFromStepTimeDelta(float x, float in_min, float in_max, uint16_t out_min, uint16_t out_max)
{
  return (in_min - x) * (out_max - out_min) / (in_min - in_max) + out_min;
}

void Jog_WheelSpeedVsFeedRate(time_t delta, int8_t dir, float &f, float &s)
{
   delta = delta < 10 ? 10 : delta;
   f = interpolateFeedRateFromStepTimeDelta(delta, 100.0, 10.0, JOG_MIN_SPEED, JOG_MAX_SPEED);
   // Convert feed rate from mm/min to mm/sec
   float v(f / 60.0);
   // Update AXES_ACCEL so it uses the value for the
   // selected axis.
   float dt = ((v * v) / (2.0 * AXES_ACCEL[0] * 14));
   //Serial.print("dt: ");Serial.println(dt);
   s = (v * dt) * dir;
   s /= 10;
}

//void Jog_StepsPerRevolution(time_t delta, int8_t dir, float &f, float &s)
//{
//   // UNTESTED. Left here for possible future development.
//  if(delta > _commandIntervalDelay)
//  {
//      s = _EncSteps / _resolution;
//  }
//  f = 500; // Need to make this more dynamic
//}


// This is where the magic happens. The encoder wheel
// pulses are translated into motion commands for GRBL.
//
// DO NOT USE THE println() method when sending
// data to GRBL. It injects both the \r and \n
// bytes to the line end and it causes GRBL to
// interpret this as 2 separate lines, causing
// extra overhead. Instead add the \n byte to
// the end of the formatted string and send it
// like so.

void ProcessEncoderRotation(int8_t steps)
{
   // Trick the code into thinking grbl is connected.
   // Used when testing this block of code by itself.
   //grblState.state = eJog;

   time_t uSecs(Enc.GetuSecs());
   time_t olduSecs(Enc.GetOlduSecs());
   // 
   if(grblState.jogging == true)
   {
      if(millis() - uSecs > 200)
      {
         grblState.jogging = false;
         // Send from here, no delay.
         userial.write(0x85);
         //userial.println("G4P0");
         //Serial.println("STOP");
      }
   }
   if((grblState.state == eJog || grblState.state == eIdle) && steps != 0 &&
      (Sel1Pos.k() == AXIS_X || Sel1Pos.k() == AXIS_Y || Sel1Pos.k() == AXIS_Z))
   {
      int8_t dir(steps > 0 ? 1 : -1);
      const char *XYZ(Sel1Pos.k() == AXIS_X ? "X" : Sel1Pos.k() == AXIS_Y ? "Y" : "Z");
      float s(0); // Travel distance (mm)
      float f(0); // Feed rate (mm / min)
      if(Sel2Pos.k() == JOG)
      {
         time_t delta(uSecs - olduSecs);
         if(delta > 100)
         {
            // Slow commands in jog mode that
            // are treated as fixed step inputs of
            // a short distance. Allows precise
            // and repeatable motion at small scale.
            s = 0.1 * dir;
            f = 1000;
         }
         else
         {
            grblState.jogging = true;
            
            Jog_WheelSpeedVsFeedRate(delta, dir, f, s);
            //Jog_StepsPerRevolution(delta, dir, f, s);
         }
         
         // Limit travel
         //s = LimitTravelRequest(s, XYZ[0]);
         //sprintf(grblJogCommand, "$J=G91F%.3f%s%.3f", f, XYZ, s);
         //SendToGrbl(grblJogCommand);
      }
      else if(Sel2Pos.k() == XP1)
      {
         s = 0.1 * dir;
         f = 1000.0;
         //sprintf(grblJogCommand, "$J=G91F1000%s%.3f", XYZ, 0.1 * dir);
         //SendToGrbl(grblJogCommand);
      }
      else if(Sel2Pos.k() == X1)
      {
         s = dir;
         f = 1000.0;
         //sprintf(grblJogCommand, "$J=G91F1000%s%d", XYZ, dir);
         //SendToGrbl(grblJogCommand);
      }
      else if(Sel2Pos.k() == X10)
      {
         s = 10 * dir;
         f = 3000.0;
         //sprintf(grblJogCommand, "$J=G91F3000%s%d", XYZ, 10 * dir);
         //SendToGrbl(grblJogCommand);
      }
      else if(Sel2Pos.k() == X100 && Sel1Pos.k() != AXIS_Z)// _EncSteps != 0)
      {
         // 100 is too big for the Z axis. Allow this only for X and Y.
         s = 100 * dir;
         f = 5000.0;
         //sprintf(grblJogCommand, "$J=G91F5000%s%d", XYZ, 100 * dir);
         //SendToGrbl(grblJogCommand);
      }

      if(f > 0)
      {
         s = LimitTravelRequest(s, XYZ[0]);
         sprintf(grblJogCommand, "$J=G91F%f%s%f", f, XYZ, s);
         SendToGrbl(grblJogCommand);
      }
   }
   else if(Sel1Pos.k() == SPINDLE && steps != 0 && grblState.maxSpindleSpeed != 0 &&
           (Sel2Pos.k() == X1 || Sel2Pos.k() == X10 || Sel2Pos.k() == X100))
   {
      int16_t cmd(steps);
      if(Sel2Pos.k() == X10)
      { 
         cmd *= 10;
      }
      else if(Sel2Pos.k() == X100)
      { 
         cmd *= 100;
      }
      grblState.cmdSpindleSpeed += cmd;
      if(grblState.cmdSpindleSpeed > grblState.maxSpindleSpeed)
      {
         grblState.cmdSpindleSpeed = grblState.maxSpindleSpeed;
      }
      else if(grblState.cmdSpindleSpeed < 0)
      {
         grblState.cmdSpindleSpeed = 0;
      }
      UpdateSpindleDisplay();
   }
   else if(Sel1Pos.k() == FEEDRATE && steps != 0 &&
           (Sel2Pos.k() == X1 || Sel2Pos.k() == X10 || Sel2Pos.k() == X100))
   {
      int16_t cmd(steps);
      if(Sel2Pos.k() == X10)
      { 
         cmd *= 10;
      }
      else if(Sel2Pos.k() == X100)
      { 
         cmd *= 100;
      }
      grblState.cmdFeedRate += cmd;
      if(grblState.cmdFeedRate > grblState.axisMaxRate[0])
      {
         grblState.cmdFeedRate = grblState.axisMaxRate[0];
      }
      else if(grblState.cmdFeedRate < 0)
      {
         grblState.cmdFeedRate = 0;
      }
      UpdateFeedRateDisplay();
   }
   else if(Sel1Pos.k() == LCDBRT && steps != 0 &&
           (Sel2Pos.k() == X1 || Sel2Pos.k() == X10 || Sel2Pos.k() == X100))
   {
      int16_t cmd(lcdBrightness);
      if(Sel2Pos.k() == X10)
      {
         steps *= 10;
      }
      else if(Sel2Pos.k() == X100)
      {
         steps *= 100;
      }

      cmd += steps;

      lcdBrightness = (cmd > 255 ? 255 : cmd < 0 ? 0 : cmd);
      analogWrite(TFT_BL, lcdBrightness);

      UpdateLCDBrightnessIndication();
      tft.updateScreenAsync();
   }
   else if(Sel1Pos.k() == FILES && steps != 0)
   {
      GC.Select(steps);
      tft.updateScreenAsync();
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
            grblState.state = eUnknownState;
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
   grblState.state = eConnected;
   if(line != NULL)
   {
      AddLineToGrblTerminal(line);
   }
   RequestGrblStateUpdate(ALLREQ);
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

      //<Idle|MPos:0.000,0.000,0.000|FS:0.0,0|WCO:13.0,6.0,2.0>            
      for(uint8_t i = 0; i < f; ++i)
      {
         if(strstr(statusFields[i], "MPos") != NULL)
         {
            char *ch = strtok(statusFields[i], ":,");
            
            ch = strtok(NULL, ":,");
            statusValues[eRPosX] = ch;
            grblState.RPOS[0] = atof(statusValues[eRPosX]);
            ch = strtok(NULL, ":,");
            statusValues[eRPosY] = ch;
            grblState.RPOS[1] = atof(statusValues[eRPosY]);
            ch = strtok(NULL, ":,");
            statusValues[eRPosZ] = ch;
            grblState.RPOS[2] = atof(statusValues[eRPosZ]);
            grblState.positionIsMPos = true;
         }
         else if(strstr(statusFields[i], "WPos") != NULL)
         {
            char *ch = strtok(statusFields[i], ":,");
            
            ch = strtok(NULL, ":,");
            statusValues[eRPosX] = ch;
            grblState.RPOS[0] = atof(statusValues[eRPosX]);
            ch = strtok(NULL, ":,");
            statusValues[eRPosY] = ch;
            grblState.RPOS[1] = atof(statusValues[eRPosY]);
            ch = strtok(NULL, ":,");
            statusValues[eRPosZ] = ch;
            grblState.RPOS[2] = atof(statusValues[eRPosZ]);
            grblState.positionIsMPos = false;
         }
         else if(strstr(statusFields[i], "F") != NULL)
         {
            char *ch = strtok(statusFields[i], ":,");
            ch = strtok(NULL, ":,");
            grblState.currentFeedRate = atof(ch);
            statusValues[eFeed] = ch;
            
            if(strstr(statusFields[i], "FS") != NULL)
            {
               ch = strtok(NULL, ":,");
               grblState.currentSpindleSpeed = atof(ch);
               statusValues[eSpindle] = ch;
            }
         }
         else if(strstr(statusFields[i], "Bf") != NULL)
         {
            char *ch = strtok(statusFields[i], ":,");
            
            ch = strtok(NULL, ":,");
            grblState.plannerBlocksAvailable = atoi(ch);
            statusValues[ePlanner] = ch;
            ch = strtok(NULL, ":,");
            grblState.rxBufferBytesAvailable = atoi(ch);
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
            grblState.feedOverride = atoi(ch);
            statusValues[eOVFeed] = ch;
            ch = strtok(NULL, ":,");
            grblState.rapidOverride = atoi(ch);
            statusValues[eOVRapid] = ch;
            ch = strtok(NULL, ":,");
            grblState.spindleOverride = atoi(ch);
            statusValues[eOVSpindle] = ch;
         }
         else if(strstr(statusFields[i], "WCO") != NULL)
         {
            char *ch = strtok(statusFields[i], ":,");

            ch = strtok(NULL, ":,");
            statusValues[eWCOX] = ch;
            grblState.WCO[0] = atof(statusValues[eWCOX]);
            ch = strtok(NULL, ":,");
            statusValues[eWCOY] = ch;
            grblState.WCO[1] = atof(statusValues[eWCOY]);
            ch = strtok(NULL, ":,");
            statusValues[eWCOZ] = ch;
            grblState.WCO[2] = atof(statusValues[eWCOZ]);
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
      if(gCodeSender == eExternalSender)
      {
         forceLCDRedraw = true;
      }
   }

   return(p);
}

void ParseGCodeParserState(char *line)
{
   // [GC:G0 G54 G17 G21 G90 G94 M0 M5 M9 T0 F500.0 S0.0]
   // Find the end of the header (:)
   char *ch(strtok(line, ":"));
   ch = strtok(NULL, " ");
   if(ch[1] == '0')
   {
      grblState.motionMode = emG0;
   }
   else if(ch[1] == '1')
   {
      grblState.motionMode = emG1;
   }
   else if(ch[1] == '2')
   {
      grblState.motionMode = emG2;
   }
   else if(ch[1] == '3')
   {
      if(ch[2] == '\0')
      {
         grblState.motionMode = emG3;
      }
      else
      {
         if(ch[4] == '2')
         {
            grblState.motionMode = emG38_2;
         }
         else if(ch[4] == '3')
         {
            grblState.motionMode = emG38_3;
         }
         else if(ch[4] == '4')
         {
            grblState.motionMode = emG38_4;
         }
         else if(ch[4] == '5')
         {
            grblState.motionMode = emG38_5;
         }
      }
   }
   else if(ch[1] == '8')
   {
      grblState.motionMode = emG80;
   }

   // Find the active workspace
   ch = strtok(NULL, " ");
   for(uint8_t s = 0; s < ecRange; ++s)
   {
      if(strncmp(ch, grblState.workSpace[s].id, 3) == 0)
      {
         grblState.activeWorkspace = &grblState.workSpace[s];
         break;
      }
   }

   // Plane
   ch = strtok(NULL, " ");
   //Serial.print("P:");Serial.println(ch);
   // Units
   ch = strtok(NULL, " ");
   //Serial.print("U:");Serial.println(ch);
   // Distance mode
   ch = strtok(NULL, " ");
   //Serial.print("D:");Serial.println(ch);
   // Feed rate mode
   ch = strtok(NULL, " ");
   //Serial.print("F:");Serial.println(ch);
   // Program mode -- WHY NO MODE???
   //ch = strtok(NULL, " ");
   //Serial.print("R:");Serial.println(ch);
   // Spindle state
   ch = strtok(NULL, " ");
   //Serial.print("M:");Serial.println(ch);
   if(strncmp(ch, "M3", 2) == 0)
   {
      grblState.spindleState = esM3;
   }
   else if(strncmp(ch, "M4", 2) == 0)
   {
      grblState.spindleState = esM4;
   }
   else if(strncmp(ch, "M5", 2) == 0)
   {
      grblState.spindleState = esM5;
   }
   // Coolant state
   ch = strtok(NULL, " ");
   //Serial.print("C:");Serial.println(ch);
   // Tool number
   ch = strtok(NULL, " ");
   //Serial.print("T:");Serial.println(ch);
   // Feed rate
   ch = strtok(NULL, " ");
   grblState.programmedFeedRate = atof(&ch[1]);
   UpdateFeedRateDisplay();
   //Serial.print("f:");Serial.println(ch);
   // Spindle speed
   ch = strtok(NULL, "]");
   //Serial.print("ss:");Serial.print(ch);Serial.print(":");Serial.println(&ch[1]);
   grblState.programmedSpindleSpeed = atof(&ch[1]);
   UpdateSpindleDisplay();

   grblState.requestGCUpdate = 3;
   UpdateParameterDisplay();
}

void ParseGCodeParameters(char *line)
{
   // The line coming in here must not have
   // the opening brace removed. Otherwise
   // it will be removed here.
   if(line[0] == '[')
   {
      line = &line[1];
   }
   //[G54:4.000,0.000,0.000]
   //[G55:4.000,6.000,7.000]
   //[G56:0.000,0.000,0.000]
   //[G57:0.000,0.000,0.000]
   //[G58:0.000,0.000,0.000]
   //[G59:0.000,0.000,0.000]
   //[G28:1.000,2.000,0.000]
   //[G30:4.000,6.000,0.000]
   //[G92:0.000,0.000,0.000]
   //[TLO:0.000]
   //[PRB:0.000,0.000,0.000:0]

   char *ch(strtok(line, ":"));
   if(ch[0] == 'T')
   {
      ch = strtok(NULL, "]");
      grblState.TLO = atof(ch);
   }
   else
   {
      idP3t *p(nullptr);
      if(ch[0] == 'P')
      {
         p = &grblState.probe;
         strncpy(p->id, ch, sizeof(p->id));
         grblState.requestWSUpdate = 3;
      }
      else if(ch[1] == '5')
      {
         uint8_t n(ch[2] - '0');
         p = &grblState.workSpace[n - 4];
         strncpy(p->id, ch, sizeof(p->id));
      }
      else if(ch[1] == '2')
      {
         p = &grblState.park28;
         strncpy(p->id, ch, sizeof(p->id));
      }
      else if(ch[1] == '3')
      {
         p = &grblState.park30;
         strncpy(p->id, ch, sizeof(p->id));
      }
      else if(ch[1] == '9')
      {
         p = &grblState.offset92;
         strncpy(p->id, ch, sizeof(p->id));
      }

      ch = strtok(NULL, ",");
      p->val3[0] = atof(ch);
      ch = strtok(NULL, ",");
      p->val3[1] = atof(ch);
      ch = strtok(NULL, "]");
      p->val3[2] = atof(ch);
   }
}

void ParseBuildOptions(char *line)
{  // Build options are requested with $I
   // [VER:1.1h.20190830:TEST]
   // [OPT:V,15,128]
   char *ch(strtok(line, ":"));

   if(ch[1] == 'V')
   {
      // Parse version data
   }
   else if(ch[1] == 'O')
   {
      // Skip the build options.
      ch = strtok(NULL, ",");
      // TODO: Extract the build options and find configuration settings such as:
      // 1- If 'Z' is present, then HOMING_FORCE_SET_ORIGIN is set and machine operates in positive space (this is not the convention default, conventions is to operate in negative space).
      // 2- If 'V' is present, then VARIABLE_SPINDLE support is available.
      // Planner block count
      ch = strtok(NULL, ",");
      grblState.plannerBlockCount = atoi(ch);
      // rx Buffer size
      ch = strtok(NULL, "]");
      grblState.rxBufferSize = atoi(ch);

      grblState.requestBDUpdate = 3;
   }
}

void ParseBracketMessage(char *line)
{
   AddLineToGrblTerminal(line);
   
   if(strncmp(line, "[GC:", 4) == 0)
   {
      ParseGCodeParserState(line);
   }
   else if(strncmp(line, "[G", 2) == 0 ||
           strncmp(line, "[TLO:", 5) == 0 ||
           strncmp(line, "[PRB:", 5) == 0)
   {
      // Skip the opening bracket
      ParseGCodeParameters(&line[1]);
   }
   else if(strncmp(line, "[OPT:", 5) == 0 ||
           strncmp(line, "[VER:", 5) == 0)
   {
      ParseBuildOptions(line);
   }
}

void ParseEEPROMData(char *line)
{
   AddLineToGrblTerminal(line);

   // Capture values we need.
   char *ch(strtok(line, "="));
   char *val = strtok(NULL, "");

   if(strncmp(ch, "$30", 3) == 0)
   {
      grblState.maxSpindleSpeed = atof(val);
      char msg[tCOLS] = { '\0' };
      sprintf(msg, "Max Spindle Speed: %.1f", grblState.maxSpindleSpeed);
      AddLineToCommandTerminal(msg);
   }
   else if(strncmp(ch, "$32", 3) == 0)
   {
      grblState.laserMode = atoi(val) == 0 ? false : true;
      char msg[tCOLS] = { '\0' };
      sprintf(msg, "Laser Mode: %s", grblState.laserMode ? "On" : "Off");;
      AddLineToCommandTerminal(msg);
   }
   else if(strncmp(ch, "$110", 4) == 0)
   {
      grblState.axisMaxRate[0] = atof(val);
   }
   else if(strncmp(ch, "$111", 4) == 0)
   {
      grblState.axisMaxRate[1] = atof(val);
   }
   else if(strncmp(ch, "$112", 4) == 0)
   {
      grblState.axisMaxRate[2] = atof(val);
   }
   else if(strncmp(ch, "$120", 4) == 0)
   {
      grblState.axisMaxAccel[0] = atof(val);
   }
   else if(strncmp(ch, "$121", 4) == 0)
   {
      grblState.axisMaxAccel[1] = atof(val);
   }
   else if(strncmp(ch, "$122", 4) == 0)
   {
      grblState.axisMaxAccel[2] = atof(val);
   }
   else if(strncmp(ch, "$130", 4) == 0)
   {
      grblState.axisMaxTravel[0] = atof(val);
   }
   else if(strncmp(ch, "$131", 4) == 0)
   {
      grblState.axisMaxTravel[1] = atof(val);
   }
   else if(strncmp(ch, "$132", 4) == 0)
   {
      grblState.axisMaxTravel[2] = atof(val);
      grblState.requestEEUpdate = 3;
      UpdateCommandTerminal();
   }
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

      if(redraw == true && Sel2Pos.k() == DEBUG)
      {
         UpdateGrblTerminal();
      }
      return(q);
   }
   return(0);
}

void RefreshScreen()
{
   Blank();
   UpdateGrblTerminal();
   UpdateCommandTerminal();
   UpdateStatusDisplay();
   UpdateSelectorStates();
   UpdateParameterDisplay();

   if(Sel1Pos.k() == LCDBRT)
   {
      UpdateLCDBrightnessIndication();
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

void UpdateLCDBrightnessIndication(bool clear)
{
   tft.fillRect(330, 144, 40, 16, ILI9488_BLACK);
   if(clear == false)
   {
      tft.setTextColor(ILI9488_LIGHTGREY, ILI9488_BLACK);
      tft.setTextSize(2);
      tft.setCursor(330, 144);
      tft.print(lcdBrightness);
   }
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

   if(Sel1Pos.k() == SYSTEM)
   {
      tft.setTextColor(ILI9488_BLACK, ILI9488_RED);
   }
   Text(x, y, Sel1Pos.val());

   tft.setTextColor(ILI9488_WHITE, ILI9488_BLACK);

   x = 415;
   y = 160;
   tft.drawRect(x, y, tft.width() - x, 20, ILI9488_PURPLE);
   x = 420;
   y = 162;
   Text(x, y, Sel2Pos.val());
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

void UpdateGrblTerminal()
{
   if(Sel2Pos.k() == DEBUG)
   {
      tft.setTextSize(1);

      int16_t x, y;
      uint16_t cw, ch;
      int8_t count = tROWS;
      int8_t r = count - 1;

      tft.getTextBounds("0", 0, 0, &x, &y, &cw, &ch);
      //tft.fillRect(0, 0, cw * tCOLS - 1, tft.height(), ILI9488_OLIVE);
      tft.fillRect(0, 48, cw * tCOLS - 1, tft.height() - 48, ILI9488_BLACK);
      tft.drawRect(0, 48, cw * tCOLS - 1, tft.height() - 48, ILI9488_OLIVE);

      while(r >= 0)
      {
         if(terminal[r] != NULL)
         {
            if(strcmp(terminal[r], "ok") == 0)
            {
               tft.setTextColor(ILI9488_BLACK, ILI9488_GREEN);
            }
            else if(strstr(terminal[r], "error") != NULL)
            {
               tft.setTextColor(ILI9488_BLACK, ILI9488_RED);
               if(strlen(terminal[r]) > 6)
               {
                  uint8_t e(atoi(&terminal[r][6]));
                  const char *ec(grblErrorDesc[e - 1]);
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
            Text(0, tft.height() - ((count - r) * ch), terminal[r]);
         }
         --r;
      }
   }
}

void UpdateCommandTerminal()
{
   tft.setTextSize(1);

   int16_t x(00), y(192);
   uint16_t cw, ch;
   uint8_t rmax((tft.height() - y) / 8);
   uint8_t rend(cROWS - rmax);

   tft.getTextBounds("0", 0, 0, &x, &y, &cw, &ch);
   tft.fillRect(300, 192, cw * tCOLS - 1, tft.height() - 192, ILI9488_BLACK);
   tft.drawRect(300, 192, cw * tCOLS - 1, tft.height() - 192, ILI9488_OLIVE);
   int8_t r = cROWS - 1;
   y = (tft.height() - ch);
   tft.setTextColor(ILI9488_WHITE, ILI9488_BLACK);
   while(r >= 0)
   {
      if(cmdTerminal[r] != NULL && r >= rend)
      {
         Text(300, y, cmdTerminal[r]);
      }
      --r;
      y -= ch;
   }
}

void ProcessLineFromGrbl()
{
   if(strncasecmp(UserialBuf, "ok", 2) == 0)
   {
      GC.Acknowledge();
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

void SendToGrbl(uint8_t b)
{
   // Single byte commands go out this way.
   userial.write(b);
   //AddLineToCommandTerminal(b);
   //UpdateCommandTerminal();
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

   if(strstr(msg, "G54") != NULL ||
      strstr(msg, "G55") != NULL ||
      strstr(msg, "G56") != NULL ||
      strstr(msg, "G57") != NULL ||
      strstr(msg, "G58") != NULL ||
      strstr(msg, "G59") != NULL)
   {
      // Force an update of the active workspace.
      RequestGrblStateUpdate(GCREQ | WSREQ);
   }
   else if(strstr(msg, "M3") != NULL ||
           strstr(msg, "M4") != NULL ||
           strstr(msg, "M5") != NULL)
   {
      RequestGrblStateUpdate(GCREQ);
   }
   AddLineToCommandTerminal(msg);
   UpdateCommandTerminal();
}

void RequestGrblStateUpdate(uint8_t type)
{
   grblState.synchronize = type != 0;
   grblState.requestEEUpdate = 3;
   grblState.requestWSUpdate = 3;
   grblState.requestGCUpdate = 3;
   grblState.requestBDUpdate = 3;
   
   if(type & EEREQ)
   {
      grblState.requestEEUpdate = 1;
   }
   if(type & WSREQ)
   {
      grblState.requestWSUpdate = 1;
   }
   if(type & GCREQ)
   {
      grblState.requestGCUpdate = 1;
   }
   if(type & BDREQ)
   {
      grblState.requestBDUpdate = 1;
   }
}

void SynchronizeWithGrbl()
{
   if(grblState.synchronize == true)
   {
      // Request current state and settings from Grbl.
      if(grblState.requestEEUpdate == 1)
      {
         SendToGrbl(GRBL_VIEWSETTINGS);
         grblState.requestEEUpdate = 2;
      }
      else if(grblState.requestEEUpdate == 3 && grblState.requestWSUpdate == 1)
      {
         // Call parameters first...
         SendToGrbl(GRBL_PARAMETERS);
         grblState.requestWSUpdate = 2;
      }
      else if(grblState.requestWSUpdate == 3 && grblState.requestGCUpdate == 1)
      {
         // ... then get the parser
         SendToGrbl(GRBL_GCODEPARSER);
         grblState.requestGCUpdate = 2;
      }
      else if(grblState.requestGCUpdate == 3 && grblState.requestBDUpdate == 1)
      {
         // Get the buffer sizes.
         SendToGrbl(GRBL_BUILDINFO);
         grblState.requestBDUpdate = 2;
      }
      else if(grblState.requestBDUpdate == 3 && grblState.requestEEUpdate == 3 &&
              grblState.requestGCUpdate == 3 && grblState.requestWSUpdate == 3)
      {
         // Request a status report.
         userial.write(GRBL_STATUSREPORT);

         grblState.synchronize = false;
         grblState.requestEEUpdate = 0;
         grblState.requestWSUpdate = 0;
         grblState.requestGCUpdate = 0;
         grblState.requestBDUpdate = 0;
      }
   }
}

void loop()
{
   // If necessary, synchronize the GRBL state.
   SynchronizeWithGrbl();
   // Update the USB state machine.
   myusb.Task();
   MonitorUSBDevices();

   ///////////////////////////////////////////////////////////
   // Check all user inputs first to establish a desired state.
   ///////////////////////////////////////////////////////////
   // Check selectors
   if(Sel1Pos.Interrupt == true)
   {
      // Interrupt received.
      if(millis() - Sel1Pos.Debounce > 20)
      {
         // Accept change
         if(Sel1Pos.Position == SEL_NONE)
         {
            Sel1Pos.Now(&emptySel);
         }
         else
         {
            Sel1Pos.Now(&selector1Pos[Sel1Pos.Position]);
         }
      }
   }

   if(Sel2Pos.Interrupt == true)
   {
      // Interrupt received.
      if(millis() - Sel2Pos.Debounce > 20)
      {
         // Accept change
         if(Sel2Pos.Position == SEL_NONE)
         {
            Sel2Pos.Now(&emptySel);
         }
         else
         {
            Sel2Pos.Now(&selector2Pos[Sel2Pos.Position]);
         }
      }
   }
   // Check button matrix.
   BM.Update();
   // Check encoder.
   ProcessEncoderRotation(Enc.Update());
   ///////////////////////////////////////////////////////////

   ///////////////////////////////////////////////////////////
   // Once the inputs are checked, use them to update the
   // system state.
   if(gCodeSender != eExternalSender && displayRefresh >= 500)
   {
       displayRefresh -= 500;
       UpdateClockDisplay();
       tft.updateScreenAsync();
   }
   else if(forceLCDRedraw == true)
   {
      forceLCDRedraw = false;
       UpdateClockDisplay();
       tft.updateScreenAsync();
   }

   if(Sel1Pos.ChangeInFrame() == true || Sel2Pos.ChangeInFrame() == true)
   {
      UpdateSelectorStates();
      if(Sel1Pos.ChangeInFrame())
      {
         // Handle sel1 changes if required.
         if(Sel1Pos.p() == FILES || Sel1Pos.k() == FILES)
         {
            GC.HandleSelectorStateChange(Sel1Pos.k(), Sel1Pos.p());
         }

         if(Sel1Pos.p() == LCDBRT || Sel1Pos.k() == LCDBRT)
         {
            UpdateLCDBrightnessIndication(Sel1Pos.p() == LCDBRT);
         }
      }
      else
      {
         // Handle sel2 changes if required.
      }
   }

   // Update the sender state.
   GC.Update();
   if(GC.State() == Streamer::STREAM && (grblState.state == eRun || grblState.state == eJog))
   {
      if(gCodeSender != eSelfSender)
      {
         gCodeSender = eSelfSender;
      }
      grblStatusTimeout = GRBL_STATUS_SELF_SENDER_TIMEOUT;
   }
   else
   {
      gCodeSender = eNoSender;
      grblStatusTimeout = GRBL_STATUS_TIMEOUT;
   }

   // Request an update of the status in GRBL
  if(gCodeSender != eExternalSender && grblStatusTimer >= grblStatusTimeout)
  {
     grblStatusTimer -= grblStatusTimeout;
     // Get a status update
     userial.write('?');
  }
  else if(gCodeSender != eNoSender && grblStatusTimer >= (grblStatusTimeout * 5))
  {
     // Maybe disconnected?
     gCodeSender = eNoSender;
     grblStatusTimeout = GRBL_STATUS_TIMEOUT;
  }

  // Pass the serial data to GRBL. Check if it is comming
   // from the external sender or the local file system.
   //while(Serial.available())
   while (true)
   {
      uint8_t c('\0');
      if(gCodeSender == eSelfSender)
      {
         if(GC.Available() > 0)
         {
            c = GC.Read();
         }
         else
         {
            break;
         }
         //if(c == '\n')
         //{
         //   Serial.print('\r');
         //}
         //Serial.print((char)c);
      }
      else
      {
         if(Serial.available())
         {
            c = Serial.read();
         }
         else
         {
            break;
         }
      }
      // Forward from computer to GRBL
      userial.write(c);

      // This was not sent by us.
      if(c == '?' && gCodeSender != eSelfSender)
      {
         gCodeSender = eExternalSender;
         grblStatusTimer = 0;
      }
      else if (Sel2Pos.k() == DEBUG)
      {
         // This consumes a fair amount of CPU time
         // so ONLY use it to debug the communication
         // between the sender and this controller.
         // NEVER use it when running a real job,
         // it can lock itself up if it can't keep up.

         // Process locally for display
         if (c == '\n')
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
            if (serialBufPtr >= tCOLS - 1)
            {
               serialBufPtr = 0;
               AddLineToCommandTerminal(serialBuf);
               memset(serialBuf, '\0', tCOLS);
            }
         }
      }
   }

  // Grab the messages from GRBL,
  // forward them to the computer,
  // then parse them here to
  // display the state on the pendant.
  while(userial.available())
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
           AddLineToGrblTerminal(UserialBuf);
           UserialBufPtr = 0;
           ProcessLineFromGrbl();
           memset(UserialBuf, '\0', 512);
        }
     }
  }
  ///////////////////////////////////////////////////////////

  // TODO: Reorder this with the input states above...
  if(digitalReadFast(ENC_SW) == LOW && pastEncoderSwitchPos == false)
  {
     // Press
     pastEncoderSwitchPos = true;
     if(Sel1Pos.k() == SEL_NONE && Sel2Pos.k() == SEL_NONE)
     {
        DrawTestPattern();
     }
     else if(grblState.state == eJog &&
             (Sel1Pos.k() == AXIS_X || Sel1Pos.k() == AXIS_Y || Sel1Pos.k() == AXIS_Z))
     {
        // Stop the jog command imediately.
        userial.write(0x85);
        SendToGrbl("G4P0");
     }
     else if(Sel1Pos.k() == FILES)
     {
        GC.Select(true);
     }
  }
  else if(digitalReadFast(ENC_SW) == HIGH && pastEncoderSwitchPos == true)
  {
     // Release
     pastEncoderSwitchPos = false;
     if(Sel1Pos.k() == SEL_NONE && Sel2Pos.k() == SEL_NONE)
     {
        RefreshScreen();
     }
     else if(Sel1Pos.k() == SPINDLE)
     {
        char s[20] = { '\0' };
        sprintf(s, "S%ld", grblState.cmdSpindleSpeed);
        SendToGrbl(s);
        RequestGrblStateUpdate(GCREQ);
     }
     else if(Sel1Pos.k() == FEEDRATE)
     {
        char s[20] = { '\0' };
        sprintf(s, "F%ld", grblState.cmdFeedRate);
        SendToGrbl(s);
        RequestGrblStateUpdate(GCREQ);
     }
     else if(Sel1Pos.k() == FILES)
     {
        GC.Select(false);
     }
   }

   // At the end of the frame reset the selector change flags if necessary.
   if(Sel1Pos.ChangeInFrame())
   {
      Sel1Pos.ResetChangeInFrame();
   }

   if(Sel2Pos.ChangeInFrame())
   {
      Sel2Pos.ResetChangeInFrame();
   }
}