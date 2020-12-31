#include <stdint.h>
#include "SD.h"
#include "Queue.h"
#include "Dialog.h"

class Streamer
{
   public:
      Streamer();
      bool begin();
      void Select(bool pressed);
      void Select(int8_t steps);
      void HandleSelectorStateChange(uint8_t now, uint8_t was);
      bool OpenDirectory(const char *name);
      bool OpenFile(const char *name);
      uint16_t Available();
      uint8_t Read();
      void CloseFile();
      void Acknowledge();
      void Update();
      uint16_t Count() const { return(fileCount); }
      uint32_t TotalLines() const { return(totalLinesInFile); }
      uint32_t ProcessedLines() const { return(lineCount); }
      uint16_t RingBufferSpaceAvailable() const;

#define STREAM_STATUS_ENUM \
ADD_ENUM(OFF)\
ADD_ENUM(LIST)\
ADD_ENUM(SELECT)\
ADD_ENUM(CONFIRM)\
ADD_ENUM(ARM_FILE)\
ADD_ENUM(STREAM)\
ADD_ENUM(HOLD)\
ADD_ENUM(CANCEL)\
ADD_ENUM(COMPLETE)\
ADD_ENUM(SD_ERROR)\
ADD_ENUM(FILE_ERROR)\
ADD_ENUM(LINE_TOO_LONG)\
ADD_ENUM(FAILED_STATE)

enum streamStatusT
{
#define ADD_ENUM(eVal) eVal,
   STREAM_STATUS_ENUM
#undef ADD_ENUM
   eStatusRange
};

const char *statusImage[eStatusRange] =
{
#define ADD_ENUM(eVal) #eVal,
   STREAM_STATUS_ENUM
#undef ADD_ENUM
};
      streamStatusT State()  const { return(streamerState); }

   private:

      queue grblQ;
      uint16_t charactersSent = 0;
      uint16_t charactersPulledFromLine = 0;
      uint16_t loadedLineLength = 0;
      uint32_t lineCount = 0;
      uint32_t totalLinesInFile = 0;
      File gcodeFile;
      uint16_t fileCount = 0;
      uint16_t selectedFile = 0;
      char filesTerminal[tROWS][tCOLS];
      streamStatusT streamerState = OFF;
      Dialog *dialog = nullptr;

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
      const char *GetSelectedFileName();
      int8_t ReadFile();

      void onOkCancelChoiceMade(uint8_t);
      std::function<void(uint8_t)> onOkCancelChoiceMadeConnect;

      void onMessageChoiceMade(uint8_t);
      std::function<void(uint8_t)> onMessageChoiceConnect;

};

extern Streamer GC;