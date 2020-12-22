

void deInitSprites();
void initSprites();


struct folderTypeItem
{
  String name;
  String path;
  FolderItemType type;
};

static std::vector<folderTypeItem> myFolder;

static int elementsInFolder = 0;
void addFolderElement( const char* fullpath, FolderItemType type )
{
  elementsInFolder++;
  String basepath = gnu_basename( fullpath );
  myFolder.push_back( { basepath, String(fullpath), type } );
  switch( type )
  {
    case F_FOLDER:            log_d("Playlist Found [%s] @ %s/.sidcache\n", basepath.c_str(), fullpath ); break;
    case F_SUBFOLDER:         log_d("Sublist Found [%s] @ %s/.sidcache\n", basepath.c_str(), fullpath ); break;
    case F_SUBFOLDER_NOCACHE: log_d("Subfolder Found :%s @ %s\n", basepath.c_str(), fullpath ); break;
    case F_PARENT_FOLDER:     log_d("Parent folder: %s @ %s\n", basepath.c_str(), fullpath ); break;
    case F_SID_FILE:          log_d("SID File: %s @ %s\n", basepath.c_str(), fullpath ); break;
  }
}

static size_t diskTickerWordWrap = 0;
static auto diskTickerMsec = millis();
static auto lastTickerMsec = millis();
static bool diskTicker()
{
  static bool toggle = false;

  if( diskTickerWordWrap++%10==0 || millis() - lastTickerMsec > 500 ) { // animate "scanning folder"
    tft.setTextDatum( TR_DATUM );
    tft.setFont( &Font8x8C64 );
    tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    uint16_t ypos = tft.height()-(tft.fontHeight()+2);
    if( toggle ) {
      tft.drawJpg( icondisk_jpg, icondisk_jpg_len, 2, ypos );
    } else {
      tft.fillRect( 2, ypos, 8, 8, C64_DARKBLUE );
    }
    char lineText[12] = {0};
    snprintf(lineText, 12, "   #%d", myFolder.size() );
    tft.drawString( lineText, tft.width() -2, ypos );
    toggle = !toggle;
    lastTickerMsec = millis();
  }
  if( diskTickerWordWrap > 1024 ) return false; // to many files
  if( millis() - diskTickerMsec > 30000 ) return false; // too long to scan
  return true;
}


/*
template <typename T>
void move_range(size_t start, size_t length, size_t dst, std::vector<T> & v)
{
  const size_t final_dst = dst > start ? dst - length : dst;

  std::vector<T> tmp(v.begin() + start, v.begin() + start + length);
  v.erase(v.begin() + start, v.begin() + start + length);
  v.insert(v.begin() + final_dst, tmp.begin(), tmp.end());
}
*/


bool scanFolder( fs::FS &fs, const char* folderName, size_t maxitems=0, bool force_regen = false ) {

  Serial.printf("Scanning folder: %s\n", folderName  );
  myFolder.clear();

  if( String( folderName ) != "/"  ) {
    String dirname = gnu_basename( folderName );
    String parentFolder = String( folderName ).substring( 0, strlen( folderName ) - (dirname.length()+1) );
    if( parentFolder == "" ) parentFolder = "/";
    myFolder.push_back( { String(".."), parentFolder, F_PARENT_FOLDER } );
  }

  diskTickerWordWrap = 0;
  diskTickerMsec = millis();

  if( !SongCache->scanFolder( fs, folderName, &addFolderElement, &diskTicker, maxitems, force_regen ) ) {
    log_e("Could not build a list of songs");
    return false;
  }

  std::sort(myFolder.begin(), myFolder.end(), [](const folderTypeItem &a, const folderTypeItem &b)
  {
    if( a.name == b.name )
      return true;
    if( b.type == F_PARENT_FOLDER ) return false; // hoist ".."
    if( b.type == F_FOLDER && a.type == F_PARENT_FOLDER ) return true;
    if( b.type == F_FOLDER ) return false; // hoist playlist files
    if( a.type == F_FOLDER ) return true; // hoist playlist files
    // TODO: sort F_SUBFOLDER and F_SUBFOLDER_NOCACHE separately from F_SID_FILE
    uint8_t al = a.name.length(), bl = b.name.length(), max = 0;
    const char *as = a.name.c_str(), *bs = b.name.c_str();
    bool ret = false;
    if( al >= bl ) {
      max = bl;
      ret = false;
    } else {
      max = al;
      ret = true;
    }
    for( uint8_t i=0;i<max;i++ ) {
      if( as[i] != bs[i] ) return as[i] < bs[i];
    }
    return ret;
  });

  return true;
}


