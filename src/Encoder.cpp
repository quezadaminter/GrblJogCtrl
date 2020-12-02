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
   delta = delta < 10 ? 10 : delta;
   f = interpolateFeedRateFromStepTimeDelta(delta, 100.0, 10.0, JOG_MIN_SPEED, JOG_MAX_SPEED);
   // Convert feed rate from mm/min to mm/sec
   float v(f / 60.0);
   // Update AXES_ACCEL so it uses the value for the
   // selected axis.
   float dt = ((v * v) / (2.0 * AXES_ACCEL[0] * 14));
   //Serial.print("dt: ");Serial.println(dt);
   s = (v * dt) * dir;
   s /= 10;
}

void Encoder::Jog_StepsPerRevolution(time_t delta, int8_t dir, float &f, float &s)
{
   // UNTESTED. Left here for possible future development.
  if(delta > _commandIntervalDelay)
  {
      s = _EncSteps / _resolution;
  }
  f = 500; // Need to make this more dynamic
}


// This is where the magic happens. The encoder wheel
// pulses are translated into motion commands for GRBL.
//
// DO NOT USE THE println() method when sending
// data to GRBL. It injects both the \r and \n
// bytes to the line end and it causes GRBL to
// interpret this as 2 separate lines, causing
// extra overhead. Instead add the \n byte to
// the end of the formatted string and send it
// like so.
void Encoder::Update(Stream *toGRBL)
{
   // Trick the code into thinking grbl is connected.
   // Used when testing this block of code by itself.
   //grblState = eJog;

   if(jogging == true)
   {
      if(millis() - uSecs > 200)
      {
         jogging = false;
         toGRBL->write(0x85);
         //userial.println("G4P0");
         //Serial.println("STOP");
      }
   }
   if((grblState == eJog || grblState == eIdle) && _EncSteps != 0 &&
      (*sel1Pos == AXIS_X || *sel1Pos == AXIS_Y || *sel1Pos == AXIS_Z))
   {
      int8_t dir(_EncSteps > 0 ? 1 : -1);
      const char *XYZ(*sel1Pos == AXIS_X ? "X" : *sel1Pos == AXIS_Y ? "Y" : "Z");
      if(*sel2Pos == JOG)
      {
         time_t delta(uSecs - olduSecs);
         float f(0); // Feed rate (mm / min)
         float s(0); // Travel distance (mm)
         if(delta > 100)
         {
            // Slow commands in jog mode that
            // are treated as fixed step inputs of
            // a short distance. Allows precise
            // and repeatable motion at small scale.
            s = 0.1 * dir;
            f = 1000;
         }
         else
         {
            jogging = true;
            
            Jog_WheelSpeedVsFeedRate(delta, dir, f, s);
            //Jog_StepsPerRevolution(delta, dir, f, s);
         }
         
         sprintf(_jogCommand, "$J=G91F%.3f%s%.3f\n", f, XYZ, s);
         toGRBL->print(_jogCommand);
         //Serial.println(_jogCommand);
      }
      else if(*sel2Pos == XP1)// && _EncSteps != 0)
      {
         sprintf(_jogCommand, "$J=G91F1000%s%.3f\n", XYZ, 0.1 * dir);
         toGRBL->print(_jogCommand);
      }
      else if(*sel2Pos == X1)// && _EncSteps != 0)
      {
         sprintf(_jogCommand, "$J=G91F1000%s%d\n", XYZ, dir);
         toGRBL->print(_jogCommand);
      }
      else if(*sel2Pos == X10)// && _EncSteps != 0)
      {
         sprintf(_jogCommand, "$J=G91F3000%s%d\n", XYZ, 10 * dir);
         toGRBL->print(_jogCommand);
      }
      else if(*sel2Pos == X100 && *sel1Pos != AXIS_Z)// _EncSteps != 0)
      {
         // 100 is too big for the Z axis. Allow this only for X and Y.
         sprintf(_jogCommand, "$J=G91F5000%s%d\n", XYZ, 100 * dir);
         toGRBL->print(_jogCommand);
      }
   }
   // Reset the encoder steps even if we don't use them.
   // Otherwise they buffer and when switching into the
   // jog modes the controller will send Grbl a movement
   // request.
   _EncSteps = 0;
}