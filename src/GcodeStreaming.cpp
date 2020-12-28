#include <cstring>
#include "Config.h"
#include "GcodeStreaming.h"
#include "ILI9488_t3.h"

//uint8_t outCountList[GRBL_RX_BUFFER_SIZE] = { 0 };
//#define OUT_BUFFER_MASK (GRBL_RX_BUFFER_SIZE - 1)
//char *outBuffer[GRBL_RX_BUFFER_SIZE] = { '\0' };
//uint16_t outHead = 0;
//uint16_t outTail = 0;
//uint16_t outCount = 0;

#define OFF 0
#define LIST 1
#define SELECT 2
#define CONFIRM 3
#define ARM_FILE 4
#define STREAM 5
#define HOLD 6
#define CANCEL 7
#define SD_ERROR 200
#define FILE_ERROR 201
#define FAILED_STATE 255

Streamer GC;

Streamer::Streamer()
{
   onOkCancelChoiceMadeConnect = std::bind(&Streamer::onOkCancelChoiceMade, *this, std::placeholders::_1);
   onMessageChoiceConnect  = std::bind(&Streamer::onMessageChoiceMade, *this, std::placeholders::_1);
}

bool Streamer::begin()
{
   ClearFilesTerminal();
   if(gcodeFile)
   {
      gcodeFile.close();
   }
   return(SD.begin(BUILTIN_SDCARD));
}

bool Streamer::OpenDirectory(const char *path)
{
   File root = SD.open(path);
   if(root.isDirectory())
   {
      ListFiles(root);
      return(true);
   }
   return(false);
}

void Streamer::ListFiles(File &dir)
{
   ClearFilesTerminal();
   fileCount = 0;
   if(dir)
   {
      while(true)
      {
         File f = dir.openNextFile();
         if(!f)
         {
            if(fileCount > 0)
            {
               selectedFile = fileCount - 1; // To match the invertd list order of the display.
            }
            //Serial.print("Found ");Serial.print(fileCount);Serial.println(" files.");
            break;
         }
         else
         {
            //Serial.print(f.name()); Serial.print("  ");
            if(f.isDirectory() == false)
            {
               // Figure out how to get file's dates.
               //uint16_t fdate = 0, ftime = 0;
               //f.getCreateDateTime(&fdate, &ftime);
               //Serial.print(f.size()); //Serial.print(" "); Serial.print(fdate); Serial.print(" ");Serial.print(ftime);
               fileCount++;
               AddLineToFilesTerminal(f.name());
            }
            else
            {
               //Serial.print("/");
               // Add recursion here if needed.
               // For now, show only one directory at a time.
               //ListFiles(f);
            }
            //Serial.println();
            f.close();
         }
      }
      dir.close();
   }
}

bool Streamer::OpenFile(const char *name)
{
   if(gcodeFile)
   {
      gcodeFile.close();
   }

   gcodeFile = SD.open(name, FILE_READ);

   return(gcodeFile);
}

void Streamer::CloseFile()
{
   if(gcodeFile)
   {
      gcodeFile.close();
   }
}

//bool lastLineIncomplete = false;
//uint8_t lastProcessed = 0;

bool Streamer::ReadFile(char &c)
{
   bool ret(true);
   if(gcodeFile)
   {
      uint8_t processed(0);//lastProcessed);
      //while(true)
      {
         if(gcodeFile.available() == true)
         {
            if(charactersSent + processed < GRBL_RX_BUFFER_SIZE)
            {
               //uint8_t c(gcodeFile.read());
               if(gcodeFile.read(&c, 1) == -1)
               {
                  // Read error!!! Maybe file is corrup or card was removed!
               }
               else
               {
                  ++processed;
                  if(c == '\n')
                  {
                     // Complete line, store its character count.
                     //lastLineIncomplete = false;
                     //lastProcessed = 0;
                     grblQ.push(processed);
                     ret = false;
                     //break;
                  }
               }
            }
            else
            {
               // Incomplete line, GRBL buffer full
               //lastProcessed = processed;
               //lastLineIncomplete = true;
               ret = false;
               //break;
            }
         }
         else
         {
            // EOF
            //lastLineIncomplete = false;
            //lastProcessed = 0;
            grblQ.push(processed);
            ret = false;
            //break;
         }
      }
      charactersSent += processed;
   }
   else
   {
      ret = false;
   }
   return(ret);
}

void Streamer::Acknowledge()
{
   // Remove last acknowledged line length;
   charactersSent -= grblQ.pop();
}

