#ifndef _Encoder
#define _Encoder

#include <Arduino.h>

class Encoder
{
   public:
      Encoder(){}
      void begin(uint8_t pA, uint8_t pB, uint8_t pSW = 255);

      int8_t Update();

   private:
      uint8_t _pinA = 255;
      uint8_t _pinB = 255;
      uint8_t _pinSW = 255;

      uint8_t mOldAB = 3;
      int8_t mEncVal = 0;
      volatile int8_t mEncSteps = 0;
      uint16_t AXES_ACCEL [] = { 10, 10, 10 }; // mm/s/s
      volatile time_t uSecs = 0, olduSecs = 0;
      bool jogging = false;
      volatile float moveDist = 0; // mm

      void TestEncoder();
};

extern Encoder Enc;
#endif