struct scrollableItem
{
  TFT_eSprite* sprite = new TFT_eSprite( &tft );
  char text[256];
  uint16_t xpos   = 0;
  uint16_t ypos   = 0;
  uint16_t width  = 0;
  uint16_t height = 0;
  unsigned long last = millis();
  unsigned long delay = 300; // milliseconds
  bool scroll = false;
  bool invert = true;
  void setup( const char* _text, uint16_t _xpos, uint16_t _ypos, uint16_t _width, uint16_t _height, unsigned long _delay=300, bool _invert=true )
  {
    snprintf( text, 255, " %s ", _text );
    xpos   = _xpos;
    ypos   = _ypos;
    width  = _width;
    height = _height;
    last   = millis()-_delay;
    delay  = _delay;
    invert = _invert;

    scroll = true;
    sprite->setPsram( false );
  }
  void doscroll()
  {
    if(!scroll) return;
    if( millis()-last < delay ) return;
    last = millis();
    sprite->createSprite( width, height );
    sprite->setFont( &Font8x8C64 );
    if( invert ) {
      sprite->setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
    } else {
      sprite->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    }
    sprite->fillSprite( C64_DARKBLUE );
    sprite->setTextDatum( TL_DATUM );
    char c = text[0];
    size_t len = strlen( text );
    memmove( text, text+1, len-1 );
    text[len-1] = c;
    text[len] = '\0';
    sprite->drawString( text, 0, 0 );
    sprite->pushSprite( xpos, ypos );
    sprite->deleteSprite();
  }
};

scrollableItem scrollableText;




class gameboystartup1 : public sid_instrument {
  public:
    int i;
    virtual void start_sample(int voice,int note) {
        sid.setAttack(    voice, 2 ); // 0 (0-15)
        sid.setDecay(     voice, 0 ); // 2 (0-15)
        sid.setSustain(   voice, 15 ); // 15 (0-15)
        sid.setRelease(   voice, 1 ); // 10 (0-15)
        sid.setPulse(     voice, 2048 ); // 1000 ( 0 - 4096 )
        sid.setWaveForm(  voice, SID_WAVEFORM_PULSE ); // SID_WAVEFORM_PULSE (TRIANGLE/SAWTOOTH/PULSE)
        sid.setGate(      voice, 1 ); // 1 (0-1)
        sid.setFrequency( voice, note); // note 0-95
        //i=0;
    }
};


class gameboystartup2 : public sid_instrument {
  public:
    int i;
    virtual void start_sample(int voice,int note) {
        sid.setAttack(    voice, 2 ); // 0 (0-15)
        sid.setDecay(     voice, 0 ); // 2 (0-15)
        sid.setSustain(   voice, 7 ); // 15 (0-15)
        sid.setRelease(   voice, 1 ); // 10 (0-15)
        sid.setPulse(     voice, 2048 ); // 1000 ( 0 - 4096 )
        sid.setWaveForm(  voice, SID_WAVEFORM_PULSE ); // SID_WAVEFORM_PULSE (TRIANGLE/SAWTOOTH/PULSE)
        sid.setGate(      voice, 1 ); // 1 (0-1)
        sid.setFrequency( voice, note); // note 0-95
        //i=0;
    }
};



void soundIntro()
{
  SIDKeyBoardPlayer::KeyBoardPlayer(1);
  sid.sidSetVolume( 0, 15 );
  delay(500);
  SIDKeyBoardPlayer::changeAllInstruments<gameboystartup1>();
  SIDKeyBoardPlayer::playNote( 0, _sid_play_notes[60], 120 ); // 1 octave = 12
  delay(120);
  SIDKeyBoardPlayer::changeAllInstruments<gameboystartup2>();
  SIDKeyBoardPlayer::playNote( 0, _sid_play_notes[84], 1000 ); // 1 octave = 12
  // fade out
  for( int t=0;t<16;t++ ) {
    delay( 1000/16 );
    sid.sidSetVolume( 0, 15-t );
  }
  SIDKeyBoardPlayer::stopNote( 0 );
  // back to full silence
  sid.setAttack(  0, 0 ); // 0 (0-15)
  sid.setDecay(   0, 0 ); // 2 (0-15)
  sid.setSustain( 0, 0 ); // 15 (0-15)
  sid.setRelease( 0, 0 ); // 10 (0-15)
  sid.setGate(    0, 0 ); // 1 (0-1)
  vTaskDelete( _sid_xHandletab[0] );
  sid.resetsid();
}




