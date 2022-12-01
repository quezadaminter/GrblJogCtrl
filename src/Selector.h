#ifndef _SELECTOR
#define _SELECTOR

#define SEL_NONE 255 // Both SW1 and SW2

// Represents each of the pins on the selector.
class SelectorInput
{
   public:
      enum StateT { eNone, eSelected, eNotSelected };

      SelectorInput(uint8_t PIN, const char *LABEL, void(*change)());
      void begin();
      static StateT Test(SelectorInput &si, uint32_t now);

      uint8_t pin = SEL_NONE;
      const char *label;
      uint32_t debounce = 0;
      void (*onChange)() = nullptr;
};

// Represents the switch itself.
class SelectorT
{
   public:
      SelectorT(SelectorInput *empty);
      void Arm(SelectorInput &si);
      /**
       * @brief Shifts the armed value to the active value.
       * 
       */
      void Update();
      volatile SelectorInput *Active();
      uint8_t Is();
      bool Is(uint8_t pin);
      uint8_t Was();
      bool hasChange();

      volatile SelectorInput *active = nullptr;
      volatile SelectorInput *armed = nullptr;
      volatile SelectorInput *was = nullptr;
      bool change = false;
};

#define SYSTEM   31   // SW1.1
#define AXIS_X   30   // SW1.2
#define AXIS_Y   29   // SW1.3
#define AXIS_Z   28   // SW1.4
#define SPINDLE  27   // SW1.5
#define FEEDRATE 26   // SW1.6
#define LCDBRT   25   // SW1.7
#define FILES    24   // SW1.8

#define JOG    34     // SW2.1
#define XP1    35     // SW2.2
#define X1     36     // SW2.3
#define X10    37     // SW2.4
#define X100   38     // SW2.5
#define F1     39     // SW2.6
#define F2     40     // SW2.7
#define DEBUG  41     // SW2.8

#endif