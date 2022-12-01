#include <Arduino.h>
#include "Selector.h"

SelectorInput::SelectorInput(uint8_t PIN, const char *LABEL, void(*change)()) :
   pin(PIN), label(LABEL), onChange(change)
{ 
}

void SelectorInput::begin()
{
   if(pin != SEL_NONE && onChange != nullptr)
   {
      attachInterrupt(digitalPinToInterrupt(pin), onChange, CHANGE);
   }
}

SelectorInput::StateT SelectorInput::Test(SelectorInput &si, uint32_t now)
{
   SelectorInput::StateT state(eNone);
   if(now - si.debounce > 10)
   {
      si.debounce = now;
      state = (digitalReadFast(si.pin) == LOW) ? eSelected : eNotSelected;

      //if(state == SelectorInput::eSelected)
      //{
      //   // Selected
      //   Serial.print(now - si.debounce); Serial.println(si.label);
      //}
      //else if(state == SelectorInput::eNotSelected)
      //{
      //   // De-selected
      //   Serial.print(now - si.debounce); Serial.println(si.label);
      //}
         
   }
   return(state);
}

SelectorT::SelectorT(SelectorInput *empty)
{
   active = empty;
   armed = empty;
   was = empty;
}

void SelectorT::Arm(SelectorInput &si)
{
   armed = &si;
}

/**
* @brief Shifts the armed value to the active value.
* 
*/
void SelectorT::Update()
{
   if(active != armed)
   {
      change = true;
      was = active;
      active = armed;
   }
}

volatile SelectorInput *SelectorT::Active()
{
   change = false;
   return(active);
}

uint8_t SelectorT::Is()
{
   return(active->pin);
}

uint8_t SelectorT::Was()
{
   return(was->pin);
}

bool SelectorT::Is(uint8_t pin)
{
   return(active->pin == pin);
}

bool SelectorT::hasChange()
{
   return(change);
}