void sidCallback(  sidEvent event ) {
  lastEvent = millis();

  switch( event ) {
    case SID_NEW_FILE:
      render = true;
      Serial.printf( "[EVENT][Free Heap:%d] New file: %s\n", ESP.getFreeHeap(), sidPlayer->getFilename() );
    break;
    case SID_NEW_TRACK:
      render = true;
      Serial.printf( "[EVENT][Free Heap:%d]  New track: %s (%02d:%02d)\n",
        ESP.getFreeHeap(),
        sidPlayer->getName(),
        sidPlayer->getCurrentTrackDuration()/60000,
        (sidPlayer->getCurrentTrackDuration()/1000)%60
      );
      //sidPlayer->setSpeed(1);
    break;
    case SID_START_PLAY:
      render = true;
      Serial.printf( "[EVENT][Free Heap:%d]  Start play: %s\n", ESP.getFreeHeap(), sidPlayer->getFilename() );
    break;
    case SID_END_PLAY:
      Serial.printf( "[EVENT][Free Heap:%d]  Stopping play: %s\n", ESP.getFreeHeap(), sidPlayer->getFilename() );
      render = false;
    break;
    case SID_PAUSE_PLAY:
      Serial.printf( "[EVENT][Free Heap:%d]  Pausing play: %s\n", ESP.getFreeHeap(), sidPlayer->getFilename() );
      render = false;
    break;
    case SID_RESUME_PLAY:
      Serial.printf( "[EVENT][Free Heap:%d]  Resume play: %s\n", ESP.getFreeHeap(), sidPlayer->getFilename() );
      render = true;
    break;
    case SID_END_TRACK:
      render = false;
      Serial.printf("[EVENT][Free Heap:%d]  End of track\n", ESP.getFreeHeap());
      //sidPlayer->playNext()
    break;
    case SID_STOP_TRACK:
      render = false;
      Serial.printf("[EVENT][Free Heap:%d]  Track stopped\n", ESP.getFreeHeap());
    break;
  }
}



Preferences prefs;


struct NVSPrefs
{

  loopmode getLoopMode()
  {
    prefs.begin("SIDExplorer");
    uint8_t playerloopmode = (loopmode)prefs.getUChar("loopmode", (uint8_t)MODE_LOOP_PLAYLIST_RANDOM );
    prefs.end();
    Serial.print("Thawed loopmode from NVS: ");
    switch( playerloopmode ) { // validate
      case MODE_SINGLE_TRACK:               Serial.println("MODE_SINGLE_TRACK"); break;// don't loop (default, will play next until end of sid and/or track)
      case MODE_SINGLE_SID:                 Serial.println("MODE_SINGLE_SID"); break;// play all songs available in one sid once
      case MODE_LOOP_SID:                   Serial.println("MODE_LOOP_SID"); break;//loop all songs in on sid file
      case MODE_LOOP_PLAYLIST_SINGLE_TRACK: Serial.println("MODE_LOOP_PLAYLIST_SINGLE_TRACK"); break;// loop all tracks available in one playlist playing the default tunes
      case MODE_LOOP_PLAYLIST_SINGLE_SID:   Serial.println("MODE_LOOP_PLAYLIST_SINGLE_SID"); break;//loop all tracks available in one playlist playing all the subtunes
      case MODE_LOOP_PLAYLIST_RANDOM:       Serial.println("MODE_LOOP_PLAYLIST_RANDOM"); break;// loop random in current track list
      default: playerloopmode = MODE_LOOP_PLAYLIST_RANDOM; break;
    }
    return (loopmode)playerloopmode;
  }
  void setLoopMode( loopmode mode )
  {
    prefs.begin("SIDExplorer");
    prefs.putUChar( "loopmode", (uint8_t)mode );
    prefs.end();
    Serial.print("Saved loopmode to NVS: ");
    switch( mode ) {
      case MODE_SINGLE_TRACK:               Serial.println("MODE_SINGLE_TRACK"); break;// don't loop (default, will play next until end of sid and/or track)
      case MODE_SINGLE_SID:                 Serial.println("MODE_SINGLE_SID"); break;// play all songs available in one sid once
      case MODE_LOOP_SID:                   Serial.println("MODE_LOOP_SID"); break;//loop all songs in on sid file
      case MODE_LOOP_PLAYLIST_SINGLE_TRACK: Serial.println("MODE_LOOP_PLAYLIST_SINGLE_TRACK"); break;// loop all tracks available in one playlist playing the default tunes
      case MODE_LOOP_PLAYLIST_SINGLE_SID:   Serial.println("MODE_LOOP_PLAYLIST_SINGLE_SID"); break;//loop all tracks available in one playlist playing all the subtunes
      case MODE_LOOP_PLAYLIST_RANDOM:       Serial.println("MODE_LOOP_PLAYLIST_RANDOM"); break;// loop random in current track list
    }
  }

