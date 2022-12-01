#include <cstring>
#include "Config.h"
#include "GcodeStreaming.h"
#include "ILI9488_t3.h"

#ifdef USE_GDB
#include <Arduino.h>
#include "TeensyDebug.h"
#pragma GCC optimize ("O0")
#endif


// This buffers data from SD card.
#define OUT_RING_BUFFER_SIZE 256//(GRBL_RX_BUFFER_SIZE * 2)
#define OUT_BUFFER_MASK (OUT_RING_BUFFER_SIZE - 1)
uint8_t outBuffer[OUT_RING_BUFFER_SIZE] = { '\0' };
uint16_t outHead = 0;
uint16_t outTail = 0;
uint16_t outCount = 0;
bool outData = false;

void outReset()
{
   outHead = outTail = outCount = 0;
   outData = false;
}

void outPush(const char *in)
{
   while(*in)
   {
      outBuffer[outHead] = *in++;
      outHead = (outHead + 1) & OUT_BUFFER_MASK;
      outCount++;
   }
   outData = true;
}

uint8_t outPop()
{
   uint8_t o(outBuffer[outTail]);
   outTail = (outTail + 1) & OUT_BUFFER_MASK;
   outCount--;
   return(o);
}

unsigned int outAvailable()
{
   return ((unsigned int)(OUT_RING_BUFFER_SIZE + outHead - outTail)) % OUT_RING_BUFFER_SIZE;
}

unsigned int outSpaceAvailable()
{
   if(outHead >= outTail)
   {
      return OUT_RING_BUFFER_SIZE - 1 - outHead + outTail;
   }
   return outTail - outHead - 1;
}

//#define OFF 0
//#define LIST 1
//#define SELECT 2
//#define CONFIRM 3
//#define ARM_FILE 4
//#define STREAM 5
//#define HOLD 6
//#define CANCEL 7
//#define COMPLETE 8
//#define SD_ERROR 200
//#define FILE_ERROR 201
//#define LINE_TOO_LONG 202
//#define FAILED_STATE 255

char dbg[128] = { '\0' };

elapsedMillis streamingGRBLUpdate;

Streamer GC;

Streamer::Streamer()
{
   onOkCancelChoiceMadeConnect = std::bind(&Streamer::onOkCancelChoiceMade, this, std::placeholders::_1);
   onMessageChoiceConnect  = std::bind(&Streamer::onMessageChoiceMade, this, std::placeholders::_1);
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

   totalLinesInFile = 0;
   lineCount = 0;
   gcodeFile = SD.open(name, FILE_READ);

   while(gcodeFile.available())
   {
      memset(dbg, '\0', 128);
      int32_t b(gcodeFile.read(dbg, 127));
      while(b > 0)
      {
         if(dbg[b - 1] == '\n')
         {
            ++totalLinesInFile;
         }
         b--;
      }
#ifndef USE_GDB
      //Serial.print(dbg);
#endif
   }
   sprintf(dbg, "Found: %lu lines in file\n", totalLinesInFile);
   AddLineToCommandTerminal(dbg);
   dbg[0] = '\0';
   gcodeFile.seek(0);
   charactersSent = 0;
   loadedLineLength = 0;
   charactersPulledFromLine = 0;
   return(gcodeFile);
}

