#ifndef _CONFIG
#define _CONFIG

// Selector Switches
#define HOME   31        // SW1.1
#define AXIS_X 30        // SW1.2
#define AXIS_Y 29        // SW1.3
#define AXIS_Z  28       // SW1.4
#define LOCKOUT  27      // SW1.5
#define UNLOCK    26     // SW1.6
#define RESET       25   // SW1.7

#define JOG      34      // SW2.1
#define XP1     35       // SW2.2
#define X1      36       // SW2.3
#define X10  37          // SW2.4
#define X100          38 // SW2.5
#define SET_AXIS_ZERO 39 // SW2.6
#define MACRO1 40        // SW2.7
#define MACRO2 41        // SW2.8
//#define MACRO3 16
//#define CYCLE_START 18
extern volatile uint8_t *sel1Pos;
extern volatile uint8_t *sel2Pos;

// GRBL
enum grblStatesT { eUnkownState, eConnected, eIdle, eRun, eHold, eJog, eAlarm, eDoor, eCheck, eHome, eSleep };
extern grblStatesT grblState;

// Commands
#define GRBL_HELP "$"
#define GRBL_VIEWSETTIMGS "$$"
#define GRBL_PARAMETERS "$#"
#define GRBL_GCODEPARSER "$G"
#define GRBL_BUILDINFO "$I"
#define GRBL_STARTUPBLOCK "$N"
#define GRBL_CHECKMODE "$C"
#define GRBL_UNLOCK "$X"
#define GRBL_HOMING "$H"
#define GRBL_JOG "$J="
#define GRBL_SLEEP "$SLP"

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

// Configuration Registers
//$0=10 	Step pulse, microseconds
//$1=25 	Step idle delay, milliseconds
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