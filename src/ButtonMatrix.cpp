#include "ButtonMatrix.h"

ButtonMatrix BM;

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
}

void ButtonMatrix::onChangeConnect(void(*func)(ButtonMasksType button, ButtonStateType state))
{
   onChange = func;
}

uint8_t ButtonMatrix::Update()
{
   // Latch the parallel inputs
   if(_clkInh != 255)
   {
      digitalWriteFast(_clkInh, HIGH);
   }
   digitalWriteFast(_SOL, LOW);
   delayNanoseconds(200);
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

      digitalWriteFast(_clk, HIGH);
      delayNanoseconds(200);
      digitalWriteFast(_clk, LOW);
      delayNanoseconds(200);
   }

   // If the callback is available sort out
   // the button states.
   if(onChange != nullptr)
   {
      uint8_t change(_data ^ data);
      if(change != 0 && _read == false)
      {
         _change = change;
         _debounce = 0;
         _read = true;
         //Serial.print("Ch: ");Serial.print(millis());
      }
      else if(_read == true && _debounce >= 180)
      {
         //Serial.print(", Ap: ");Serial.print(millis());
         if(_change != 0)
         {
            for(uint8_t i = 0; i < eBMRange; ++i)
            {
               if((_change & _buttons[i]) == _buttons[i])
               {
                  onChange((ButtonMasksType)i, ((data & _buttons[i]) == _buttons[i] ? ePressed : eReleased));
               }
            }
            _change = 0;
         }
         _read = false;
      }
   }
   _data = data;
   return(_data);
}