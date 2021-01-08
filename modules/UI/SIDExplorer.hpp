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


#include <ESP32-Chimera-Core.h>  // https://github.com/tobozo/ESP32-Chimera-Core
#define tft M5.Lcd // syntax sugar
#define DEST_FS_USES_SD
#include <ESP32-targz.h>         // https://github.com/tobozo/ESP32-Targz
#ifdef SD_UPDATABLE
  // SD-loadability
  #include "../SDUpdater/SDUpdaterHooks.h"
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

int lastresp = -1;
unsigned long lastpush = millis();
unsigned long lastEvent = millis();
int debounce = 200;

uint8_t maxVolume = 10; // value must be between 0 and 15
static bool render = true;
static bool stoprender = false;

static size_t diskTickerWordWrap; // for blinking an activity icon
static unsigned long diskTickerMsec;

static SIDTunesPlayer *sidPlayer = nullptr;
static SID_Archive_Checker *SidArchiveChecker = nullptr;
static OscilloView **oscViews = nullptr;
static NVSPrefs PlayerPrefs;
static ScrollableItem *scrollableText = new ScrollableItem(); //nullptr;
static folderItemType_t currentBrowsingType = F_PLAYLIST;
static SongCacheManager *SongCache = nullptr;
sidEvent lastSIDEvent = SID_END_PLAY;
sidEvent nextSIDEvent = SID_END_PLAY;



class SIDExplorer
{

  public:

    ~SIDExplorer();
    SIDExplorer( MD5FileConfig *_cfg );

    int32_t explore();

  private:

    MD5FileConfig    *cfg = nullptr;
    SID_Meta_t       *song = nullptr;
    fs::FS           *fs = nullptr;
    //M5Display        *sidLcd = nullptr;
    TFT_eSprite      *spriteText = nullptr;
    uint16_t         *sptr = nullptr; // pointer to the sprite
    char             *folderToScan = nullptr;
    char             *lineText = nullptr;
    char             *currentPath = nullptr;

    uint16_t spriteWidth        = 0;
    uint16_t spriteHeight       = 0;
    uint16_t lineHeight         = 0;
    uint16_t minScrollableWidth = 0;

    int32_t        itemClicked        = -1;
    const uint16_t itemsPerPage       = 8;
    uint16_t       itemCursor         = 0;
    uint16_t       lastItemCursor[16] = {0};
    uint8_t        folderDepth        = 0;
    int            positionInPlaylist = -1;

    uint16_t pageNum     = 0;
    uint16_t pageTotal   = 0;
    uint16_t pageStart   = 0;
    uint16_t pageEnd     = 0;
    uint16_t ydirnamepos = 0;
    uint16_t ydislistpos = 0;
    uint16_t yitempos    = 1;

    loopmode       playerloopmode     = SID_LOOP_OFF;

    const char*    loopmodeicon       = (const char*)iconloop_jpg;
    size_t         loopmodeicon_len   = iconloop_jpg_len;

    #ifdef BOARD_HAS_PSRAM
      size_t maxitems = 0; // 0 = no limit
    #else
      size_t maxitems = 1024;
      //Serial.printf("Limiting folders to %d items due to lack of psram\n", maxitems );
    #endif

    bool exitloop              = false;
    bool firstRender           = true;
    bool wire_begun            = false;
    bool explorer_needs_redraw = false;
    bool adsrenabled           = false;

    unsigned long inactive_since = millis();
    unsigned long inactive_delay = 5000;

    int readHID();
    void processHID();
    void handleAutoPlay();
    void nextInPlaylist();
    void prevInPlaylist();

    bool getPaginatedFolder( const char* folderName, size_t offset = 0, size_t maxitems=0, bool force_regen = false );
    bool getSortedFolder( const char* folderName, size_t maxitems=0, bool force_regen = false );

    void setUIPlayerMode( loopmode mode );
    void setupPagination();

    void drawPaginaged();
    void drawToolPage( const char* title );
    void infoWindow( const char* text );

    void animateView();
    void drawHeader();
    void loadingScreen();

    void resetPathCache();
    void resetSpreadCache();

    void updateMd5File();
    void checkArchive( bool force_check = false );

    void initSprites();
    void deInitSprites();