  uint8_t getVolume()
  {
    prefs.begin("SIDExplorer");
    uint8_t volume = (loopmode)prefs.getUChar("volume", 10 );
    volume = volume%16; // avoid returning poisonous value
    Serial.printf("Thawed volume from NVS: %d\n", volume );
    prefs.end();
    return volume;
  }

  void setVolume( uint8_t volume )
  {
    prefs.begin("SIDExplorer");
    prefs.putUChar( "volume", volume );
    prefs.end();
    Serial.printf("Saved volume to NVS: %d\n", volume );
  }


  void setLastPath( const char* path )
  {
    prefs.begin("SIDExplorer");
    size_t bytes_written = prefs.putString("path", path);
    prefs.end();
  }

  void getLastPath( char* path )
  {
    prefs.begin("SIDExplorer");
    size_t pathlen = prefs.getString("path", path, 255);
    if( pathlen <= 0 ) {
      snprintf( path, 255, "%s", SID_FOLDER );
    }
    prefs.end();
  }


};

NVSPrefs PlayerPrefs;



struct SIDExplorer
{

  fs::FS &fs;
  SID_Archive* archives = nullptr;
  size_t totalelements = 0;

  char     *folderToScan;
  char     *lineText;

  int32_t  itemClicked        = -1;

  const uint16_t itemsPerPage = 8;
  uint16_t itemCursor         = 0;
  uint16_t lastItemCursor[16] = {0};
  uint8_t  folderDepth        = 0;
  uint16_t spriteWidth        = 0;
  uint16_t spriteHeight       = 0;
  uint16_t lineHeight         = 0;
  uint16_t minScrollableWidth = 0;
  loopmode playerloopmode     = MODE_LOOP_PLAYLIST_RANDOM;
  const char* loopmodeicon    = (const char*)iconloop_jpg;
  size_t loopmodeicon_len     = iconloop_jpg_len;
  bool     exitloop           = false;
  bool     firstRender        = true;
  bool     wire_begun         = false;

  #ifdef BOARD_HAS_PSRAM
    size_t maxitems = 0; // 0 = no limit
  #else
    size_t maxitems = 255;
    //Serial.printf("Limiting folders to %d items due to lack of psram\n", maxitems );
  #endif
  TFT_eSprite spriteScroll    = TFT_eSprite( &tft );
  uint16_t* sptr; // pointer to the sprite

  uint16_t pageNum     = 0;
  uint16_t pageTotal   = 0;
  uint16_t pageStart   = 0;
  uint16_t pageEnd     = 0;
  uint16_t ydirnamepos = 0;
  uint16_t ydislistpos = 0;
  uint16_t yitempos    = 1;

  bool explorer_needs_redraw = false;
  bool adsrenabled = false;
  unsigned long inactive_since = millis();
  unsigned long inactive_delay = 5000;

  ~SIDExplorer() {
    free( folderToScan );
    free( lineText     );
  }

  SIDExplorer( fs::FS &_fs, SID_Archive* _archives = nullptr, size_t _totalelements = 0 ) :
    fs(_fs), archives(_archives), totalelements(_totalelements) {
      folderToScan = (char*)sid_calloc( 256, sizeof(char) );
      lineText     = (char*)sid_calloc( 256, sizeof(char) );
      //snprintf( folderToScan, 256, "%s", SID_FOLDER );
      PlayerPrefs.getLastPath( folderToScan );
      spriteWidth        = tft.width();
      spriteHeight       = tft.height();
      lineHeight         = spriteScroll.fontHeight()+2;
      minScrollableWidth = spriteWidth - 14;
  }

