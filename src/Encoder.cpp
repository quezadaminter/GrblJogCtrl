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

uint16_t interpolateFeedRateFromStepTimeDelta(float x, float in_min, float in_max, uint16_t out_min, uint16_t out_max)
{
  return (in_min - x) * (out_max - out_min) / (in_min - in_max) + out_min;
}

void Encoder::Jog_WheelSpeedVsFeedRate(time_t delta, int8_t dir, float &f, float &s)
{
   delta = delta < 10 ? 10 :delta;
   f = interpolateFeedRateFromStepTimeDelta(delta, 100.0, 10.0, 100, 1000);
   // Convert feed rate from mm/min to mm/sec
   float v(f / 60.0);
   float dt = ((v * v) / (2.0 * AXES_ACCEL[0] * 14));
   //dt = dt < 0.01 ? 0.01 : dt;

   //dt1 = v / AXES_ACCEL[0];
   //dt1 = v * dt1 * dir;
   s = (v * dt) * dir;
}

void Encoder::Jog_StepsPerRevolution(time_t delta, int8_t dir, float &f, float &s)
{
  if(delta > _commandIntervalDelay)
  {
      s = _EncSteps / _resolution;
  }
  f = 500; // Need to make this more dynamic
}

void Encoder::Update(Stream *toGRBL)
{
   //grblState = eJog;

   if((grblState == eJog || grblState == eIdle) &&
      (*sel1Pos == AXIS_X || *sel1Pos == AXIS_Y || *sel1Pos == AXIS_Z))
   {
      int8_t dir(_EncSteps > 0 ? 1 : -1);
      const char *XYZ(*sel1Pos == AXIS_X ? "X" : *sel1Pos == AXIS_Y ? "Y" : "Z");
      if(*sel2Pos == JOG)
      {
         //float dt = -1, dt1 = -1;
         if(_EncSteps != 0)
         {
            time_t delta(uSecs - olduSecs);
            float f(0);
            float s(0);
            if(delta > 100)
            {
               // Slow commands in jog mode that
               // are treated as fixed step inputs of
               // a short distance. Allows precise
               // and repeatable motion at small scale.
               s = 0.1 * dir;
               f = 500;
            }
            else
            {
               jogging = true;
               
               Jog_WheelSpeedVsFeedRate(delta, dir, f, s);
               //Jog_StepsPerRevolution(delta, dir, f, s);
            }
            
            //sprintf(_jogCommand, "$J=G91F%.3f%s%.3f ; %ld ; %f ; %f", f, XYZ, s, delta, dt, dt1);
            sprintf(_jogCommand, "$J=G91F%.3f%s%.3f", f, XYZ, s);
            toGRBL->println(_jogCommand);
            //Serial.println(_jogCommand);
            _EncSteps = 0;
         }
         else if(jogging == true)
         {
            if(millis() - uSecs > 200)
            {
               jogging = false;
               toGRBL->write(0x85);
               //userial.println("G4P0");
               //Serial.println("STOP");
            }
         }
      }
      else if(*sel2Pos == XP1 && _EncSteps != 0)
      {
         sprintf(_jogCommand, "$J=G91F100%s%.3f", XYZ, 0.1 * dir);
         toGRBL->println(_jogCommand);
      }
      else if(*sel2Pos == X1 && _EncSteps != 0)
      {
         sprintf(_jogCommand, "$J=G91F100%s%d", XYZ, dir);
         toGRBL->println(_jogCommand);
      }
      else if(*sel2Pos == X10 && _EncSteps != 0)
      {
         sprintf(_jogCommand, "$J=G91F250%s%d", XYZ, 10 * dir);
         toGRBL->println(_jogCommand);
      }
      else if(*sel2Pos == X100 && _EncSteps != 0 && *sel1Pos != AXIS_Z)
      {
         // 100 is too big for the Z axis. Allow this only for X and Y.
         sprintf(_jogCommand, "$J=G91F500%s%d", XYZ, 100 * dir);
         toGRBL->println(_jogCommand);
      }
      _EncSteps = 0;
   }
}