void Streamer::onMessageChoiceMade(uint8_t s)
{
   MessageDialog::DialogStatesT state(static_cast<MessageDialog::DialogStatesT>(s));
   RefreshScreen();
   if(state == MessageDialog::eOk)
   {
      // Close dialog
      dialog->Close();
      delete dialog;
      dialog = nullptr;
   }
}

void Streamer::onOkCancelChoiceMade(uint8_t s)
{
   OkCancelDialog::DialogStatesT state(static_cast<OkCancelDialog::DialogStatesT>(s));
   Serial.print("State1: ");Serial.println(s);
   Serial.print("State2: ");Serial.println(streamerState);
   Serial.flush();
   if(state == OkCancelDialog::eOk)
   {
      if(streamerState == CONFIRM)
      {
         streamerState = ARM_FILE;
      }
      else if(streamerState == ARM_FILE)
      {
         uint8_t nameIndex((tROWS - 1) - selectedFile);
         if(OpenFile(filesTerminal[nameIndex]) == true)
         {
            // Let's go!
            streamerState = STREAM;
         }
         else
         {
            streamerState = FILE_ERROR;
         }
      }
      else if(streamerState == STREAM)
      {
         // Hold
         streamerState = HOLD;
      }
      else if(streamerState == HOLD)
      {
         streamerState = CANCEL;
      }
      else if(streamerState == CANCEL)
      {
         // Send end of program code to GRBL
         // Flush buffers and close files.
         // Reset state machine.
         streamerState = LIST;
      }
   }
   else
   {
      UpdateFilesTerminal();
      streamerState = SELECT;
   }
}

void Streamer::Select(bool pressed)
{
      // Let dialog handle this.
   if(dialog != nullptr)
   {
      dialog->Select(pressed);
      if(pressed == false)
      {
         Serial.println("Close");
         Serial.flush();
         dialog->Close();
         delete dialog;
         dialog = nullptr;
         RefreshScreen();
         Serial.println("done!");
         Serial.flush();
      }
   }
   else if(streamerState == SELECT)
   {
      if(pressed == true)
      {

      }
      else
      {
         uint8_t nameIndex((tROWS - 1) - selectedFile);
         Serial.print("Picked: "); Serial.print(selectedFile); Serial.print(" "); Serial.println(filesTerminal[nameIndex]);
         streamerState = CONFIRM;
      }
   }
}

void Streamer::Select(int8_t steps)
{
   // Let dialog handle this.
   if(dialog != nullptr)
   {
      dialog->Select(steps);
   }
   else
   {
      steps *= -1; // Invert direction for file list.
      if(selectedFile + steps >= fileCount)
      {
         selectedFile = fileCount - 1;
      }
      else if(selectedFile == 0 && steps < 0)
      {
         selectedFile = 0;
      }
      else
      {
         selectedFile += steps;
      }
      uint8_t nameIndex((tROWS - 1) - selectedFile);
      Serial.print("Selected: "); Serial.print(selectedFile); Serial.print(" "); Serial.println(filesTerminal[nameIndex]);

      UpdateFilesTerminal();
   }
}

void Streamer::HandleSelectorStateChange(uint8_t now, uint8_t was)
{
   if(now == FILES)
   {
      // Switched into files mode.
      streamerState = LIST;
   }
   else if(was == FILES)
   {
      // Switched out of files mode.
      ClearFilesTerminal();
      UpdateFilesTerminal();
      tft.updateScreenAsync();
      streamerState = OFF;
   }
   Serial.print("State3: ");Serial.println(streamerState);
   Serial.flush();
}

void Streamer::AddLineToFilesTerminal(const char *line)
{
   // Wrap long lines.
   uint8_t breaks((strlen(line) / (tCOLS - 1)) + 1);

   for(uint8_t r = breaks; r < tROWS; ++r)
   {
      strncpy(filesTerminal[r - breaks], filesTerminal[r], tCOLS - 1);
   }
   
   for(uint8_t r = 0; r < breaks; ++r)
   {
      strncpy(filesTerminal[(tROWS - breaks) + r], &line[r * (tCOLS - 1)], tCOLS);
   }
}

void Streamer::ClearFilesTerminal()
{
  memset(filesTerminal, '\0', sizeof(filesTerminal[0][0]) * tROWS * tCOLS);
}

