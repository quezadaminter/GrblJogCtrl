#ifndef _DIALOG
#define _DIALOG
#include <stdint.h>
#include <stdlib.h>
#include <functional>
#include <Arduino.h>

class Dialog
{
   public:
      Dialog(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *title);
      virtual ~Dialog()
      {
         //Serial.println("Destroy!");
      }
      virtual void Draw() = 0;
      virtual void Close() = 0;
      virtual void Select(bool pressed) = 0;
      virtual void Select(int8_t steps) = 0;
      virtual uint8_t GetState() const = 0;

      //virtual void onChoiceConnect(void(*func)(uint8_t state) = nullptr);
      virtual void onChoiceConnect(std::function<void(uint8_t)> cb);
   protected:
      uint16_t _x, _y, _w, _h;
      const char *_title = nullptr;

      virtual void DrawTitle();
      //void (*onChoice)(uint8_t state);
      std::function<void(uint8_t)> onChoice;
};

class OkCancelDialog : public Dialog
{
   public:
      enum DialogStatesT { eUnknown, eOk, eCancel };
      OkCancelDialog(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *title, DialogStatesT defState = eUnknown);
      void SetState(DialogStatesT s){ state = s; }
      uint8_t GetState() const { return(state); }
      void Draw();
      void Close();
      void Select(bool pressed);
      void Select(int8_t steps);
      //void onChoiceConnect(void(*func)(DialogStatesT state) = nullptr);
   private:
      DialogStatesT state = eUnknown; 
      void DrawButtons(bool selected = false);
      //void (*onChoice)(DialogStatesT state);
};

class MessageDialog : public Dialog
{
   public:
      enum DialogStatesT { eUnknown, eOk };
      MessageDialog(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *title, DialogStatesT defState = eUnknown);
      void SetState(DialogStatesT s){ state = s; }
      uint8_t GetState() const { return(state); }
      void Draw();
      void Close();
      void Select(bool pressed);
      void Select(int8_t steps);
   private:
      DialogStatesT state = eUnknown; 
      void DrawButtons(bool selected = false);
};

#endif