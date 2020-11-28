#include "Encoder.h"
#include "Config.h"

Encoder Enc;

void Encoder::begin(uint8_t pA, uint8_t pB, uint8_t pSW = 255)
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
  attachInterrupt(digitalPinToInterrupt(_pinA), TestEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(_pinB), TestEncoder, CHANGE);

}

void Encoder::TestEncoder()
{
   mOldAB <<= 2; // Store previous state in higher bits
   uint8_t a(digitalReadFast(_pinA) == HIGH ? 1 : 0);
   uint8_t b(digitalReadFast(_pinB) == HIGH ? 1 : 0);
   b <<= 1;
   mOldAB |= (a | b);
   mEncVal += ENCODER_STATES[mOldAB & 0x0F];
   if(mEncVal > 3)
   {
      // Completed 4 steps forward, 1 click
      mEncSteps += 1;
      mEncVal = 0;
      olduSecs = uSecs;
      uSecs = millis();
   }
   else if(mEncVal < -3)
   {
      // Completed 4 steps backward, 1 click
      mEncSteps -= 1;
      mEncVal = 0;
      olduSecs = uSecs;
      uSecs = millis();
   }
}

int8_t Encoder::Update()
{
   if((grblState == eJog || grblState == eIdle) &&
      (*sel1Pos == AXIS_X || *sel1Pos == AXIS_Y || *sel1Pos == AXIS_Z))
   {
      if(*sel2Pos == JOG)
      {
         float dt = -1, dt1 = -1;
         if(mEncSteps != 0)
         {
            time_t delta(uSecs - olduSecs);
            //float fr(mEncSteps / (float)delta);
            //Serial.println(delta);Serial.println(mEncSteps);Serial.println(fr);
            //fr = map(fr, 0.0, 1.0, 0, 50);
            //Serial.println(fr);
            //Serial.println(mEncSteps);
            //userial.print("delta: "); userial.println(delta);
            float f(0);
            float s(0);
            int8_t dir(mEncSteps > 0 ? 1 : -1);
            if(delta > 100)
            {
               s = 0.1 * dir;
               f = 500;
               // Do not bother sending a stop command for this
               // motion since it is too short.
            }
            else
            {
               jogging = true;
               delta = delta < 10 ? 10 :delta;
               f = map(delta, 100.0, 10.0, 100, 1000);
              //if(delta < 10)
              //{
              //   f = 1000;
              //}
              //else if(delta < 25)
              //{
              //   f = 750;
              //}
              //else if(delta < 50)
              //{
              //   f = 500;
              //}
              //else if(delta < 75)
              //{
              //   f = 250;
              //}
              //else if(delta <= 100)
              //{
              //   f = 100;
              //}
              float v(f / 60.0);
              dt = ((v * v) / (2.0 * AXES_ACCEL[0] * 14));
              //dt = dt < 0.01 ? 0.01 : dt;

              dt1 = v / AXES_ACCEL[0];
              dt1 = v * dt1 * dir;
              s = (v * dt) * dir;
            }
            
            char jog[128] = { '\0' };
            const char *XYZ = (*sel1Pos == AXIS_X ? "X" : *sel1Pos == AXIS_Y ? "Y" : "Z");
            sprintf(jog, "$J=G91F%.3f%s%.3f ; %ld ; %f ; %f", f, XYZ, s, delta, dt, dt1);
            userial.println(jog);
            Serial.println(jog);
            mEncSteps = 0;
         }
         else if(jogging == true)
         {
            if(millis() - uSecs > 200)
            {
               jogging = false;
               userial.write(0x85);
               //userial.println("G4P0");
               Serial.println("STOP");
            }
         }
      }
      else if(*sel2Pos == XP1 && mEncSteps != 0)
      {
         int8_t dir(mEncSteps > 0 ? 1 : -1);
         char jog[128] = { '\0' };
         const char *XYZ = (*sel1Pos == AXIS_X ? "X" : *sel1Pos == AXIS_Y ? "Y" : "Z");
         sprintf(jog, "$J=G91F100%s%.3f", XYZ, 0.1 * dir);
         userial.println(jog);
      }
      else if(*sel2Pos == X1 && mEncSteps != 0)
      {
         int8_t dir(mEncSteps > 0 ? 1 : -1);
         char jog[128] = { '\0' };
         const char *XYZ = (*sel1Pos == AXIS_X ? "X" : *sel1Pos == AXIS_Y ? "Y" : "Z");
         sprintf(jog, "$J=G91F100%s%d", XYZ, dir);
         userial.println(jog);
      }
      else if(*sel2Pos == X10 && mEncSteps != 0)
      {
         int8_t dir(mEncSteps > 0 ? 1 : -1);
         char jog[128] = { '\0' };
         const char *XYZ = (*sel1Pos == AXIS_X ? "X" : *sel1Pos == AXIS_Y ? "Y" : "Z");
         sprintf(jog, "$J=G91F250%s%d", XYZ, 10 * dir);
         userial.println(jog);
      }
      else if(*sel2Pos == X100 && mEncSteps != 0 && *sel1Pos != AXIS_Z)
      {
         int8_t dir(mEncSteps > 0 ? 1 : -1);
         char jog[128] = { '\0' };
         const char *XYZ = (*sel1Pos == AXIS_X ? "X" : "Y");
         sprintf(jog, "$J=G91F500%s%d", XYZ, 100 * dir);
         userial.println(jog);
      }
      mEncSteps = 0;
   }
}