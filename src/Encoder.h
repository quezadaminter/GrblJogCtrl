#ifndef _Encoder
#define _Encoder

#include <Arduino.h>

class Encoder
{
   public:
      Encoder(){}
      void begin(uint8_t pA, uint8_t pB, uint8_t pSW = 255);

      void HandleInterrupt();
      void Update(Stream *toGRBL);

   private:
      uint8_t _pinA = 255;
      uint8_t _pinB = 255;
      uint8_t _pinSW = 255;
      char _jogCommand[128] = { '\0' };
      time_t _commandIntervalDelay = 100; // mSecs
      float _resolution = 10; // Inverse of distance travelled per pulse, for ex, 10 = 1 / 0.1mm/pulse 

      uint8_t _OldAB = 3;
      int8_t _EncVal = 0;
      volatile int8_t _EncSteps = 0;
      uint16_t AXES_ACCEL [3] = { 500, 500, 50 }; // mm/s/s <-- Need to fill these from the GRBL eeprom values $120-$122
      volatile time_t uSecs = 0, olduSecs = 0;
      bool jogging = false;
      uint16_t JOG_MIN_SPEED = 1000; // mm/min (Don't make it too slow).
      uint16_t JOG_MAX_SPEED = 8000; // mm/min (Needs to be set from the GRBL eeprom settings $110-$112

      void Jog_WheelSpeedVsFeedRate(time_t delta, int8_t dir, float &f, float &s);
      void Jog_StepsPerRevolution(time_t delta, int8_t dir, float &f, float &s);
};

extern Encoder Enc;
#endif