  int32_t explore() {

    spriteScroll.setPsram(false);
    spriteScroll.setFont( &Font8x8C64 );
    spriteScroll.setTextSize(1);

    tft.setTextDatum( MC_DATUM );
    tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    tft.drawString( "Checking MD5", tft.width()/2, tft.height()/2 );

    MD5Config.progressCb = &MD5ProgressCb;

    sidPlayer = new SIDTunesPlayer( 1 );
    sidPlayer->setMD5Parser( &MD5Config );

    sidPlayer->setEventCallback( sidCallback );
    sidPlayer->begin( SID_CLOCK, SID_DATA, SID_LATCH, SID_CLOCK_PIN );

    playerloopmode = PlayerPrefs.getLoopMode();
    sidPlayer->setLoopMode( playerloopmode );
    setUIPlayerMode( playerloopmode );

    maxVolume = PlayerPrefs.getVolume();
    sidPlayer->SetMaxVolume( maxVolume );

    if( myFolder.size() == 0 ) {
      checkArchives();
    }
/*

    songstruct *blah = (songstruct *)sid_calloc(1,sizeof(songstruct));
    blah->subsongs = 1;

    snprintf( blah->md5, 32, "%s", "48c497c669ea4cd7122e073b35393d01" ); // 1:02

    if( sidPlayer->MD5Parser->getDurationsFastFromMD5Hash( blah ) ) {
      Serial.println("Success!");
    }
*/

    while( !exitloop ) {

      setupPagination(); // handle pagination
      drawPaginaged();   // draw page

      explorer_needs_redraw = false;
      adsrenabled = false;
      inactive_since = millis();
      inactive_delay = 5000;

      while( !explorer_needs_redraw ) { // wait for button activity
        animateView();
        processHID();
      } // end while( !explorer_needs_redraw )

    } // end while( !exitloop )
    return itemClicked;
  }


  static void MD5ProgressCb( size_t current, size_t total )
  {
    static size_t lastprogress = 0;
    size_t progress = (100*current)/total;
    if( progress != lastprogress ) {
      lastprogress = progress;
      // TODO: show UI Progress
      Serial.printf("Progress: %d%s\n", progress, "%" );
    }
  }


  void animateView()
  {

    if( millis() - inactive_delay < inactive_since ) {
      scrollableText.doscroll(); // animate text overflow if any
      stoprender = true; // send stop signal to task
      delay(10);
      renderVoicesTaskHandle = NULL;
    } else {
      if( sidPlayer->playerrunning ) {
        if( renderVoicesTaskHandle == NULL  && ! adsrenabled ) {
          // ADSR not running, draw UI and run task
          log_w("launching renverVoices task");
          drawHeader( nullptr );
          positionInPlaylist = -1; // make sure the title is displayed
          xTaskCreatePinnedToCore( renderVoices, "renderVoices", 8192, this, 1, &renderVoicesTaskHandle, SID_PLAYER_CORE/*SID_CPU_CORE*/ ); // will trigger TFT writes
          adsrenabled = true;
        } else {
          // ADSR already running or stopping
        }
      } else {
        scrollableText.doscroll(); // animate text overflow if any
        stoprender = true; // send stop signal to task
        delay(10);
        renderVoicesTaskHandle = NULL;
      }
    }
  }


  void drawHeader( void* param )
  {
    TFT_eSprite *headerSprite;
    if( param == NULL ) {
      param = (void*)&tft;
      headerSprite = new TFT_eSprite( (M5Display*)param );
      ((M5Display*)param)->drawString( "-=SID Explorer=-", spriteWidth /2, 2 ); // draw failsafe lowres header
    } else {
      headerSprite = new TFT_eSprite( (TFT_eSprite*)param );
      ((TFT_eSprite*)param)->drawString( "-=SID Explorer=-", spriteWidth /2, 2 ); // draw failsafe lowres header
    }
    headerSprite->setPsram(false);
    const char *hdrptr = (const char*)headerSprite->createSprite( tft.width(), 16 );
    if( hdrptr == NULL ) {
      log_w("Not enough ram to draw header");
      return;
    }
    headerSprite->fillSprite( C64_DARKBLUE ); // Commodore64 blueish
    headerSprite->drawFastHLine( 0, 0, tft.width(), C64_MAROON_DARKER );
    headerSprite->drawFastHLine( 0, 1, tft.width(), C64_MAROON_DARK );
    headerSprite->fillRect( 0, 2, tft.width(), 12, C64_MAROON );
    headerSprite->drawJpg( header48x12_jpg, header48x12_jpg_len, 41, 2 );
    headerSprite->drawFastHLine( 0, 14, tft.width(), C64_MAROON_DARK );
    headerSprite->drawFastHLine( 0, 15, tft.width(), C64_MAROON_DARKER );
    headerSprite->pushSprite( 0, 0 );
    headerSprite->deleteSprite();
  }