    static void drawVolumeIcon( int16_t x, int16_t y, uint8_t volume )
    {
      tft.fillRect(x, y, 8, 8, C64_DARKBLUE );
      if( volume == 0 ) {
        // speaker off icon
        tft.drawJpg( soundoff_jpg, soundoff_jpg_len, x, y );
      } else {
        // draw volume
        for( uint8_t i=0;i<volume;i+=2 ) {
          tft.drawFastVLine( x+i/2, y+7-i/2, i/2, i%4==0 ? C64_LIGHTBLUE : TFT_WHITE );
        }
      }
    }


    static void drawSongHeader( SIDExplorer* mySIDExplorer, char* songNameStr, char* filenumStr, bool &scrolltext )
    {

      mySIDExplorer->spriteText->fillSprite(C64_DARKBLUE);
      mySIDExplorer->spriteText->setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
      mySIDExplorer->spriteText->setFont( &Font8x8C64 );
      mySIDExplorer->spriteText->setTextSize(1);

      snprintf( songNameStr, 255, " %s by %s (c) %s ", sidPlayer->getName(), sidPlayer->getAuthor(), sidPlayer->getPublished() );
      sprintf( filenumStr, " S:%-2d  T:%-3d ", sidPlayer->currentsong+1, SongCache->CacheItemNum+1 );

      tft.startWrite();

      tft.drawJpg( (const uint8_t*)mySIDExplorer->loopmodeicon, mySIDExplorer->loopmodeicon_len, 0, 16 );
      drawVolumeIcon( tft.width()-8, 16, maxVolume );
      uint16_t scrollwidth = tft.width();
      scrollwidth/=8;
      scrollwidth*=8;
      if( scrollwidth < tft.textWidth( songNameStr ) ) {
        //Serial.println("Reset scroll");
        if( !scrolltext ) {
          scrollableText->setup( songNameStr, 0, 32, scrollwidth, tft.fontHeight(), 300, true );
          scrolltext = true;
        }
      } else {
        scrolltext = false;
      }
      tft.setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
      tft.setTextDatum( TL_DATUM );
      tft.setFont( &Font8x8C64 );
      tft.drawString( filenumStr, 12, 16 );

      tft.endWrite();

      if( !scrolltext ) {
        mySIDExplorer->spriteText->setTextDatum( TL_DATUM );
        mySIDExplorer->spriteText->drawString( songNameStr, 16, 0 );
      }
      mySIDExplorer->spriteText->pushSprite(0,32);
    }


    static void drawSongProgress( unsigned long currentprogress, uint8_t deltaMM, uint8_t deltaSS )
    {

      char timeStr[12] = {0};
      uint32_t len_light = (currentprogress*tft.width())/100;
      uint32_t len_dark  = tft.width()-len_light;

      tft.startWrite();

      // progress text
      tft.setTextColor( C64_DARKBLUE, C64_MAROON );
      tft.setTextDatum( TL_DATUM );
      tft.setFont( &Font8x8C64 );

      sprintf(timeStr, "%02d:%02d", deltaMM, deltaSS );
      tft.drawString( timeStr, 0, 4 );
      // song length text
      tft.setTextDatum( TR_DATUM );
      sprintf(timeStr, "%02d:%02d",
        (sidPlayer->song_duration/60000)%60,
        (sidPlayer->song_duration/1000)%60
      );
      tft.drawString( timeStr, tft.width(), 4 );
      // progressbar
      tft.drawFastHLine( 0,         26, len_light, C64_MAROON );
      tft.drawFastHLine( 0,         27, len_light, TFT_WHITE );
      tft.drawFastHLine( 0,         28, len_light, C64_LIGHTBLUE );
      tft.drawFastHLine( 0,         29, len_light, C64_MAROON_DARK );
      tft.fillRect( len_light, 26, len_dark, 4, C64_DARKBLUE );

      tft.endWrite();
    }