void Streamer::CloseFile()
{
   if(gcodeFile)
   {
      gcodeFile.close();
   }
}
int lc = 0;
char out[128] = { '\0' };
int8_t Streamer::ReadFile()
{
   int8_t ret(1);
   uint16_t dlen(strlen(dbg));
   if(dlen > 0)
   {
      // waiting for space mate.
      if(outSpaceAvailable() > dlen)
      {
         outPush(dbg);
         dbg[0] = '\0';
         dlen = 0;
      }
   }
   else if(gcodeFile && dlen == 0)
   {
      char B[1] = { '\0' };
      int32_t count(0);
      bool skip = false;
      while(true)
      {
         if(count > 127)
         {
            // Buffer overrun, line too long!
            ret = -2;
            break;
         }
         int32_t r(gcodeFile.read(B, 1));
         if(r == -1)
         {
            ret = -1;
            break;
         }
         else if(r == 0)
         {
            // EOF
            ret = 0;
            gcodeFile.close();
            break;
         }
         else if(B[0] != ' ' && B[0] != '\r' && B[0] != '%')
         {
            if(B[0] == '(')
            {
               skip = true;
            }
            else if(B[0] == ')')
            {
               skip = false;
            }
            else if(skip == false)
            {
               dbg[count] = toupper(B[0]);
               // Read a full line
               if(B[0] == '\n')
               {
                  dbg[count + 1] = '\0';
                  if(dbg[0] == '\n')//strlen(dbg) > 1)
                  {
                     dbg[0] = '\0';
                     //if(totalLinesInFile > 0)
                     {
                        // Take off empty lines since those are not processed.
                        //--totalLinesInFile;
                        // Actually better to count it as processed. This way
                        // it is easier to correlate a processed line number
                        // to an entry in a file.
                        ++lineCount;
                     }
                     //outPush(dbg);
                  }
                  //Serial.print("\rIN: ");Serial.print(strlen(dbg));Serial.print(", ");Serial.print(dbg);
                  //Serial.flush();
                  break;
               }
               else if(B[0] == ';')
               {
                  dbg[count] = '\n';
                  dbg[count + 1] = '\0';
                  if(strlen(dbg) > 1)
                  {
                     if(dbg[0] == '\n')
                     {
                        dbg[0] = '\0';
                        // See note above.
                        ++lineCount;
                     }
                     // Remove line comment and add new g code line
                     //outPush(dbg);
                  }
                  break;
               }
               ++count;
            }
         }
         
      }
   }

   if(out[0] == '\0')
   {
      // Pull a line out and make room
      uint8_t x(0);
      while(outAvailable() && x < 127)
      {
         out[x] = outPop();
         if(out[x] == '\n')
         {
            ++x;
            out[x] = '\0';
            // Got the line
            break;
         }
         ++x;
      }
      //Serial.print("\rMI: ");Serial.print(strlen(out));Serial.print(", ");Serial.print(x);Serial.print(", ");Serial.print(out);
      //Serial.flush();
      loadedLineLength = x;
      charactersPulledFromLine = 0;
   }

   return(ret);
}

uint16_t Streamer::Available()
{
   uint16_t ret(0);

   if(charactersSent + loadedLineLength < (GRBL_RX_BUFFER_SIZE))
   {
      if(lineCount < totalLinesInFile)
      {
         ret = loadedLineLength - charactersPulledFromLine;
      }
      else
      {
         streamerState = COMPLETE;
      }
      
   }
   return(ret);
}

uint8_t Streamer::Read()
{
   uint8_t c = out[charactersPulledFromLine];
   charactersPulledFromLine++;
   if(c == '\n')
   {
      out[charactersPulledFromLine] = '\0';
      //Serial.print("\rOU: ");Serial.print(strlen(out));Serial.print(", ");Serial.print(out);
      //Serial.flush();
      grblQ.push(loadedLineLength);
      //Serial.print("\rQ: ");Serial.print(grblQ.size());Serial.print(", ");Serial.println(grblQ.sum());
      charactersSent += loadedLineLength;
      ++lineCount;
      sprintf(out, "progress: %u / %lu", lineCount, totalLinesInFile);
      AddLineToCommandTerminal(out);
      //Serial.print("STR: ");Serial.print(charactersReadInLine);Serial.print(", "), Serial.print(charactersSent);Serial.print(", ");Serial.println(grblQ.size());Serial.flush();
      charactersPulledFromLine = 0;
      loadedLineLength = 0;
      out[0] = '\0';
   }
   return(c);
}

void Streamer::Acknowledge()
{
   // Remove last acknowledged line length;
   charactersSent -= grblQ.pop();
   //Serial.print("ACK: ");Serial.println(charactersSent);
}

void Streamer::onMessageChoiceMade(uint8_t s)
{
   MessageDialog::DialogStatesT state(static_cast<MessageDialog::DialogStatesT>(s));
   if(state == MessageDialog::eOk)
   {
      RefreshScreen();
   }
}

