#ifndef _DIALOG
#define _DIALOG
#include <stdint.h>
#include <stdlib.h>

class Dialog
{
   public:
      Dialog(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *title);
      virtual ~Dialog()
      {
         //uint8_t lines(sizeof(_title) / sizeof(char));
         //for(uint8_t l = 0; l < lines; ++l)
         //{
         //   delete _title[l];
         //}
         ////delete [] _title;
         //free(_title);
      }
      virtual void Draw() = 0;
      virtual  void Close() = 0;
      virtual void Select(bool pressed) = 0;
      virtual void Select(int8_t steps) = 0;

   protected:
      uint16_t _x, _y, _w, _h;
      const char *_title = nullptr;
};

class OkCancelDialog : public Dialog
{
   public:
      enum DialogStatesT { eUnknown, eOk, eCancel };
      OkCancelDialog(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *title, DialogStatesT defState = eUnknown);
      void SetState(DialogStatesT s){ state = s; }
      DialogStatesT GetState() const { return(state); }
      void Draw();
      void Close();
      void Select(bool pressed);
      void Select(int8_t steps);
      void onChoiceConnect(void(*func)(DialogStatesT state) = nullptr);
   private:
      DialogStatesT state = eUnknown; 
      void DrawButtons(bool selected = false);
      void (*onChoice)(DialogStatesT state);
};

#endif