    static void drawVoices( void* param )
    {

      SIDExplorer *mySIDExplorer = nullptr;
      loopmode playerloopmode = SID_LOOP_OFF;

      char    filenumStr[32]   = {0};
      char    songNameStr[255] = {0};
      int32_t lastprogress     = 0;
      int32_t lastCacheItemNum = -1;
      //int16_t textPosX         = 0,
      //        textPosY         = 0;
      uint8_t lastvolume       = maxVolume;
      int16_t lastsubsong      = -1;
      uint8_t deltaMM          = 0,
              lastMM           = 0;
      uint8_t deltaSS          = 0,
              lastSS           = 0;
      bool    draw_header      = false;
      bool    scrolltext       = false;

      if( param == NULL ) {
        log_e("Can't attach voice renderer to SIDExplorer");
        stoprender = false;
        log_d("[%d] Task will self-delete", ESP.getFreeHeap() );
        vTaskDelete( NULL );
      }

      stoprender     = false; // make the task stoppable from outside
      mySIDExplorer  = (SIDExplorer *) param;
      playerloopmode = mySIDExplorer->playerloopmode;

      mySIDExplorer->initSprites();
      tft.fillRect(0, 16, tft.width(), 16, C64_DARKBLUE ); // clear area under the logo

      while( stoprender == false ) {
        //if( stoprender ) break;

        if( render == false ) {
          // silence ?
          vTaskDelay( 10 );
          continue;
        }

        // track progress
        auto currentprogress = (100*sidPlayer->delta_song_duration) / sidPlayer->getCurrentTrackDuration();
        deltaMM = (sidPlayer->delta_song_duration/60000)%60;
        deltaSS = (sidPlayer->delta_song_duration/1000)%60;

        if( deltaMM != lastMM || deltaSS != lastSS ) {
          lastMM = deltaMM;
          lastSS = deltaSS;
          lastprogress = 0; // force progress render
        }

        if( lastsubsong != sidPlayer->currentsong || SongCache->CacheItemNum != lastCacheItemNum ) {
          lastsubsong = sidPlayer->currentsong;
          lastCacheItemNum = SongCache->CacheItemNum;
          currentprogress = 0;
          lastMM = 0;
          lastSS = 0;
          scrolltext = false; // wait for the next redraw
          draw_header = true;
        }
        if( playerloopmode != mySIDExplorer->playerloopmode ) {
          playerloopmode = mySIDExplorer->playerloopmode;
          draw_header = true;
        }
        if( lastvolume != maxVolume ) {
          lastvolume = maxVolume;
          draw_header = true;
        }
        /*
        if( sidPlayer->getPositionInPlaylist() != mySIDExplorer->positionInPlaylist ) {
          currentprogress = 0;
          lastMM = 0;
          lastSS = 0;
          scrolltext = false; // wait for the next redraw
          draw_header = true;
        }
        */

        if( scrolltext ) {
          scrollableText->doscroll();
        }

        if( lastprogress != currentprogress ) {
          lastprogress = currentprogress;
          if( currentprogress > 0 ) {
            drawSongProgress( currentprogress, deltaMM, deltaSS );
          } else {
            // clear progress zone
            tft.fillRect( 0, 25, tft.width(), 6, C64_DARKBLUE );
          }
        }

        if( draw_header ) {
          draw_header = false;
          drawSongHeader( mySIDExplorer, songNameStr, filenumStr, scrolltext );
        }

        for( int voice=0; voice<3; voice++ ) {
          oscViews[voice]->drawEnveloppe( voice, 0, HEADER_HEIGHT + ( voice*VOICE_HEIGHT ) );
        }

        if( showFPS ) renderFPS();
        vTaskDelay(10);
      }
      stoprender = false;
      log_d("[%d] Task will self-delete", ESP.getFreeHeap() );
      mySIDExplorer->deInitSprites();
      mySIDExplorer->explorer_needs_redraw = true;
      vTaskDelete( NULL );
    }


    static void eventCallback( SIDTunesPlayer* player, sidEvent event )
    {
      lastSIDEvent = event;
      lastEvent = millis();
      vTaskDelay(10);

      switch( event ) {
        case SID_NEW_FILE:
          render = true;
          log_d( "[%d] SID_NEW_FILE: %s", ESP.getFreeHeap(), sidPlayer->getFilename() );
        break;
        case SID_NEW_TRACK:
          render = true;
          log_d( "[%d] SID_NEW_TRACK: %s (%02d:%02d) %d/%d subsongs",
            ESP.getFreeHeap(),
            sidPlayer->getName(),
            sidPlayer->getCurrentTrackDuration()/60000,
            (sidPlayer->getCurrentTrackDuration()/1000)%60,
            sidPlayer->currentsong+1,
            sidPlayer->subsongs
          );
          //sidPlayer->setSpeed(1);
        break;
        case SID_START_PLAY:
          render = true;
          log_d( "[%d] SID_START_PLAY: %s", ESP.getFreeHeap(), sidPlayer->getFilename() );
        break;
        case SID_END_PLAY:
          render = false;
          log_d( "[%d] SID_END_PLAY: %s", ESP.getFreeHeap(), sidPlayer->getFilename() );
          switch( currentBrowsingType ) {
            case F_PLAYLIST:
              if( SongCache->CacheItemNum < SongCache->CacheItemSize ) {
                nextSIDEvent = SID_START_PLAY;
              } else {
                nextSIDEvent = SID_END_PLAY;
                log_d("End of playlist (playlist size:%d, position: %d", SongCache->CacheItemSize, SongCache->CacheItemNum );
              }
            break;
            case F_SID_FILE:
            default:
            break;
          }
        break;
        case SID_PAUSE_PLAY:
          log_d( "[%d] SID_PAUSE_PLAY: %s", ESP.getFreeHeap(), sidPlayer->getFilename() );
          render = false;
        break;
        case SID_RESUME_PLAY:
          log_d( "[%d] SID_RESUME_PLAY: %s", ESP.getFreeHeap(), sidPlayer->getFilename() );
          render = true;
        break;
        case SID_END_TRACK:
          render = false;
          log_d("[%d] SID_END_TRACK", ESP.getFreeHeap());
        break;
        case SID_STOP_TRACK:
          render = false;
          log_d("[%d] Track stopped", ESP.getFreeHeap());
        break;
      }
    }