  void processHID()
  {
    if( !wire_begun ) {
      Wire.begin( SDA, SCL ); // connect XPadShield
      wire_begun = true;
    }
    Wire.requestFrom(0x10, 1); // scan buttons from XPadShield
    int resp = Wire.read();
    if( resp!=0 && ( lastresp != resp || lastpush + debounce < millis() ) ) {
      switch(resp) {
        case 0x01: // down
          if( itemCursor < myFolder.size()-1 ) itemCursor++;
          else itemCursor = 0;
          explorer_needs_redraw = true;
          stoprender = true;
          delay(10);
        break;
        case 0x02: // up
          if( itemCursor > 0 ) itemCursor--;
          else itemCursor = myFolder.size()-1;
          explorer_needs_redraw = true;
          stoprender = true;
          delay(10);
        break;
        case 0x04: // right
          if( /*myFolder[itemCursor].type == F_FOLDER &&*/ sidPlayer->playerrunning && sidPlayer->maxSongs > 1 ) {
            if( playerloopmode == MODE_LOOP_SID || playerloopmode == MODE_SINGLE_SID || playerloopmode == MODE_LOOP_PLAYLIST_SINGLE_SID ) {
              sidPlayer->playPrevSongInSid();
            } else {
              sidPlayer->playPrev();
            }
          }
        break;
        case 0x08: // left
          if( /*myFolder[itemCursor].type == F_FOLDER &&*/ sidPlayer->playerrunning && sidPlayer->maxSongs > 1 ) {
            if( playerloopmode == MODE_LOOP_SID || playerloopmode == MODE_SINGLE_SID || playerloopmode == MODE_LOOP_PLAYLIST_SINGLE_SID ) {
              sidPlayer->playNextSongInSid();
            } else {
              sidPlayer->playNextSong();
            }
          }
        break;
        break;
        case 0x10: // B (play/stop button)
          if( !adsrenabled ) {
            switch( myFolder[itemCursor].type )
            {
              case F_SID_FILE:
                if( sidPlayer->playerrunning ) {
                  sidPlayer->stopPlayer();
                  vTaskDelay(100);
                } else {
                  sidPlayer->setMaxSongs( 1 );
                }
                Serial.println("Adding song");
                if( sidPlayer->addSong( SID_FS, myFolder[itemCursor].path.c_str() ) > 0 ) {
                  vTaskDelay(100); // give time to load
                  Serial.println("Song added, now playing");
                  sidPlayer->SetMaxVolume( maxVolume ); //value between 0 and 15
                  sidPlayer->play(); //it will play one songs in loop
                }
              break;
              case F_FOLDER:
                if( sidPlayer->playerrunning ) {
                  sidPlayer->stopPlayer();
                  vTaskDelay(100);
                }
                sidPlayer->setMaxSongs( 100 );
                if( SongCache->loadCache( sidPlayer, myFolder[itemCursor].path.c_str() ) ) {
                  sidPlayer->setLoopMode( playerloopmode );
                  sidPlayer->SetMaxVolume( maxVolume ); //value between 0 and 15
                  sidPlayer->play(); //it should play all songs in loop
                }
              break;
              case F_PARENT_FOLDER:
              case F_SUBFOLDER:
              case F_SUBFOLDER_NOCACHE:
                snprintf(folderToScan, 256, "%s", myFolder[itemCursor].path.c_str() );
                PlayerPrefs.setLastPath( folderToScan );
                scanFolder( fs, folderToScan, maxitems );
                if( myFolder[itemCursor].type == F_PARENT_FOLDER ) {
                  // going up
                  if( folderDepth > 0 ) {
                    itemCursor = lastItemCursor[folderDepth-1];
                  } else {
                    itemCursor = 0;
                  }
                  lastItemCursor[folderDepth] = 0;
                } else {
                  // going down
                  lastItemCursor[folderDepth] = itemCursor;
                  itemCursor = 0;
                }
              break;
            }
          } else {
            sidPlayer->stopPlayer();
            vTaskDelay(100);
          }
          explorer_needs_redraw = true;
        break;
        case 0x20: // Action button
          // circular toggle
          if( adsrenabled ) {
            switch (playerloopmode) {
              case MODE_SINGLE_TRACK:               setUIPlayerMode( MODE_SINGLE_SID ); break;// don't loop (default, will play next until end of sid and/or track)
              case MODE_SINGLE_SID:                 setUIPlayerMode( MODE_LOOP_SID ); break;// play all songs available in one sid once
              case MODE_LOOP_SID:                   setUIPlayerMode( MODE_LOOP_PLAYLIST_SINGLE_TRACK ); break; //loop all songs in on sid file
              case MODE_LOOP_PLAYLIST_SINGLE_TRACK: setUIPlayerMode( MODE_LOOP_PLAYLIST_SINGLE_SID ); break; // loop all tracks available in one playlist playing the default tunes
              case MODE_LOOP_PLAYLIST_SINGLE_SID:   setUIPlayerMode( MODE_LOOP_PLAYLIST_RANDOM ); break; //loop all tracks available in one playlist playing all the subtunes
              case MODE_LOOP_PLAYLIST_RANDOM:       setUIPlayerMode( MODE_SINGLE_TRACK ); break; // loop random in current track list
            };
            sidPlayer->setLoopMode( playerloopmode );
            PlayerPrefs.setLoopMode( playerloopmode ); // save to NVS
          } else {
            // only when the ".." folder is being selected
            if( myFolder[itemCursor].type == F_PARENT_FOLDER ) {
              Serial.printf("Refresh folder -> regenerate %s Dir List and Songs Cache\n", folderToScan );
              scanFolder( fs, folderToScan, maxitems, true );
            } else {
              // TODO: show information screen on current selected item
              // folder = show dircache count or suggest creating dircache
              // playlist = show playlist length
              // sid file = author, name, pub, subsongs, lengths
            }
          }
        break;
        case 0x40: // C
          if( maxVolume > 0 ) {
            maxVolume--;
            sidPlayer->SetMaxVolume( maxVolume ); //value between 0 and 15
            Serial.printf("New volume level: %d\n", maxVolume );
            PlayerPrefs.setVolume( maxVolume );
          }
        break;
        case 0x80: // D
          if( maxVolume < 15 ) {
            maxVolume++;
            sidPlayer->SetMaxVolume( maxVolume ); //value between 0 and 15
            Serial.printf("New volume level: %d\n", maxVolume );
            PlayerPrefs.setVolume( maxVolume );
          }
        break;
        default: // simultaneous buttons push ?
          log_w("Unhandled button combination: %X", resp );
        break;

      }
      lastresp = resp;
      lastpush = millis();
    }
    delay(30); // don't spam I2C
  }


