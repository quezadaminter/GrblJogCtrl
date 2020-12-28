#include "Dialog.h"
#include "Config.h"

OkCancelDialog::OkCancelDialog(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *title, DialogStatesT defState) :
   Dialog(x, y, w, h, title), state(defState)
{
}

void OkCancelDialog::Select(bool pressed)
{
   if(pressed == false)
   {
      // On release
      if(onChoice)// != nullptr)
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

void OkCancelDialog::Select(int8_t steps)
{
   if(steps > 0 && state != OkCancelDialog::eCancel)
   {
      state = OkCancelDialog::eCancel;
      DrawButtons();
   }
   else if(steps < 0 && state != OkCancelDialog::eOk)
   {
      state = OkCancelDialog::eOk;
      DrawButtons();
   }
}

void OkCancelDialog::Draw()
{
   tft.fillRect(_x, _y, _w, _h, ILI9488_BLACK);
   tft.drawRect(_x, _y, _w, _h, ILI9488_PURPLE);

   DrawTitle();
   DrawButtons();
   //tft.updateScreenAsync();
}

void OkCancelDialog::DrawButtons(bool selected)
{
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