#ifndef _CONFIG
#define _CONFIG

//#define USE_GDB

#include <stdint.h>
#include "ILI9488_t3.h"
#include "Selector.h"
#include "GRBL.h"

// Button Matrix
extern bool btnShiftPressed;

// Selectors
extern SelectorT Selector1;
extern SelectorT Selector2;

// Colors not defined by the ILI9844_t3 library
#define ILI9488_LIGHTGREEN 0x87FF // 150, 255, 150

// Display
#define tROWS 33 //40
#define tCOLS 31
#define cROWS 28
extern ILI9488_t3 tft;
extern uint8_t lcdBrightness;
extern elapsedMillis displayRefresh;
extern elapsedMillis heartBeatTimeout;
extern elapsedMillis laserTimeout;
extern void DrawTestPattern();
extern void RefreshScreen();
extern void Text(uint16_t x, uint16_t y, const char *text);
extern void Text(uint16_t x, uint16_t y, const char *text, size_t len);
extern void AddLineToCommandTerminal(const char *line);
void UpdateClockDisplay();
void UpdateSpindleDisplay();
void UpdateFeedRateDisplay();

// GRBL
enum grblStatesT { eUnknownState, eConnected, eIdle, eRun, eHold, eJog, eAlarm, eDoor, eCheck, eHome, eSleep };

enum grblMotionModeT { emUnkown, emG0, emG1, emG2, emG3, emG38_2, emG38_3, emG38_4, emG38_5, emG80 };
enum grblCoordSpaceT { ecG54, ecG55, ecG56, ecG57, ecG58, ecG59, ecRange, ecUnkown };
enum grblPlaneSelectT { epUnknown, epG17, epG18, epG19 };
enum grblDistanceModeT { edUnfIkown, edG90, edG91 };
enum grblFeedRateModeT { efUnknown, efG93, efG94 };
enum grblUnitsModeT { euUnknown, euG20, euG21 };
enum grblTLOModeT { etUnknown, etG3_1, et49 };
enum grblProgramModeT { eMUnkown, eM0, eM1, eM2, eM30 };
enum grblSpindleStateT { esUnknown, esM3, esM4, esM5 };
enum grblCoolantStateT { eCUnknown, eCM7, eCM8, eCM9 };


extern const char *grblStatusImage[eStatusRange];

extern char *statusValues[eStatusRange];
extern float GetMachinePosition(char axis);

typedef struct
{
   char id[4] = { '\0' };
   float val3[3] = { 0.f, 0.f, 0.f };
} idP3t;

typedef struct
{
   grblStatesT state = eUnknownState;

   bool isHomed = false;

   // Reported position
   float RPOS[3] = { 0.f, 0.f, 0.f };
   // Grbl > 1.1 reports either MPos or WPos depending on value of $10
   bool positionIsMPos = true;
   // Active work coordinate offset
   float WCO[3] = { 0.f, 0.f, 0.f };
   // Buffer
   uint8_t plannerBlocksAvailable = 0;
   uint8_t rxBufferBytesAvailable = 0;
   //spindle
   float currentSpindleSpeed = 0.f;
   int32_t cmdSpindleSpeed = 0;
   float programmedSpindleSpeed = 0.f;

   // Commanded travel
   float cmdTravel[3] = {0.f, 0.f, 0.f }; // Absolute position accumulator.
   // Feed Rate
   float currentFeedRate = 0.f;
   float programmedFeedRate = 0.f;
   int32_t cmdFeedRate = 0.f;

   // Overrides
   uint8_t feedOverride = 0;
   uint8_t rapidOverride = 0;
   uint8_t spindleOverride = 0;

   // EEPROM ($$)
   float axisMaxRate[3] = { 1000.f, 1000.f, 500.f };
   float axisMaxAccel[3] = { 50.f, 50.f, 50.f };
   float axisMaxTravel[3] = { 500.f, 500.f, 100.f };
   float zAxisTopLimit = -2.0; // The pull-off position after homing. TODO: Find an automated way to set this value from the firmware.
   bool laserMode = false;
   float maxSpindleSpeed = 0.f;

   // Gcode parameters ($#)
   idP3t workSpace[ecRange];
   idP3t park28;
   idP3t park30;
   idP3t offset92;
   idP3t probe;
   float TLO = 0.f;
   idP3t *activeWorkspace = nullptr;

   // Jogging ($J=)
   bool jogging = false;

   // Gcode parser state ($G)
   grblMotionModeT motionMode;
   grblFeedRateModeT feedRateMode;
   grblUnitsModeT unitsMode;
   grblDistanceModeT distanceMode;
   grblPlaneSelectT planeMode;
   grblTLOModeT toolLengthMode;
   grblCoordSpaceT activeWorkspaceMode;
   grblProgramModeT programMode;
   grblCoolantStateT coolantMode;
   grblSpindleStateT spindleState;

   // Build information ($I)
   uint8_t plannerBlockCount = 0;
   uint8_t rxBufferSize = 0;

   bool synchronize = true;
   uint8_t requestEEUpdate = 1;
   uint8_t requestWSUpdate = 1;
   uint8_t requestGCUpdate = 1;
   uint8_t requestBDUpdate = 1;

} grblStateStruct;