  void setUIPlayerMode( loopmode mode )
  {
    switch (mode) {
      case MODE_SINGLE_TRACK: // don't loop (default, will play next until end of sid and/or track)
        loopmodeicon     = (const char*)single_jpg;
        loopmodeicon_len = single_jpg_len;
      break;
      case MODE_SINGLE_SID:// play all songs available in one sid once
        loopmodeicon     = (const char*)single_jpg;
        loopmodeicon_len = single_jpg_len;
      break;
      case MODE_LOOP_SID: //loop all songs in on sid file
        loopmodeicon     = (const char*)iconsidfile_jpg;
        loopmodeicon_len = iconsidfile_jpg_len;
      break;
      case MODE_LOOP_PLAYLIST_SINGLE_TRACK: // loop all tracks available in one playlist playing the default tunes
        loopmodeicon     = (const char*)iconloop_jpg;
        loopmodeicon_len = iconloop_jpg_len;
      break;
      case MODE_LOOP_PLAYLIST_SINGLE_SID: //loop all tracks available in one playlist playing all the subtunes
        loopmodeicon     = (const char*)iconlist_jpg;
        loopmodeicon_len = iconlist_jpg_len;
      break;
      case MODE_LOOP_PLAYLIST_RANDOM: // loop random in current track list
        loopmodeicon     = (const char*)iconrandom_jpg;
        loopmodeicon_len = iconrandom_jpg_len;
      break;
    };
    playerloopmode = mode;
  }


  void setupPagination()
  {
    pageNum   = itemCursor/itemsPerPage;
    pageTotal = myFolder.size()/itemsPerPage;
    pageStart = pageNum*itemsPerPage;
    pageEnd   = pageStart+itemsPerPage;
    ydirnamepos = lineHeight*1.5 + 2;
    ydislistpos = ydirnamepos + lineHeight*1.5;
    if( pageEnd >= myFolder.size() ) {
      pageEnd = myFolder.size();
    }
    log_d(" ------ MENU ---------- [%d]", ESP.getFreeHeap() );
    log_d(" c: %d, p: %d/%d, ps: %d, pe: %d", itemCursor, pageNum+1, pageTotal+1, pageStart, pageEnd );
    log_d("$> %s", folderToScan );
  }


