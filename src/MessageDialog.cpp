#include "Dialog.h"
#include "Config.h"

MessageDialog::MessageDialog(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *title, DialogStatesT defState) :
   Dialog(x, y, w, h, title), state(defState)
{
}

void MessageDialog::Select(bool pressed)
{
   if(pressed == false)
   {
      // On release
      if(onChoice != nullptr)
      {
         onChoice(state);
      }
      DrawButtons(false);
   }
   else
   {
      DrawButtons(true);
   }
}

void MessageDialog::Select(int8_t steps)
{
   if(state != MessageDialog::eOk)
   {
      state = MessageDialog::eOk;
      DrawButtons();
   }
}

void MessageDialog::Draw()
{
   tft.fillRect(_x, _y, _w, _h, ILI9488_BLACK);
   tft.drawRect(_x, _y, _w, _h, ILI9488_PURPLE);

   DrawTitle();
   DrawButtons();
   //tft.updateScreenAsync();
}

void MessageDialog::DrawButtons(bool selected)
{
   int16_t x, y;
   uint16_t w, h;
   uint16_t bw(_w / 3), bh(_h / 3), bx(_x + ((_w - bw) / 2)), by((_y + _h) - (bh + 3));

   tft.getTextBounds("Ok", bx, by, &x, &y, &w, &h);
   uint16_t tx(bx + ((bw - w) / 2)), ty(by + ((bh - h) / 2));

   tft.fillRoundRect(bx, by, bw, bh, 8, ILI9488_BLACK);
   if(state == MessageDialog::eOk)
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

   tft.updateScreenAsync();
}

void MessageDialog::Close()
{
   // Clean up.
   // Don't release the dialog until the internal
   // calls to redraw are complete.
   tft.waitUpdateAsyncComplete();
}