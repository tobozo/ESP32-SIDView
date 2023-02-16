#pragma once


namespace SIDView
{


  // CacheManager types
  enum SongCacheType_t
  {
    CACHE_DOTFILES,
    CACHE_DEEPSID
  };

  // folder items
  enum folderItemType_t
  {
    F_PARENT_FOLDER,     // parent folder
    F_PLAYLIST,          // SID file list for current folder
    F_DEEPSID_FOLDER,    // "folder" arg to deepsid API
    F_SUBFOLDER,         // subfolder with cached SID file list
    F_SUBFOLDER_NOCACHE, // subfolder with no SID file list
    F_SID_FILE,          // single SID file
    F_UNSUPPORTED_FILE,  // other file types unsupported by this player
    F_SYMLINK,           // symbolic link to a SID file, playlist or folder
    F_TOOL_CB,           // virtual file representing a program/callback
  };

  // display list items
  struct folderTypeItem_t
  {
    String name; // label
    String path; // full path
    folderItemType_t type;
    bool operator==(const folderTypeItem_t &item)
    {
      return item.type == type && item.name == name && item.path == path;
    }
  };


  // some callbacks CacheManager can use to nofity SIDExplorer UI
  typedef bool (*activityTickerCb)( const char* title, size_t totalitems ); // callback for i/o activity during folder enumeration
  typedef void (*folderElementPrintCb)( const char* fullpath, folderItemType_t type ); // adding a folder element to an enumerated list
  typedef void (*activityProgressCb)( size_t progress, size_t total, size_t words_count, size_t files_count, size_t folders_count ); // progress cb


  struct timeout_runner_t
  {
    typedef bool (*while_t)();
    bool whileMs( while_t whileCb, uint32_t timeout_ms )
    {
      uint32_t now = millis();
      uint32_t expired = now + timeout_ms;
      while( whileCb() ) {
        if( expired < millis() ) return false;
        vTaskDelay(1);
      }
      return true;
    }
  };


  class SIDExplorer;
  class ScrollableItem;

};


namespace UI_Utils
{
  enum UI_mode_t
  {
    none = 0,
    paged_listing,
    animated_views
  };

  struct LoadingScreen_t
  {
    public:
      const int imgwidth  = 88; // insertdisk_jpg width
      const int imgheight = 26; // insertdisk_jpg height
      int imgposx;   // insertdisk_jpg position X
      int imgposy;   // insertdisk_jpg position Y
      int titleposx; // text title position X
      int titleposy; // text title position Y
      int msgposx;   // text message position X
      int msgposy;   // text message position Y
      void init( int width, int height );
      void drawDisk();
      void drawModem();
      void clearDisk();
      void blinkDisk();
      void drawTitle( const char* title );
      void drawMsg(  const char* msg );
      void drawStrings( const char* title, const char* msg );
    private:
      void initSD();
      void setStyles();
      bool _inited = false;
      bool _toggle = false; // disk blink
  };

  extern LoadingScreen_t loadingScreen;



  struct SongHeader_t
  {
    int32_t lastprogress     = 0;
    int32_t lastCacheItemNum = -1;
    uint8_t lastvolume       = 0;
    //uint8_t maxVolume        = 15;
    int16_t lastsubsong      = -1;
    uint8_t deltaMM          = 0;
    uint8_t lastMM           = 0xff;
    uint8_t deltaSS          = 0;
    uint8_t lastSS           = 0xff;
    unsigned long currentprogress = 0;
    bool    draw_header      = false;
    bool    song_name_needs_scrolling = false;
    loopmode playerloopmode     = SID_LOOP_OFF;
    loopmode lastplayerloopmode = SID_LOOP_ON;
    char    filenumStr[32]   = {0};
    char    songNameStr[256] = {0};
    void loop();
    void setSong();
    void drawSong();
    void drawProgress( uint8_t lenMM, uint8_t lenSS );
    void clearProgress();
  };

  extern SongHeader_t songHeader;





  struct vumeter_builtin_led_t
  {
    int pin {-1};
    int freq_hz {5000};
    int channel {1};
    int resolution {8};
    uint8_t on_value {0x00};
    uint8_t off_value {0xff};
    void init();
    void deinit();
    void setFreq( int _freq_hz );
    void set( float value, int _freq_hz );
    void off();
  };

  extern vumeter_builtin_led_t led_meter;

};