extern grblStateStruct grblState;

#define GRBL_RX_BUFFER_SIZE 128

// Commands
#define GRBL_HELP "$"
#define GRBL_VIEWSETTINGS "$$"
#define GRBL_PARAMETERS "$#"
#define GRBL_GCODEPARSER "$G"
#define GRBL_BUILDINFO "$I"
#define GRBL_STARTUPBLOCK "$N"
#define GRBL_CHECKMODE "$C"
#define GRBL_UNLOCK "$X"
#define GRBL_HOMING "$H"
#define GRBL_JOG "$J="
#define GRBL_SLEEP "$SLP"
#define GRBL_SET_X_ZERO "G10P0L20X0"
#define GRBL_SET_Y_ZERO "G10P0L20Y0"
#define GRBL_SET_Z_ZERO "G10P0L20Z0"
#define GRBL_PROBE_X "G91\nG38.2X10F25\nG90"
#define GRBL_PROBE_Y "G91\nG38.2Y10F25\nG90"
#define GRBL_PROBE_Z "G91\nG38.2Z-10F25\nG90"
#define GRBL_JOG_TO_X0 "$J=G90F6000X0"
#define GRBL_JOG_TO_Y0 "$J=G90F6000Y0"
#define GRBL_JOG_TO_Z0 "$J=G90F500Z0"
#define GRBL_JOG_TO_X0Y0 "$J=G90F6000X0Y0"
#define GRBL_LASER_MODE_ON "$32=1"
#define GRBL_LASER_MODE_OFF "$32=0"
#define GRBL_LASER_TESTMODE_ON "M3S1000"
#define GRBL_LASER_TESTMODE_OFF "M5\nS0"

// Realtime Commands
#define GRBL_SOFTRESET 0x18
#define GRBL_STATUSREPORT '?'
#define GRBL_CYCLESTARTRESUME '~'
#define GRBL_FEEDHOLD '!'
#define GRBL_SAFETYDOOR 0x84
#define GRBL_JOGCANCEL 0x85
#define GRBL_FEEDOD100 0x90
#define GRBL_FEEDODP10 0x91
#define GRBL_FEEDODM10 0x92
#define GRBL_FEEDODP1 0x93
#define GRBL_FEEDODM1 0x94
#define GRBL_RAPIDOD100 0x95
#define GRBL_RAPIDOD50 0x96
#define GRBL_RAPIDOD25 0x97
#define GRBL_SPINDLEOD100 0x99
#define GRBL_SPINDLEODP10 0x9A
#define GRBL_SPINDLEODM10 0x9B
#define GRBL_SPINDLEODP1 0x9C
#define GRBL_SPINDLEODM1 0x9D
#define GRBL_SPINDLESTOP 0x9E
#define GRBL_FLOODCOOLANT 0xA0
#define GRBL_MISTCOOLANT 0xA1

// Configuration Registers (SAMPLE)
//$0=10 	Step pulse, microseconds
//$1=255	Step idle delay, milliseconds
//$2=0 	Step port invert, mask
//$3=0 	Direction port invert, mask
//$4=0 	Step enable invert, boolean
//$5=0 	Limit pins invert, boolean
//$6=0 	Probe pin invert, boolean
//$10=1 	Status report, mask
//$11=0.010 	Junction deviation, mm
//$12=0.002 	Arc tolerance, mm
//$13=0 	Report inches, boolean
//$20=0 	Soft limits, boolean
//$21=0 	Hard limits, boolean
//$22=1 	Homing cycle, boolean
//$23=0 	Homing dir invert, mask
//$24=25.000 	Homing feed, mm/min
//$25=500.000 	Homing seek, mm/min
//$26=250 	Homing debounce, milliseconds
//$27=1.000 	Homing pull-off, mm
//$30=1000. 	Max spindle speed, RPM
//$31=0. 	Min spindle speed, RPM
//$32=0 	Laser mode, boolean
//$100=250.000 	X steps/mm
//$101=250.000 	Y steps/mm
//$102=250.000 	Z steps/mm
//$110=500.000 	X Max rate, mm/min
//$111=500.000 	Y Max rate, mm/min
//$112=500.000 	Z Max rate, mm/min
//$120=10.000 	X Acceleration, mm/sec^2
//$121=10.000 	Y Acceleration, mm/sec^2
//$122=10.000 	Z Acceleration, mm/sec^2
//$130=200.000 	X Max travel, mm
//$131=200.000 	Y Max travel, mm
//$132=200.000 	Z Max travel, mm
#endif
