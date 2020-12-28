#include "Dialog.h"
#include "Config.h"

Dialog::Dialog(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *title) :
   _x(x), _y(y), _w(w), _h(h), _title(title)
{
}

OkCancelDialog::OkCancelDialog(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *title, DialogStatesT defState) :
   Dialog(x, y, w, h, title), state(defState)
{
}

void OkCancelDialog::onChoiceConnect(void(*func)(OkCancelDialog::DialogStatesT state))
{
   onChoice = func;
}

void OkCancelDialog::Select(bool pressed)
{
   if(pressed == false)
   {
      // On release
      if(onChoice != nullptr)
      {
         onChoice(state);
      }
      Serial.println("x");
      DrawButtons(false);
   }
   else
   {
      Serial.println("y");
      DrawButtons(true);
   }
}

void OkCancelDialog::Select(int8_t steps)
{
   if(steps > 0 && state != OkCancelDialog::eCancel)
   {
      state = OkCancelDialog::eCancel;
      Serial.println("w");
      DrawButtons();
   }
   else if(steps < 0 && state != OkCancelDialog::eOk)
   {
      state = OkCancelDialog::eOk;
      Serial.println("z");
      DrawButtons();
   }
}

void OkCancelDialog::Draw()
{
   tft.fillRect(_x, _y, _w, _h, ILI9488_BLACK);
   tft.drawRect(_x, _y, _w, _h, ILI9488_PURPLE);

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

   Serial.println("f");
   DrawButtons();
   tft.updateScreenAsync();
}

void OkCancelDialog::DrawButtons(bool selected)
{
   Serial.println("g");
   int16_t x, y;
   uint16_t w, h;
   uint16_t bw(_w / 3), bh(_h / 3), bx(_x + 3), by((_y + _h) - (bh + 3));

   tft.getTextBounds("Ok", bx, by, &x, &y, &w, &h);
   uint16_t tx(bx + ((bw - w) / 2)), ty(by + ((bh - h) / 2));

   tft.fillRoundRect(bx, by, bw, bh, 8, ILI9488_BLACK);
   if(state == OkCancelDialog::eOk)
   {
      tft.fillRoundRect(bx, by, bw, bh, 8, ILI9488_GREEN);
      tft.setTextColor(selected ? ILI9488_WHITE : ILI9488_BLACK);
   }
   else
   {
      tft.drawRoundRect(bx, by, bw, bh, 8, ILI9488_GREEN);
      tft.setTextColor(ILI9488_GREEN, ILI9488_BLACK);
   }
   Text(tx, ty, "Ok");

   bx = ((_x + _w) - (bw + 3));
   tft.getTextBounds("Cancel", bx, by, &x, &y, &w, &h);
   tx = bx + ((bw - w) / 2);
   ty = by + ((bh - h) / 2);
   
   tft.fillRoundRect(bx, by, bw, bh, 8, ILI9488_BLACK);
   if(state == OkCancelDialog::eCancel)
   {
      tft.fillRoundRect(bx, by, bw, bh, 8, ILI9488_RED);
      tft.setTextColor(selected ? ILI9488_WHITE : ILI9488_BLACK);
      Serial.print("col:");Serial.println(selected);
   }
   else
   {
      tft.drawRoundRect(bx, by, bw, bh, 8, ILI9488_RED);
      tft.setTextColor(ILI9488_RED, ILI9488_BLACK);
   }
   Text(tx, ty, "Cancel");

   tft.updateScreenAsync();
}

void OkCancelDialog::Close()
{
   // Clean up.
   // Don't release the dialog until the internal
   // calls to redraw are complete.
   tft.waitUpdateAsyncComplete();
}