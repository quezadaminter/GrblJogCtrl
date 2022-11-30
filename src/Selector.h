#ifndef _SELECTOR
#define _SELECTOR

#define SEL_NONE 255 // Both SW1 and SW2

class uintCharStruct
{
   public:
      uintCharStruct()
      {
         k = SEL_NONE;
         val = "NONE";
      }
      uintCharStruct(uint8_t K, const char *V)
      {
         k = K;
         val = V;
      }

      uint8_t k;
      const char *val;
};

class SelectorInput
{
   public:
      SelectorInput(uint8_t PIN, const char *LABEL, void(*selected)(bool)) :
        pin(PIN), label(LABEL), onSelected(selected)
      { 
      }
      void begin(void(*change)())
      {
         if(pin != SEL_NONE && change != nullptr)
         {
            onChange = change;
            attachInterrupt(digitalPinToInterrupt(pin), onChange, CHANGE);
         }
      }
      static void Test(SelectorInput &si, uint32_t now)
      {
         if(now - si.debounce > 10)
         {
            si.debounce = now;
            if(si.onSelected != nullptr)
            {
               si.onSelected(digitalReadFast(si.pin) == LOW);
            }
         }
      }

      uint8_t pin = SEL_NONE;
      const char *label;
      uint32_t debounce = 0;
      void (*onChange)() = nullptr;
      void (*onSelected)(bool) = nullptr;
};

class SelectorT
{
   public:
      SelectorT(volatile uintCharStruct *init){ now = init; }
      uint8_t k() volatile { return(now->k); }
      const char *val() volatile { return(now->val); }
      uint8_t p() volatile { return(prev); }
      void Reset(uint8_t pos) volatile { Position = pos; }
      void Now(volatile uintCharStruct *n) volatile
      { prev = now->k; now = n; Interrupt = false; changeInFrame = true; Position = SEL_NONE; }
      bool ChangeInFrame() volatile { return(changeInFrame); }
      void ResetChangeInFrame() volatile { changeInFrame = false; }
      volatile uint32_t Debounce = 0;
      volatile bool Interrupt = false;
      volatile uint8_t Position = SEL_NONE;
   private:
      //static uintCharStruct emptySel;
      volatile uintCharStruct *now = nullptr;
      volatile uint8_t prev;
      bool changeInFrame = false;
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