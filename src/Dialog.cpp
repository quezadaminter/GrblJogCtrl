#include "Dialog.h"
#include "Config.h"

Dialog::Dialog(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *title) :
   _x(x), _y(y), _w(w), _h(h), _title(title)
{
   //Serial.println("HELLO D!");
   //Serial.flush();
}

void Dialog::onChoiceConnect(std::function<void(uint8_t)> cb)
{
   onChoice = std::move(cb);
   //onChoice = cb;
}

void Dialog::DrawTitle()
{
   tft.setTextSize(2);
   int16_t x, y;
   uint16_t w, h;
   if(_title != nullptr)
   {
      tft.getTextBounds(_title, _x + 2, _y + 2, &x, &y, &w, &h);
      tft.setTextColor(ILI9488_LIGHTGREY, ILI9488_BLACK);
      uint8_t lines((w / _w) + 1);
      uint8_t slen(strlen(_title));
      uint8_t split(0);//slen / lines);
      uint8_t cw(w / slen);
      // The getTextBounds method seems to break the computations for x and y
      // if the bounds of the text exceed the size of the display buffer.
      x = _x + 2;
      y = _y + 2;
      h = 16;
      if(lines == 1)
      {
         Text(x, y, _title);
      }
      else
      {
         for(uint8_t l = 0; l < lines; ++l)
         {
            uint8_t lastSpace(255);
            for(uint8_t p = split; p < slen; ++p)
            {
               if(_title[p] == ' ')
               {
                  lastSpace = p;
               }
               if((p - split) * cw > _w)
               {
                  if(lastSpace == 255)
                  {
                     lastSpace = p - 1;
                  }
                  break;
               }
            }
            if(l == 0)
            {
               Text(x, y + (h * l), &_title[0], lastSpace);
               split = lastSpace + 1;
            }
            else
            {
               Text(x, y + (h * l), &_title[split], lastSpace - split);
               split = lastSpace + 1;
            }
         }
      }
   }
}