  void drawPaginaged()
  {
    sptr = (uint16_t*)spriteScroll.createSprite( spriteWidth, spriteHeight );
    if( sptr == nullptr ) {
      log_e("Failed to create %dx%d sprite when heap was at %d and folder containing %d items", spriteWidth, spriteHeight, ESP.getFreeHeap(), myFolder.size() );
    }
    spriteScroll.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    spriteScroll.fillSprite( C64_DARKBLUE );
    spriteScroll.setTextWrap( false );
    spriteScroll.setTextDatum( TC_DATUM );

    //spriteScroll.drawString( "-=SID Explorer=-", spriteWidth /2, 2 );
    drawHeader( &spriteScroll );

    spriteScroll.setTextDatum( TL_DATUM );

    folderDepth = 0;
    for( int i=0; i<strlen(folderToScan); i++ ) {
      if( folderToScan[i] == '/' ) folderDepth++;
    }

    if( spriteScroll.textWidth( folderToScan )+2 > spriteWidth  ) {
      String bname = gnu_basename( folderToScan );
      snprintf( lineText, 256, ">> %s", bname.c_str() );
    } else {
      snprintf( lineText, 256, "$> %s", folderToScan );
    }
    spriteScroll.drawString( lineText, 2, ydirnamepos );

    for(size_t i=pageStart,j=0; i<pageEnd; i++,j++ ) {
      yitempos = ydislistpos + j*lineHeight;
      switch( myFolder[i].type )
      {
        case F_FOLDER:
          sprintf( lineText, "%s", myFolder[i].name.c_str() );
          spriteScroll.drawJpg( iconlist_jpg, iconlist_jpg_len, 2, yitempos );
          break;
        case F_SUBFOLDER:
          sprintf( lineText, "%s", myFolder[i].name.c_str() );
          spriteScroll.drawJpg( iconfolder2_jpg, iconfolder2_jpg_len, 2, yitempos );
          break;
        case F_SUBFOLDER_NOCACHE:
          sprintf( lineText, "%s", myFolder[i].name.c_str() );
          spriteScroll.drawJpg( iconfolder1_jpg, iconfolder1_jpg_len, 2, yitempos );
          break;
        case F_PARENT_FOLDER:
          sprintf( lineText, "%s", myFolder[i].name.c_str() );
          spriteScroll.drawJpg( iconup_jpg, iconup_jpg_len, 2, yitempos );
          break;
        case F_SID_FILE:
          String basename = gnu_basename( myFolder[i].path.c_str() );
          sprintf( lineText, "%s", basename.c_str() );
          spriteScroll.drawJpg( iconsidfile_jpg, iconsidfile_jpg_len, 2, yitempos );
          break;
      }
      if( i==itemCursor ) { // highlight
        spriteScroll.setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
        if( strcmp( lineText, ".." ) == 0 ) {
            if( spriteScroll.textWidth( folderToScan ) > minScrollableWidth-10 ) {
              scrollableText.setup( folderToScan, 24, ydirnamepos, minScrollableWidth-10, lineHeight, 300, false );
            } else {
              scrollableText.scroll = false;
            }
        } else if( spriteScroll.textWidth( lineText ) > minScrollableWidth ) {
          scrollableText.setup( lineText, 14, yitempos, minScrollableWidth, lineHeight );
        } else {
          scrollableText.scroll = false;
        }
      } else {
        spriteScroll.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
      }
      spriteScroll.drawString( lineText, 14, yitempos );
    }

    spriteScroll.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    sprintf(lineText, "%d/%d", pageNum+1, pageTotal+1 );
    spriteScroll.setTextDatum( TL_DATUM );
    spriteScroll.drawString( lineText, 16, spriteHeight-lineHeight );
    spriteScroll.drawJpg( iconinfo_jpg, iconinfo_jpg_len, 2, spriteHeight-lineHeight ); // clear disk icon
    sprintf(lineText, "#%d", myFolder.size() );
    spriteScroll.setTextDatum( TR_DATUM );
    spriteScroll.drawString( lineText, spriteWidth -2, spriteHeight-lineHeight );

    if( firstRender ) {
      firstRender = false;
      for( int i=spriteHeight; i>=0; i-- ) {
        spriteScroll.pushSprite( 0, i );
      }
      soundIntro();
    } else {
      spriteScroll.pushSprite( 0, 0 );
    }

    spriteScroll.deleteSprite();
  }


  void checkArchives()
  {

    mkdirp( MD5Config.fs, MD5Config.md5filepath ); // from ESP32-Targz: create traversing directories if none exist

    if( !scanFolder(fs, folderToScan, maxitems ) ) {
      // first run ?
      //TODO: confirmation dialog
      //#ifdef BOARD_HAS_PSRAM
      if( archives != nullptr ) {
        deInitSprites(); // free some ram
        SidArchiveChecker = new SID_Archive_Checker( &MD5Config );
        SidArchiveChecker->checkArchives( archives, totalelements );
        // at this stage the heap is probably too low to do anything else
        //TODO: confirmation dialog
        ESP.restart();
        // initSprites();
      }
      //#endif
    } else {
      //TODO: confirmation dialog
      // give some hints for building the archives ?
    }
  }

};
