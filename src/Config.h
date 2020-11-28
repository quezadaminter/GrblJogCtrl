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
extern volatile uint8_t *sel1Pos = NULL;
extern volatile uint8_t *sel2Pos = NULL;

// GRBL
enum grblStatesT { eUnkownState, eIdle, eRun, eHold, eJog, eAlarm, eDoor, eCheck, eHome, eSleep };

extern grblStatesT grblState = eUnkownState;
#endif