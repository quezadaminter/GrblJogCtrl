#ifndef _Encoder
#define _Encoder

#include <Arduino.h>

class Encoder
{
   public:
      Encoder(){}
      void begin(uint8_t pA, uint8_t pB, uint8_t pSW = 255);

      void HandleInterrupt();
      int8_t Update();//Stream *toGRBL);
      time_t GetuSecs() const { return(uSecs); }
      time_t GetOlduSecs() const { return(olduSecs); }

      void onMotionConnect(void(*func)(int8_t steps) = nullptr);

   private:
      uint8_t _pinA = 255;
      uint8_t _pinB = 255;
      uint8_t _pinSW = 255;

      uint8_t _OldAB = 3;
      int8_t _EncVal = 0;
      volatile int8_t _EncSteps = 0;
      volatile time_t uSecs = 0, olduSecs = 0;

      void (*onMotion)(int8_t steps);
};

extern Encoder Enc;
#endif