void Streamer::UpdateFilesTerminal()
{
   {
      tft.setTextSize(1);

      int16_t x(0), y(48);
      uint16_t cw, ch;
      //uint8_t rmax((tft.height() - y) / 8); // 34
      uint8_t rend(0);//tROWS - rmax);

      tft.getTextBounds("0", 0, 0, &x, &y, &cw, &ch);
      tft.fillRect(0, 48, cw * tCOLS - 1, tft.height() - 48, ILI9488_BLACK);
      tft.drawRect(0, 48, cw * tCOLS - 1, tft.height() - 48, ILI9488_OLIVE);
      int8_t r = tROWS - 1;
      y = (tft.height() - ch);
      tft.setTextColor(ILI9488_WHITE, ILI9488_BLACK);
      char label[tCOLS] = { '\0' };
      while(r >= 0)
      {
         if(filesTerminal[r][0] != '\0' && r >= rend)
         {
            sprintf(label, "%02d - %s", r, filesTerminal[r]);
            if(r == ((tROWS - 1) - selectedFile))
            {
               tft.setTextColor(ILI9488_BLACK, ILI9488_WHITE);
               Text(0, y, label);//filesTerminal[r]);
               tft.setTextColor(ILI9488_WHITE, ILI9488_BLACK);
            }
            else
            {
               Text(0, y, label);//filesTerminal[r]);
            }
         }
         --r;
         y -= ch;
      }
   }
}

void Streamer::HandleConfirmationDialog()
{
   if(dialog == nullptr)
   {
      dialog = new OkCancelDialog((tft.width() - 220) / 2, (tft.height() - 160) / 2, 220, 160,
                                     "Arm selected file for streaming?", OkCancelDialog::eCancel);
      dialog->onChoiceConnect(onOkCancelChoiceMadeConnect);
      dialog->Draw();
   }
   else if(streamerState == CONFIRM)
   {
      ;
   }
}

void Streamer::HandleArmFileDialog()
{
   if(dialog == nullptr)
   {
      dialog = new OkCancelDialog((tft.width() - 220) / 2, (tft.height() - 160) / 2, 220, 160,
                                     "Run armed file?", OkCancelDialog::eCancel);
      dialog->onChoiceConnect(onOkCancelChoiceMadeConnect);
      dialog->Draw();
   }
}

void Streamer::MonitorStreaming()
{
   if(dialog == nullptr)
   {
      dialog = new OkCancelDialog((tft.width() - 220) / 2, (tft.height() - 160) / 2, 220, 160,
                                     "Pause?", OkCancelDialog::eCancel);
      dialog->onChoiceConnect(onOkCancelChoiceMadeConnect);
      dialog->Draw();
   }
}

void Streamer::HandleHoldDialog()
{
   if(dialog == nullptr)
   {
      dialog = new OkCancelDialog((tft.width() - 220) / 2, (tft.height() - 160) / 2, 220, 160,
                                     "Resume?", OkCancelDialog::eCancel);
      dialog->onChoiceConnect(onOkCancelChoiceMadeConnect);
      dialog->Draw();
   }
}

void Streamer::ProcessStremCancel()
{
   if(dialog == nullptr)
   {
      dialog = new OkCancelDialog((tft.width() - 220) / 2, (tft.height() - 160) / 2, 220, 160,
                                     "End Program?", OkCancelDialog::eCancel);
      dialog->onChoiceConnect(onOkCancelChoiceMadeConnect);
      dialog->Draw();
   }
}

void Streamer::DisplayError()
{
   if(dialog == nullptr)
   {
      const char *msg(nullptr);
      // Draw error dialog.
      if(streamerState == SD_ERROR)
      {
         msg = ("SD Card error.");
      }
      else if(streamerState == FILE_ERROR)
      {
         msg = ("File I/O error.");
      }
      else
      {
         msg = ("GC error.");
      }
      Serial.println(msg);
      dialog = new MessageDialog((tft.width() - 220) / 2, (tft.height() - 160) / 2, 220, 160,
                                  msg);
      dialog->onChoiceConnect(onMessageChoiceConnect);
      dialog->Draw();
   }
}

void Streamer::Update()
{
   switch(streamerState)
   {
      case(OFF):
         break;
      case(LIST):
         if(begin() == true)
         {
            if(OpenDirectory("/") == true)
            {
               UpdateFilesTerminal();
               tft.updateScreenAsync();
               streamerState = SELECT;
            }
            else
            {
               streamerState = FILE_ERROR;
            }
         }
         else
         {
            streamerState = SD_ERROR;
         }
         break;
      case(SELECT):
         break;
      case(CONFIRM):
         HandleConfirmationDialog();
         break;
      case(ARM_FILE):
         HandleArmFileDialog();
         break;
      case(STREAM):
         MonitorStreaming();
         break;
      case(HOLD):
         HandleHoldDialog();
         break;
      case(CANCEL):
         ProcessStremCancel();
         break;
      case(SD_ERROR):
      case(FILE_ERROR):
            DisplayError();
            streamerState = FAILED_STATE;
         break;
      default:
         break;
   }
}