#include <stdint.h>
#include "SD.h"
#include "Queue.h"
#include "Dialog.h"

class Streamer
{
   public:
      bool begin();
      void Select(bool pressed);
      void Select(int8_t steps);
      void HandleSelectorStateChange(uint8_t now, uint8_t was);
      bool OpenDirectory(const char *name);
      bool OpenFile(const char *name);
      bool ReadFile(char &c);
      void CloseFile();
      void Acknowledge();
      void Update();
      uint16_t Count() const { return(fileCount); }

   private:
      queue grblQ;
      uint16_t charactersSent = 0;
      File gcodeFile;
      uint16_t fileCount = 0;
      uint16_t selectedFile = 0;
      char filesTerminal[tROWS][tCOLS];
      volatile uint8_t streamerState = 0;
      OkCancelDialog *okcDialog = nullptr;

      void AddLineToFilesTerminal(const char *line);
      void ClearFilesTerminal();
      void ListFiles(File &dir);
      void UpdateFilesTerminal();
      void DisplayError();
      void HandleConfirmationDialog();
      void DrawOkCancelDialog();
      void HandleArmFileDialog();
      void MonitorStreaming();
      void HandleHoldDialog();
      void ProcessStremCancel();

      void onOkCancelChoiceMade(OkCancelDialog::DialogStatesT state);
};

extern Streamer GC;