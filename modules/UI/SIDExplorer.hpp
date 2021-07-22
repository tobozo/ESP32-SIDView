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

#ifndef _SID_EXPLORER_H_
#define _SID_EXPLORER_H_


#define SID_PLAYER
#define SID_INSTRUMENTS
#include <SID6581.h> // https://github.com/hpwit/SID6581
static SIDTunesPlayer *sidPlayer = nullptr;

#if defined HID_TOUCH
  #define HAS_TOUCH
#endif

#include <ESP32-Chimera-Core.h>  // https://github.com/tobozo/ESP32-Chimera-Core
#define tft M5.Lcd // syntax sugar
#define DEST_FS_USES_SD
#include <ESP32-targz.h>         // https://github.com/tobozo/ESP32-Targz
#ifdef SD_UPDATABLE
  // SD-loadability
  #include "../SDUpdater/SDUpdaterHooks.h"
#endif


#if defined HID_XPAD
  #include "../HID/HID_XPad.h"/*
  #define initHID initXPad
  #define readHID readXpad
  #define beforeHIDRead beforeXpadRead
  #define afterHIDRead afterXpadRead*/
#endif
#if defined HID_USB
  #include "../HID/HID_USB.h"/*
  #define initHID HIDUSBInit
  #define readHID readUSBHIDKeyboarD
  #define beforeHIDRead HIDBeforeRead
  #define afterHIDRead HIDAfterRead*/
#endif
#if defined HID_TOUCH
  #include "../HID/HID_Touch.h"/*
  #define initHID initTouch
  #define readHID readTouch
  #define beforeHIDRead beforeTouchRead
  #define afterHIDRead afterTouchRead*/
#endif
#if defined HID_SERIAL
  #include "../HID/HID_Serial.h"/*
  #define initHID initSerial
  #define readHID readSerial
  #define beforeHIDRead beforeSerialRead
  #define afterHIDRead afterSerialRead*/
#endif


// lazily borrowing gnu_basename() from SID_HVSC_Indexer.h
#define gnu_basename BufferedIndex::gnu_basename

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
//  MD5_URL,          // https://www.prg.dtu.dk/HVSC/C64Music/DOCUMENTS/Songlengths.md5
  MD5_INDEX_LOOKUP, // MD5_RAW_READ, MD5_INDEX_LOOKUP, MD5_RAINBOW_LOOKUP
  nullptr           // callback function for progress when the init happens, overloaded later
};

#include "../assets/assets.h"
#include "../NVS/prefs.hpp"
#include "../helpers/UI_Utils.h"
#include "../helpers/intro_sound.h"
#include "../Cache/SIDCacheManager.hpp"
#include "../Downloader/SIDArchiveManager.hpp"
#include "../Oscillo/OscilloView.hpp"
#include "SIDVirtualFolders.h"

static TaskHandle_t renderVoicesTaskHandle = NULL;
static TaskHandle_t renderUITaskHandle = NULL;

int lastresp = -1;
unsigned long lastpush = millis();
unsigned long lastEvent = millis();
int debounce = 200;

uint8_t maxVolume = 10; // value must be between 0 and 15
static bool render = true;
static bool stoprender = false;
static bool indexTaskRunning = false;

static size_t diskTickerWordWrap; // for blinking an activity icon
static unsigned long diskTickerMsec;
static uint32_t ramSize = 0; // populated at boot for statistics

static SID_Archive_Checker *SidArchiveChecker = nullptr;
static OscilloView **oscViews = nullptr;
static NVSPrefs PlayerPrefs;
static ScrollableItem *scrollableText = new ScrollableItem(); //nullptr;
static folderItemType_t currentBrowsingType = F_PLAYLIST;
static SongCacheManager *SongCache = nullptr;
static sidEvent lastSIDEvent = SID_END_PLAY;
static sidEvent nextSIDEvent = SID_END_PLAY;


class SIDExplorer
{

  public:

    ~SIDExplorer();
    SIDExplorer( MD5FileConfig *_cfg );
    int32_t explore();
    static void mainTask( void *param );

  private:

    MD5FileConfig *cfg = nullptr;
    SID_Meta_t    *track = nullptr;
    fs::FS        *fs = nullptr;
    TFT_eSprite   *spriteText = nullptr;
    TFT_eSprite   *spriteHeader = nullptr;
    TFT_eSprite   *spritePaginate = nullptr;
    uint16_t      *sptr = nullptr; // pointer to the sprite
    char          *lineText = nullptr;

    uint16_t spriteWidth           = 0;
    uint16_t spriteHeight          = 0;
    uint16_t lineHeight            = 0;
    uint16_t minScrollableWidth    = 0;
    int TouchPadHeight             = 64;
    int32_t     itemClicked        = -1;
    uint16_t    itemsPerPage       = 8;
    uint16_t    lastItemCursor[16] = {0};
    uint8_t     folderDepth        = 0;
    int         positionInPlaylist = -1;
    loopmode    playerloopmode     = SID_LOOP_OFF;
    const char* loopmodeicon       = (const char*)iconloop_jpg;
    size_t      loopmodeicon_len   = iconloop_jpg_len;

    #ifdef BOARD_HAS_PSRAM
      size_t maxitems = 0; // 0 = no limit
    #else
      size_t maxitems = 1024;
      //Serial.printf("Limiting folders to %d items due to lack of psram\n", maxitems );
    #endif

    bool exitloop              = false; // exit render looptask
    bool firstRender           = true;  // for ui loading effect
    bool explorer_needs_redraw = false; // sprite ready to be pushed
    bool adsrenabled           = false; // adsr view

    unsigned long inactive_since = millis(); // UI debouncer
    unsigned long inactive_delay = 5000;
    unsigned long sleep_delay    = 600000; // sleep after 600 seconds

    void sidRunner(); // init the SID
    void sidSleep();

    void initSprites();
    void deInitSprites();

    void processHID();
    void handleAutoPlay();
    void nextInPlaylist();
    void prevInPlaylist();
    void setUIPlayerMode( loopmode mode );

    void setupPagination();
    bool getPaginatedFolder( const char* folderName, size_t offset = 0, size_t maxitems=0, bool force_regen = false );
    void drawPaginaged();
    void infoWindow( const char* text );
    void animateView();
    void drawHeader();
    void loadingScreen();

    void drawToolPage( const char* title, const unsigned char* img = icontool_jpg, size_t img_len = icontool_jpg_len, const char* text[] = {nullptr}, size_t linescount = 0, uint8_t ttDatum = TC_DATUM, uint8_t txDatum = TL_DATUM );
    bool drawSearchPage( char* searchstr, bool search=false, size_t pagenum=0, size_t maxitems=8 );

    void checkArchive( bool force_check = false );
    void resetPathCache();
    void resetSpreadCache();
    void resetWordsCache();
    void updateMd5File();

    // keywords indexing task
    static void buildSIDWordsIndexTask( void* param );

    // UI drawing static methods
    static void drawVolumeIcon( int16_t x, int16_t y, uint8_t volume );
    static void drawSongHeader( SIDExplorer* mySIDExplorer, char* songNameStr, char* filenumStr, bool &scrolltext );
    static void drawSongProgress( unsigned long currentprogress, uint8_t deltaMM, uint8_t deltaSS );
    static void drawVoices( void* param );
    static void drawCursor( bool state, int32_t cursorX, int32_t cursorY, uint8_t fontWidth, uint8_t fontHeight, uint32_t fgcolor, uint32_t bgcolor );
    static void blinkCursor( int32_t cursorX, int32_t cursorY, uint8_t fontWidth, uint8_t fontHeight, uint32_t fgcolor, uint32_t bgcolor );

    // callbacks
    static void eventCallback( SIDTunesPlayer* player, sidEvent event );
    static void addFolderElement( const char* fullpath, folderItemType_t type );
    static bool keywordsTicker( const char* title, size_t totalitems );
    static void keyWordsProgress( size_t current, size_t total, size_t words_count, size_t files_count, size_t folders_count  );
    static bool diskTicker( const char* title, size_t totalitems );
    static void MD5ProgressCb( size_t current, size_t total );

};


  // dafuq Arduino IDE ???
  #ifdef ARDUINO
    #include "SIDExplorer.cpp"
  #endif



#endif