    static void addFolderElement( const char* fullpath, folderItemType_t type )
    {
      static int elementsInFolder = 0;
      static String basepath = "";
      elementsInFolder++;
      basepath = gnu_basename( fullpath );
      myVirtualFolder->push_back( { basepath, String(fullpath), type } );
      switch( type )
      {
        case F_PLAYLIST:          log_d("[%d] Playlist Found [%s] @ %s/.sidcache", ESP.getFreeHeap(), basepath.c_str(), fullpath ); break;
        case F_SUBFOLDER:         log_d("[%d] Sublist Found [%s] @ %s/.sidcache",  ESP.getFreeHeap(), basepath.c_str(), fullpath ); break;
        case F_SUBFOLDER_NOCACHE: log_d("[%d] Subfolder Found :%s @ %s",           ESP.getFreeHeap(), basepath.c_str(), fullpath ); break;
        case F_PARENT_FOLDER:     log_d("[%d] Parent folder: %s @ %s",             ESP.getFreeHeap(), basepath.c_str(), fullpath ); break;
        case F_SID_FILE:          log_d("[%d] SID File: %s @ %s",                  ESP.getFreeHeap(), basepath.c_str(), fullpath ); break;
        case F_TOOL_CB:           log_d("[%d] Tool file: %s @ %s",                 ESP.getFreeHeap(), basepath.c_str(), fullpath ); break;
      }
      basepath = "";
      diskTicker( "", myVirtualFolder->size() );
    }


    static bool diskTicker( const char* title, size_t totalitems )
    {
      static bool toggle = false;
      static auto lastTickerMsec = millis();
      static char lineText[32] = {0};

      if( diskTickerWordWrap++%10==0 || millis() - lastTickerMsec > 500 ) { // animate "scanning folder"
        tft.setTextDatum( TR_DATUM );
        tft.setFont( &Font8x8C64 );
        tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
        uint16_t ypos = tft.height()-(tft.fontHeight()+2);
        tft.fillRect( 0, ypos, tft.width(), 8, C64_DARKBLUE );
        if( toggle ) {
          tft.drawJpg( icondisk_jpg, icondisk_jpg_len, 2, ypos );
        }

        tft.drawFastHLine( (tft.width()/2-44)+15, (tft.height()/2-13)+22, 4, toggle ? TFT_RED : TFT_ORANGE );

        snprintf(lineText, 12, "%s: #%d", title, totalitems );
        tft.drawString( lineText, tft.width() -2, ypos );
        toggle = !toggle;
        lastTickerMsec = millis();
      }
      //if( diskTickerWordWrap > 1024 ) return false; // to many files
      //if( millis() - diskTickerMsec > 30000 ) return false; // too long to scan
      return true;
    }


    static void MD5ProgressCb( size_t current, size_t total )
    {
      static size_t lastprogress = 0;
      size_t progress = (100*current)/total;
      if( progress != lastprogress ) {
        lastprogress = progress;
        // TODO: show UI Progress
        Serial.printf("Progress: %d%s\n", progress, "%" );
        UIPrintProgressBar( progress, total );
      }
    }
};


  // dafuq Arduino IDE ???
  #ifdef ARDUINO
    #include "SIDExplorer.cpp"
  #endif



#endif
