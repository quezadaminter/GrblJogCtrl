#include "Encoder.h"
#include "Config.h"

Encoder Enc;

const int8_t ENCODER_STATES [] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};  //encoder lookup table

void Encoder::begin(uint8_t pA, uint8_t pB, uint8_t pSW)
{
   _pinA = pA;
   _pinB = pB;
   _pinSW = pSW;

   if(pSW != 255)
   {
      pinMode(_pinSW, INPUT_PULLUP);
   }
  
  pinMode(_pinA, INPUT_PULLUP);
  pinMode(_pinB, INPUT_PULLUP);
//  attachInterrupt(digitalPinToInterrupt(_pinA), this->TestEncoder, CHANGE);
//  attachInterrupt(digitalPinToInterrupt(_pinB), this->TestEncoder, CHANGE);
}

void Encoder::HandleInterrupt()
{
   _OldAB <<= 2; // Store previous state in higher bits
   uint8_t a(digitalReadFast(_pinA) == HIGH ? 1 : 0);
   uint8_t b(digitalReadFast(_pinB) == HIGH ? 1 : 0);
   b <<= 1;
   _OldAB |= (a | b);
   _EncVal += ENCODER_STATES[_OldAB & 0x0F];
   if(_EncVal > 3)
   {
      // Completed 4 steps forward, 1 click
      _EncSteps += 1;
      _EncVal = 0;
      olduSecs = uSecs;
      uSecs = millis();
   }
   else if(_EncVal < -3)
   {
      // Completed 4 steps backward, 1 click
      _EncSteps -= 1;
      _EncVal = 0;
      olduSecs = uSecs;
      uSecs = millis();
   }
}

// Move this to be required at begin() call.
void Encoder::onMotionConnect(void(*func)(int8_t steps))
{
   onMotion = func;
}

int8_t Encoder::Update()//Stream *toGRBL)
{
   int8_t es(_EncSteps);
   // Reset the encoder steps even if we don't use them.
   // Otherwise they buffer and when switching into the
   // jog modes the controller will send Grbl a movement
   // request.
   _EncSteps = 0;
   return(es);
}