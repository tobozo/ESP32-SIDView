/*\

  ESP32-SIDView
  https://github.com/tobozo/ESP32-SIDView

  This project is an attempt to provide a visual dimension to chiptune music and
  is heavily inspired by SIDWizPlus (https://github.com/maxim-zhao/SidWizPlus).

  You'll need either a genuine SID6581 or any clone (FPGASID, TeenSID, SwinSID, etc)
  and a PCB handling the communication with the ESP32 along with the audio output.
  See the /pcb/ folder of this project for more details about the hardware.

  MIT License

  Copyright (c) 2020 tobozo

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  -----------------------------------------------------------------------------

  Library dependencies (all available from the Arduino Library Manager):

  - https://github.com/hpwit/SID6581
  - https://github.com/lovyan03/LovyanGFX
  - https://github.com/tobozo/ESP32-Targz
  - https://github.com/tobozo/ESP32-Chimera-Core
  - https://github.com/tobozo/M5Stack-SD-Updater


\*/

#include "config.h"

SIDExplorer* SidViewer = nullptr;

void setup()
{

  M5.begin(true, true); // begin both TFT and SD
  tft.setRotation( 2 ); // portrait mode !

  #ifdef SD_UPDATABLE
    setupButtonsStyle(); // set styles first (default it 320x240 landscape)
    setAssertTrigger( &myAssertStartUpdateFromButton ); // use my own buttons combination at boot
    // checkMenuStickyPartition(); // copy self to SD Card as "/menu.bin" and OTA1 partition
    checkSDUpdater( SID_FS, MENU_BIN, 1000 ); // load existing (not this sketch) menu.bin
  #endif

  Serial.printf("SID Player UI: %d*%d\n", M5.Lcd.width(), M5.Lcd.height() );
  SidViewer = new SIDExplorer( &MD5Config );
}


void loop()
{
  SidViewer->explore();
  vTaskDelete( NULL );
}
