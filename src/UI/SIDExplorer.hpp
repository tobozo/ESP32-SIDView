/*\

  ESP32-SIDView
  https://github.com/tobozo/ESP32-SIDView

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

\*/

#pragma once

#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-label"
#pragma GCC diagnostic ignored "-Wformat-truncation"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wformat"

#include "../config.h"

#include "../assets/assets.h"
#include "../Cache/SIDCacheManager.hpp"
#include "../Cache/SIDVirtualFolders.hpp"
#include "../helpers/Downloader/SIDArchiveManager.hpp"
#include "../helpers/HID/HID_Common.hpp"
#include "../helpers/NVS/prefs.hpp"
#include "../helpers/WiFiManager/WiFiManagerHooks.hpp"
#include "../Oscillo/OscilloView.hpp"
#include "../UI/ScrollableItem.hpp"


namespace SIDView
{

  static MD5Archive HSVC =
  {
    HVSC_VERSION, // just a fancy name
  //  nullptr,      // must be URL to a tgz file if not null
    HVSC_VERSION  // dotfile name will be derivated from that
  };

  static MD5FileConfig MD5Config =
  {
    &SID_FS,          // SD
    &HSVC,            // High Voltage SID Collection info
    SID_FOLDER,       // /sid
    MD5_FOLDER,       // /md5
    MD5_FILE,         // /md5/Songlengths.full.md5
    MD5_FILE_IDX,     // /md5/Songlengths.full.md5.idx
    MD5_INDEX_LOOKUP, // MD5_RAW_READ, MD5_INDEX_LOOKUP, MD5_RAINBOW_LOOKUP
    nullptr           // callback function for progress when the init happens, overloaded later
  };

  static TaskHandle_t renderVoicesTaskHandle = NULL;
  static TaskHandle_t renderUITaskHandle = NULL;
  static TaskHandle_t HIDTaskHandle = NULL;

  //static HIDControls LastHIDAction = -1;
  static unsigned long lastpush = millis();
  static unsigned long lastEvent = millis();
  static int debounce = 200;

  static uint8_t maxVolume = 10; // value must be between 0 and 15
  static bool indexTaskRunning = false;

  static uint32_t ramSize = 0; // populated at boot for statistics

  #if defined SID_DOWNLOAD_ARCHIVES
    static SID_Archive_Checker *SidArchiveChecker = nullptr;
  #endif

  static NVSPrefs PlayerPrefs;

  static folderItemType_t currentBrowsingType = F_PLAYLIST;
  static sidEvent lastSIDEvent = SID_END_PLAY;
  static sidEvent nextSIDEvent = SID_END_PLAY;


  class SIDExplorer
  {

    public:

      ~SIDExplorer();
      SIDExplorer( MD5FileConfig *_cfg=&SIDView::MD5Config );
    private:

      void setup();
      void loop();
      // Main player task
      static void mainTask( void *param );
      // HID scan task
      static void HIDTask( void * param );

      MD5FileConfig *cfg      = nullptr;
      SID_Meta_t    *track    = nullptr;

      uint16_t    lastItemCursor[16] = {0}; // click/select history

      HIDControls HIDAction          = HID_NONE;
      HIDControls LastHIDAction      = HID_NONE;

      #ifdef BOARD_HAS_PSRAM
        size_t maxitems = 0; // 0 = no limit
      #else
        size_t maxitems = 1024; // Limiting folders to %d items due to lack of psram
      #endif

      bool explorer_needs_redraw = false; // sprite ready to be pushed

      unsigned long inactive_since = 0; // UI debouncer
      unsigned long inactive_delay = 5000;
      unsigned long sleep_delay    = 600000; // sleep after 600 seconds

      void processHID();
      void updatePagination();
      void animateView();

      void handleHIDDown();
      void handleHIDUp();
      void handleHIDRight();
      void handleHIDLeft();

      void handleHIDPlayStop();       // action 0
      void handleHIDToggleLoopMode(); // action 1
      void handleHIDVolumeDown();     // action 2
      void handleHIDVolumeUp();       // action 3

      void handleTool();
      void handleFolder();
      void handlePlaylist();
      void handleSIDFile();

      void handleAutoPlay();
      void nextInPlaylist();
      void prevInPlaylist();

      void setUIPlayerMode( loopmode mode );

      bool getPaginatedFolder( size_t offset = 0, bool force_regen = false );
      size_t getFolder( size_t offset, size_t maxitems, bool force_regen );

      void resetPathCache();
      void resetSpreadCache();

      void runDeepsidModal();
      void runHVCSCModal();

      void checkArchive( bool force_check = false );

      // callbacks
      static void eventCallback( SIDTunesPlayer* player, sidEvent event );
      static void addFolderElement( const char* fullpath, folderItemType_t type );

      #if defined ENABLE_HVSC_SEARCH
        void handleHIDSearch();
        bool drawSearchPage( char* searchstr, bool search=false, size_t pagenum=0, size_t maxitems=8 );
        // keywords indexing task
        static void buildSIDWordsIndexTask( void* param );
        static bool keywordsTicker( const char* title, size_t totalitems );
        static void keyWordsProgress( size_t current, size_t total, size_t words_count, size_t files_count, size_t folders_count  );
        void resetWordsCache();
      #endif

      #if defined SID_DOWNLOAD_ARCHIVES
        void updateMd5File();
      #endif

  };

}; // end namespace SIDView

using SIDView::SIDExplorer; // export class to global namespace