const char *Streamer::GetSelectedFileName()
{
   uint8_t nameIndex((tROWS - 1) - selectedFile);
   return(filesTerminal[nameIndex]);
}

void Streamer::onOkCancelChoiceMade(uint8_t s)
{
   OkCancelDialog::DialogStatesT state(static_cast<OkCancelDialog::DialogStatesT>(s));
   //sprintf(dbg, "State2a: %x, %xd\n", &streamerState, this);
   //Serial.print("State2: ");Serial.println(streamerState);
   //Serial.println(dbg);
   //Serial.flush();
   if(state == OkCancelDialog::eOk)
   {
      if(streamerState == CONFIRM)
      {
         streamerState = ARM_FILE;
         char msg[tCOLS] = "Arming: ";
         strncat(msg, GetSelectedFileName(), tCOLS - 1);
         AddLineToCommandTerminal(msg);
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
      streamerState = SELECT;
   }
      //Serial.print("State5: ");Serial.println(streamerState);
}

void Streamer::Select(bool pressed)
{
      // Let dialog handle this.
   //Serial.print("Select1: ");Serial.println(pressed);
   if(dialog != nullptr)
   {
      //Serial.print("State4: ");Serial.println(streamerState);
      //sprintf(dbg, "State4a: %x, %xd\n", &streamerState, this);
      //Serial.println(dbg);
      dialog->Select(pressed);
      if(pressed == false)
      {
         RefreshScreen();
         UpdateFilesTerminal();
         //Serial.println("Close");
         //Serial.flush();
         dialog->Close();
         delete dialog;
         dialog = nullptr;
         //Serial.println("done!");
         //Serial.flush();
      }
      //Serial.print("State6: ");Serial.println(streamerState);
   }
   else if(streamerState == SELECT)
   {
      //Serial.print("Select2: ");Serial.println(pressed);
      if(pressed == true)
      {

      }
      else
      {
         //uint9_t nameIndex((tROWS - 1) - selectedFile);
         //Serial.print("Picked: "); Serial.print(selectedFile); Serial.print(" "); Serial.println(filesTerminal[nameIndex]);
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
      //uint8_t nameIndex((tROWS - 1) - selectedFile);
      //Serial.print("Selected: "); Serial.print(selectedFile); Serial.print(" "); Serial.println(filesTerminal[nameIndex]);

      UpdateFilesTerminal();
   }
}

void Streamer::HandleSelectorStateChange(bool selected)
{
   if(selected == true)
   {
      // Switched into files mode.
      streamerState = LIST;
   }
   else
   {
      // Switched out of files mode.
      ClearFilesTerminal();
      UpdateFilesTerminal();
      if(dialog != nullptr)
      {
         dialog->Close();
         delete dialog;
         dialog = nullptr;
      }
      RefreshScreen();
      tft.updateScreenAsync();
      streamerState = OFF;
   }
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

uint16_t Streamer::RingBufferSpaceAvailable() const
{
   return(OUT_RING_BUFFER_SIZE - outCount);
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
   // Pull data from SD card into memory ring buffer
   int8_t r(ReadFile());
   if(r > 0)
   {
      // Draw progress on screen on a timer.
   }
   else
   {
      if(r == 0)
      {
         // EOF
         if(dialog == nullptr)
         {
            dialog = new MessageDialog((tft.width() - 220) / 2, (tft.height() - 160) / 2, 220, 160,
                                        "End of file reached!");
            dialog->onChoiceConnect(onMessageChoiceConnect);
            dialog->Draw();
            //streamerState = COMPLETE;
         }
      }
      else if(r == -2)
      {
         streamerState = LINE_TOO_LONG;
         //DisplayError();
      }
      else
      {
         // Read error
         streamerState = FILE_ERROR;
         //DisplayError();
      }
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
      else if(streamerState == LINE_TOO_LONG)
      {
         msg = "GCODE line too long!";
      }
      else
      {
         msg = ("GC error.");
      }
      //Serial.println(msg);
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
      case(LINE_TOO_LONG):
            DisplayError();
            streamerState = FAILED_STATE;
         break;
      default:
         break;
   }
}