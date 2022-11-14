#include "ButtonMatrix.h"

ButtonMatrix BM;

void printBits(uint8_t v, bool ln = false)
{
   uint8_t mask(B10000000);
   Serial.print("B");
   for(uint8_t i = 0; i < 8; ++i)
   {
      if(mask & v)
      {
         Serial.print("1");
      }
      else
      {
         Serial.print("0");
      }
      mask = (mask >> 1);
   }
   if(ln == true)
   {
      Serial.println();
   }
}

ButtonMatrix::ButtonMatrix()
{
   _buttons[eR1C1] = R1C1;
   _buttons[eR1C2] = R1C2;
   _buttons[eR1C3] = R1C3;
   _buttons[eR2C1] = R2C1;
   _buttons[eR2C2] = R2C2;
   _buttons[eR2C3] = R2C3;
   _buttons[eR3C1] = R3C1;
   _buttons[eR3C2] = R3C2;
   _buttons[eR3C3] = R3C3;
   _buttons[eR4C1] = R4C1;
   _buttons[eR4C2] = R4C2;
   _buttons[eR4C3] = R4C3;
}

void ButtonMatrix::begin(uint8_t clk, uint8_t shiftLoad, uint8_t QH, uint8_t nQH, uint8_t clkInh)
{
   _clk = clk;
   _SOL = shiftLoad;
   _QH = QH;
   _nQH = nQH;
   _clkInh = clkInh;

   pinMode(clk, OUTPUT);
   pinMode(shiftLoad, OUTPUT);
   pinMode(QH, INPUT_PULLDOWN);

   if(nQH != 255)
   {
      pinMode(nQH, INPUT_PULLDOWN);
   }

   if(clkInh != 255)
   {
      pinMode(clkInh, OUTPUT);
   }
   
   // Enable the parallel inputs.
   digitalWriteFast(_SOL, LOW);

   _throttle = 0;
}

void ButtonMatrix::onChangeConnect(void(*func)(ButtonMasksType button, ButtonStateType state))
{
   onChange = func;
}

uint8_t ButtonMatrix::Update()
{
   // The SN74HC165 cannot run too fast or else the readings are
   // unreliable. Throttle to execute with some space between readings
   // to keep it happy.
   if(_throttle >= 200)
   {
      // Latch the parallel inputs
      if(_clkInh != 255)
      {
         digitalWriteFast(_clkInh, HIGH);
      }
      // Disable the parallel inputs and shift the values into the serial register.
      digitalWriteFast(_SOL, HIGH);
      if(_clkInh != 255)
      {
         digitalWriteFast(_clkInh, LOW);
      }

      // Clock the values
      uint8_t data(0);
      for(uint8_t i = 0; i < 8; ++i)
      {
         data = (data << 1);
         data |= digitalReadFast(_QH);

         // The TI SN74HC165 clocks on the low to high transition.
         digitalWriteFast(_clk, LOW);
         // Take a small pause between clock pulses to keep the
         // SN74HC165 happy.
         delayNanoseconds(200);
         digitalWriteFast(_clk, HIGH);
         // Wait again a bit for next clock pulse.
         delayNanoseconds(200);
      }
      // Enable the parallel inputs.
      digitalWriteFast(_SOL, LOW);

      // Data has been moved from the shift register to the local variable.
      // Do something with it.

      // If the callback is available sort out
      // the button states.
      if(onChange != nullptr)
      {
         // If the input changed between now and the last frame...
         _change = (_data ^ data);
         if(_change != 0)
         {
            // ... loop through the buttons and find the one that has the matching pattern
            // for a row and column definition.
            //printBits(_change, true);
            for(uint8_t i = 0; i < eBMRange; ++i)
            {
               if((_change & _buttons[i]) == _buttons[i])
               {
                  // Call the callback on the button that matches the change pattern.
                  onChange((ButtonMasksType)i, ((data & _buttons[i]) == _buttons[i] ? ePressed : eReleased));
               }
            }
            _throttle = 0;
         }
         else
         {
            _throttle = 0;
         }
      }
      else
      {
         _throttle = 0;
      }
      _data = data;
   }
   return(_data);
}