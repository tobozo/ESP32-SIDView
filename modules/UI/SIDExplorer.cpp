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

#include "SIDExplorer.hpp"


SIDExplorer::~SIDExplorer()
{
  //free( folderToScan );
  free( lineText     );
  free( oscViews     );
  //free( currentPath  );
  deInitSprites();
  //sprite->deleteSprite();
  delete SongCache;
  delete sidPlayer;
  delete Nav->items;
  SongCache = NULL;
  sidPlayer = NULL;
  Nav->items = NULL;
}



SIDExplorer::SIDExplorer( MD5FileConfig *_cfg ) : cfg(_cfg)
{
  mux = xSemaphoreCreateMutex();
  if( psramInit() ) {
    ramSize         = ESP.getPsramSize();
  }
  fs              = cfg->fs;
  cfg->progressCb = &MD5ProgressCb;

  //folderToScan    = (char*)         sid_calloc( 256, sizeof(char) );
  lineText        = (char*)         sid_calloc( 256, sizeof(char) );
  //currentPath     = (char*)         sid_calloc( 256, sizeof(char) );
  oscViews        = (OscilloView**) sid_calloc( 3, sizeof( OscilloView* ) );
  track           = (SID_Meta_t*)   sid_calloc( 1, sizeof( SID_Meta_t ) );

  spriteWidth     = tft.width();
  spriteHeight    = tft.height();
  spriteText      = new TFT_eSprite( &tft );
  spriteHeader    = new TFT_eSprite( &tft );
  spritePaginate  = new TFT_eSprite( &tft );

  uint32_t voice_maxheight = min( tft.height(), 160 );
  uint32_t voice_maxwidth  = min( tft.width(), 256 );

  VOICE_HEIGHT  = ((voice_maxheight-HEADER_HEIGHT)/3); // = 29 on a 128x128
  VOICE_WIDTH   = voice_maxwidth;

  for( int i=0; i<3; i++ ) {
    oscViews[i]   = new OscilloView( VOICE_WIDTH, VOICE_HEIGHT );
  }
//   log_i("Init sidplayer");
//   sidPlayer       = new SIDTunesPlayer( cfg->fs );
//   log_i("Init songcache");
//   SongCache       = new SongCacheManager( sidPlayer );
//   log_i("Init foldercache");
//   myVirtualFolder = new VirtualFolderNoRam( SongCache );
//   log_i("Setting md5 parser");
//   sidPlayer->setMD5Parser( cfg );

  log_i("Set sprites memory");
  spriteText->setPsram(false);
  spriteText->setFont( &Font8x8C64 );
  spriteText->setTextSize(1);

  spriteHeader->setPsram(false);
  spriteHeader->setFont( &Font8x8C64 );
  spriteHeader->setTextSize(1);

  spritePaginate->setPsram(false);
  spritePaginate->setFont( &Font8x8C64 );
  spritePaginate->setTextSize(1);

  lineHeight         = spriteText->fontHeight()+2;
  log_w("Line height for lists=%d", lineHeight );

  minScrollableWidth = spriteWidth - 14;
  float ipp = ( float(tft.height()) - (float(lineHeight*2.5)+16.0) - TouchPadHeight ) / float(lineHeight);

  itemsPerPage = floor(ipp);//( tft.height() - (lineHeight*2.5 + 16) ) / lineHeight;
  log_w("Items per page for lists=%d", itemsPerPage );

  xTaskCreatePinnedToCore( SIDExplorer::mainTask, "sidExplorer", 16384, this, SID_MAINTASK_PRIORITY, &renderUITaskHandle, SID_MAINTASK_CORE ); // will trigger SD reads and TFT writes

  //initHID();

}



void SIDExplorer::mainTask( void *param )
{
  vTaskDelay(1); // let the loop() task self-delete
  log_e("Attaching SIDExplorer from core #%d", xPortGetCoreID() );
  SIDExplorer *SidViewer = (SIDExplorer*) param;
  //tft.setRotation( 2 ); // portrait mode !
  //tft.setBrightness( 50 );
  log_i("Init sidplayer");
  sidPlayer = new SIDTunesPlayer( SidViewer->cfg->fs );
  log_i("Setting md5 parser");
  sidPlayer->setMD5Parser( SidViewer->cfg );
  log_i("Init songcache");
  SongCache = new SongCacheManager( sidPlayer );
  log_i("Init nav and folder cache");
  Nav = new FolderNav( new VirtualFolderNoRam( SongCache ) );
  initHID();
  log_i("Init SIDExplorer");
  SidViewer->explore();
  log_i("Leaving mainTask");
  vTaskDelete( NULL );
}



int32_t SIDExplorer::explore()
{
  randomSeed(analogRead(0)); // for random song

  loadingScreen();  //log_e("Accessing Filesystem from core #%d", xPortGetCoreID() );
  afterHIDRead();

  //M5.sd_begin();
  delay(100);

  PlayerPrefs.getLastPath( Nav->dir );
  playerloopmode = PlayerPrefs.getLoopMode();
  maxVolume = PlayerPrefs.getVolume();

  if(Nav->dir[0] != '/' || ! fs->exists( Nav->dir ) ) {
    Nav->setDir( SID_FOLDER );
    //sprintf( Nav->dir, "%s", SID_FOLDER );
  } else {
    fs::File testFile = fs->open( Nav->dir );
    if( !testFile.isDirectory() ) {
      Nav->setDir( SID_FOLDER );
      //sprintf( Nav->dir, "%s", SID_FOLDER );
    }
    testFile.close();
  }

  if( Nav->items->size() == 0 ) {
    checkArchive();
  }

  sidPlayer->setEventCallback( eventCallback );
  sidPlayer->setLoopMode( playerloopmode );
  setUIPlayerMode( playerloopmode );

  sidRunner();

  beforeHIDRead();

  while( !exitloop ) {

    afterHIDRead();
    log_v("[%d] Setting up pagination", ESP.getFreeHeap() );
    setupPagination(); // handle pagination
    log_v("[%d] Drawing paginated", ESP.getFreeHeap() );
    drawPaginaged();   // draw page
    beforeHIDRead();

    explorer_needs_redraw = false;
    adsrenabled = false;
    inactive_since = millis();

    log_v("[%d] entering loop", ESP.getFreeHeap() );

    while( !explorer_needs_redraw ) { // wait for button activity
      handleAutoPlay();
      animateView();
      processHID();
      vTaskDelay(1);
    } // end while( !explorer_needs_redraw )

    vTaskDelay(1);

  } // end while( !exitloop )
  return itemClicked;
}




void SIDExplorer::sidRunner()
{
  if( !sidPlayer->begun ) {
    sidPlayer->sid.setTaskCore( M5.SD_CORE_ID );
    if( sidPlayer->begin( SID_CLOCK, SID_DATA, SID_LATCH, SID_CLOCK_PIN ) ) {
      sidPlayer->setMaxVolume( maxVolume );
      log_w("SID Player Begin OK");
    } else {
      log_e("SID Player failed to start ... duh !");
      while(1);
    }
  }
}


void SIDExplorer::sidSleep()
{

  //sidPlayer->kill();

  tft.setBrightness(0);
  tft.getPanel()->setSleep(true);

  auto backlight = (lgfx::Light_PWM*)tft.getPanel()->getLight();
  if( backlight != nullptr ) {
    auto cfg = backlight->config();
    int16_t pin_bl = cfg.pin_bl;
    if( pin_bl > -1 ) {
      gpio_hold_en((gpio_num_t) pin_bl ); // keep TFT_LED off while in deep sleep
      gpio_deep_sleep_hold_en();
    }
  }

  int16_t pin_bl = ( (lgfx::Light_PWM*) tft.getPanel()->getLight() )->config().pin_bl;
  if( pin_bl > -1 ) {
    gpio_hold_en((gpio_num_t) pin_bl ); // keep TFT_LED off while in deep sleep
    gpio_deep_sleep_hold_en();
  }

  esp_deep_sleep_start();

}




void SIDExplorer::handleAutoPlay()
{
  if( currentBrowsingType == F_PLAYLIST ) {
    switch( nextSIDEvent ) {
      case SID_START_PLAY:
        nextInPlaylist();
      break;
      default:
      break;
    }
    nextSIDEvent = SID_END_PLAY;
  }
}



void SIDExplorer::prevInPlaylist()
{
  sidPlayer->setLoopMode( SID_LOOP_OFF );
  render = false;
  vTaskDelay(1);
  if( SongCache->CacheItemNum > 0 && SongCache->getInfoFromCacheItem( Nav->path, SongCache->CacheItemNum-1 ) ) {
    log_d("Playing track #%d from cached playlist file %s (%d elements)", SongCache->CacheItemNum, Nav->path, SongCache->CacheItemSize );
    sidPlayer->playSID( SongCache->currenttrack );
    inactive_delay = 100;
    lastpush = millis();
    explorer_needs_redraw = true;
  } else {
    log_e("Prev cache item (#%d) is not playable", SongCache->CacheItemNum-1);
  }
  render = true;
  vTaskDelay(1);
}



void SIDExplorer::nextInPlaylist()
{
  sidPlayer->setLoopMode( SID_LOOP_OFF );
  render = false;
  vTaskDelay(1);
  if( SongCache->getInfoFromCacheItem( Nav->path, SongCache->CacheItemNum+1 ) ) {
    log_d("Playing track #%d from cached playlist file %s (%d elements)", SongCache->CacheItemNum, Nav->path, SongCache->CacheItemSize );
    sidPlayer->playSID( SongCache->currenttrack );
    inactive_delay = 100;
    lastpush = millis();
    explorer_needs_redraw = true;
  } else {
    log_e("Next cache item (#%d) is not playable", SongCache->CacheItemNum+1);
  }
  render = true;
  vTaskDelay(1);
}



void SIDExplorer::processHID()
{
  int resp = readHID();

  if( resp>0 && ( lastresp != resp || lastpush + debounce < millis() ) ) {

    afterHIDRead(); // pause HID bus if necessary

    log_d("HID Action = %d", resp );

    currentBrowsingType = Nav->items->get(Nav->pos%itemsPerPage).type;
    Nav->setPath( Nav->items->get(Nav->pos%itemsPerPage).path.c_str() );
    //snprintf( Nav->path, 255, "%s", Nav->items->get(Nav->pos%itemsPerPage).path.c_str() );

    switch(resp) {


      case HID_SLEEP:
        sidSleep();
      break;

      //#if defined HID_USB

      case HID_SEARCH:
      {

        // memoize current position/folder
        Nav->freeze();
        /*
        String _currentPath = String(Nav->path);
        String _folderToScan = String(Nav->dir);
        uint16_t _Nav->pos = Nav->pos;
        uint16_t _Nav->num    = Nav->num;
        uint16_t _Nav->oldnum = Nav->oldnum;
        folderItemType_t _currentBrowsingType = currentBrowsingType;
        */

        scrollableText->scroll = false;
        explorer_needs_redraw = true;
        stoprender = true;
        adsrenabled = false;
        inactive_delay = 5000;
        delay(10);

        bool itemsfound = true;
        char *searchstr = (char*)sid_calloc(255,1);
        uint8_t len = 0;

        Nav->pos = 0;
        Nav->num = 0;
        Nav->oldnum = 0;

        const char* searchWelcomePage[] = {
          "Keyword search",
          "",
          "     ****     ",
          "",               // <CR>
          "  Type in to  ", // text
          "search for SID",
          "file or artist",
          "",               // <CR>
          "  Live long,  ",
          " and prosper! ",
          "",               // <CR>
          "     ****     "
        };

        drawToolPage( ">$", search_8x8_jpeg, search_8x8_jpeg_len, searchWelcomePage, 12, TL_DATUM );

        #if defined HID_USB
          // resume HID read
          beforeHIDRead();

          int keyPressed = 0;
          bool dismissed = false;

          do {

            HIDControlKey* cKey = getLastPressedKey();
            if( cKey == NULL ) {
              blinkCursor( ((len+3)*8)+2, 16, 8, 8, C64_LIGHTBLUE, C64_DARKBLUE );
              vTaskDelay(10);
              continue;
            }

            switch( cKey->control ) {
              case HID_NONE:
                // printable
                keyPressed = cKey->key;
                switch( keyPressed ) {
                  case -1: break; // unhandled non printable key
                  case 0x0d:  dismissed = true; break; // ENTER key
                  default:
                    // printable key (hopefully)
                    if( isprint(keyPressed)!=0 && len < 254 ) {
                      searchstr[len] = tolower(keyPressed);
                      searchstr[len+1] = '\0';
                      len++;
                      Nav->pos = 0;
                      itemsfound = drawSearchPage( searchstr, true, Nav->pos, itemsPerPage );
                    }
                  break;
                }
              break;
              case HID_ACTION4: // BACKSPACE
                if( len > 0 ) {
                  len--;
                  searchstr[len] = '\0';
                  Nav->pos = 0;
                  itemsfound = drawSearchPage( searchstr, true, Nav->pos, itemsPerPage );
                }
              break;
              case HID_ACTION5: // ESCAPE
                itemsfound = false;
                Nav->pos = 0;
                dismissed = true;
              break;
              case HID_DOWN: // arrow_down or pg_down
                if( Nav->pos < Nav->items->size()-1 ) Nav->pos++;
                else Nav->pos = 0;
                //itemsfound = drawSearchPage( searchstr, true, Nav->pos, itemsPerPage );
                afterHIDRead();
                drawPaginaged();
                beforeHIDRead();
              break;
              case HID_UP: // arrow_up or pg_up
                if( Nav->pos > 0 ) Nav->pos--;
                else Nav->pos = Nav->items->size()-1;
                //itemsfound = drawSearchPage( searchstr, true, Nav->pos, itemsPerPage );
                afterHIDRead();
                drawPaginaged();
                beforeHIDRead();
              break;
              default:
                // blink cursor
                //afterHIDRead();
                //beforeHIDRead();
              break; // other controls (e.g. escape key?)
            }

            vTaskDelay(1);
          } while( dismissed == false );

          afterHIDRead();

        #elif defined HID_SERIAL

          do {

            while(Serial.available()==0) {
              blinkCursor( ((len+3)*8)+2, 16, 8, 8, C64_LIGHTBLUE, C64_DARKBLUE );
              vTaskDelay(10);
            }

            // process serial
            String blah = Serial.readStringUntil('\n');
            blah.trim();
            if( blah.length() > 0 ) {
              snprintf( searchstr, 255, "%s", blah.c_str() );
              len = blah.length();
              searchstr[len] = '\0';
              itemsfound = drawSearchPage( searchstr, true, Nav->pos, itemsPerPage );
              if( itemsfound > 0 || blah == "!q" ) {
                break;
              }
            }

          } while( true );

        #else

        #endif

        free( searchstr );

        Nav->thaw();

        if( !itemsfound /*|| Nav->pos == 0*/ ) {
          // restore current position / folder
          getPaginatedFolder( Nav->dir, Nav->num, itemsPerPage  );
          explorer_needs_redraw = true;
          stoprender = true;
          inactive_delay = 5000;

        } else {

          currentBrowsingType = Nav->items->get(Nav->pos%itemsPerPage).type;
          Nav->setPath( Nav->items->get(Nav->pos%itemsPerPage).path.c_str() );

          #if defined HID_USB
            goto HID_PLAYSTOP;
          #endif
        }


      }
      break;

      //#endif // defined HID_USB

      case HID_DOWN: // down
        if( Nav->pos < Nav->items->size()-1 ) Nav->pos++;
        else Nav->pos = 0;
        explorer_needs_redraw = true;
        stoprender = true;
        inactive_delay = 5000;
        delay(10);
      break;
      case HID_UP: // up
           //HID_UP: // also a GOTO label
        if( Nav->pos > 0 ) Nav->pos--;
        else Nav->pos = Nav->items->size()-1;
        explorer_needs_redraw = true;
        stoprender = true;
        inactive_delay = 5000;
        delay(10);
      break;
      case HID_RIGHT: // right
        if( sidPlayer->isPlaying() ) {
          switch( currentBrowsingType ) {
            case F_SID_FILE: sidPlayer->playNextSongInSid(); break;
            case F_PLAYLIST:
              sidPlayer->stop();
              nextSIDEvent = SID_START_PLAY; // dispatch to handleAutoPlay
              stoprender = true;
              vTaskDelay(10);
              //nextInPlaylist();
            break;
            default: /* SHOULD NOT BE THERE */; break;
          }
          inactive_delay = 300;
        }
      break;
      case HID_LEFT: // left
        if( sidPlayer->isPlaying() ) {
          switch( currentBrowsingType ) {
            case F_SID_FILE: sidPlayer->playPrevSongInSid(); break;
            case F_PLAYLIST: prevInPlaylist(); break;
            default: /* SHOULD NOT BE THERE */; break;
          }
          inactive_delay = 300;
        }
      break;
      case HID_PLAYSTOP: // B (play/stop button)
        #if defined HID_USB || defined HID_SERIAL
          HID_PLAYSTOP: // also a GOTO label for HID search
        #endif

        if( !adsrenabled ) {

          switch( currentBrowsingType )
          {
            case F_SID_FILE:
              //sidPlayer->setLoopMode( SID_LOOP_ON );
              if( sidPlayer->getInfoFromSIDFile( Nav->path, track ) ) {
                sidPlayer->playSID( track );
                inactive_delay = 300;
                inactive_since = millis();
              }
              //inactive_since = millis() - inactive_delay;
            break;
            case F_PLAYLIST:
              sidPlayer->setLoopMode( SID_LOOP_OFF );
              if( SongCache->getInfoFromCacheItem( Nav->path, 0 ) ) {
                // playing first song in this cache
                sidPlayer->playSID( SongCache->currenttrack );
                inactive_delay = 300;
                inactive_since = millis();
              }
              //inactive_since = millis() - inactive_delay;
            break;
            case F_PARENT_FOLDER:
            case F_SUBFOLDER:
            case F_SUBFOLDER_NOCACHE:
              Nav->setDir( Nav->path );
              PlayerPrefs.setLastPath( Nav->dir );
              //log_d("#### will getPaginatedFolder");
              //getPaginatedFolder( Nav->dir, 0, maxitems );
              if( Nav->pos==0 && currentBrowsingType == F_PARENT_FOLDER ) {
                // going up
                if( folderDepth > 0 ) {
                  Nav->pos = lastItemCursor[folderDepth-1];
                } else {
                  Nav->pos = 0;
                }
                lastItemCursor[folderDepth] = 0;
              } else {
                // going down
                lastItemCursor[folderDepth] = Nav->pos;
                Nav->pos = 0;
              }
              if( getPaginatedFolder( Nav->dir, 0, maxitems ) ) {
                Nav->oldnum = 0;
                //setupPagination();
                explorer_needs_redraw = true;
                inactive_delay = 5000;
              } else {
                log_e("DUH");
              }
            break;
            case F_TOOL_CB:
              sidPlayer->kill();
              log_d("[%d] Invoking tool %s", ESP.getFreeHeap(), Nav->path );
              if       ( strcmp( Nav->path, "update-md5"         ) == 0 ) {
                updateMd5File();
              } else if( strcmp( Nav->path, "reset-path-cache"   ) == 0 ) {
                resetPathCache();
              } else if( strcmp( Nav->path, "reset-spread-cache" ) == 0 ) {
                resetSpreadCache();
              } else if( strcmp( Nav->path, "reset-words-cache" ) == 0 ) {
                resetWordsCache();
              }
              inactive_delay = 5000;
            break;
          }
        } else {
          log_e("Stopping sidPlayer");
          sidPlayer->stop();
          inactive_delay = 5000;
          vTaskDelay(100);
        }
        explorer_needs_redraw = true;
      break;
      case HID_ACTION1: // Action button
        // circular toggle
        if( adsrenabled ) {
          switch (playerloopmode) {
            case SID_LOOP_OFF:    setUIPlayerMode( SID_LOOP_ON );     break;
            case SID_LOOP_ON:     setUIPlayerMode( SID_LOOP_RANDOM ); break;
            case SID_LOOP_RANDOM: setUIPlayerMode( SID_LOOP_OFF );    break;
          };
          PlayerPrefs.setLoopMode( playerloopmode ); // save to NVS
          sidPlayer->setLoopMode( playerloopmode );
          inactive_delay = 300;
        } else {
          // only when the ".." folder is being selected
          if( currentBrowsingType == F_PARENT_FOLDER ) {
            log_d("Refresh folder -> regenerate %s Dir List and Songs Cache", Nav->dir );
            delete( sidPlayer->MD5Parser );
            sidPlayer->MD5Parser = NULL;
            log_d("#### will getPaginatedFolder");
            getPaginatedFolder( Nav->dir, 0, maxitems, true );
            sidPlayer->setMD5Parser( cfg );
            explorer_needs_redraw = true;
            inactive_delay = 5000;
          } else {
            // TODO: show information screen on current selected item
            // folder = show dircache count or suggest creating dircache
            // playlist = show playlist length
            // sid file = author, name, pub, subsongs, lengths
          }
        }
      break;
      case HID_ACTION2: // C
        if( maxVolume > 0 ) {
          maxVolume--;
          sidPlayer->setMaxVolume( maxVolume ); //value between 0 and 15
          log_d("New volume level: %d", maxVolume );
          PlayerPrefs.setVolume( maxVolume );
        }
      break;
      case HID_ACTION3: // D
        if( maxVolume < 15 ) {
          maxVolume++;
          sidPlayer->setMaxVolume( maxVolume ); //value between 0 and 15
          log_d("New volume level: %d", maxVolume );
          PlayerPrefs.setVolume( maxVolume );
        }
      break;
      default: // simultaneous buttons push ?
        log_w("Unhandled button combination: %X", resp );
        inactive_delay = 5000;
      break;

    }
    lastresp = resp;
    lastpush = millis();
    beforeHIDRead(); // resume HID bus if necessary
  }
}



void SIDExplorer::setUIPlayerMode( loopmode mode )
{

  switch (mode) {
    case SID_LOOP_OFF:
      loopmodeicon     = (const char*)single_jpg;
      loopmodeicon_len = single_jpg_len;
    break;
    case SID_LOOP_ON:
      loopmodeicon     = (const char*)iconloop_jpg;
      loopmodeicon_len = iconloop_jpg_len;
    break;
    case SID_LOOP_RANDOM:
      loopmodeicon     = (const char*)iconrandom_jpg;
      loopmodeicon_len = iconrandom_jpg_len;
    break;
    /*
      loopmodeicon     = (const char*)iconsidfile_jpg;
      loopmodeicon     = (const char*)iconlist_jpg;
    */
  };
  playerloopmode = mode;
}



void SIDExplorer::setupPagination()
{
  Nav->oldnum = Nav->num;
  Nav->num    = Nav->pos/itemsPerPage;
  if( Nav->items->size() > 0 ) {
    Nav->count = ceil( float(Nav->items->size())/float(itemsPerPage) );
  } else {
    Nav->count = 1;
  }
  Nav->start   = Nav->num*itemsPerPage;
  Nav->end     = Nav->start+itemsPerPage;

  if( Nav->num != Nav->oldnum ) {
    // infoWindow( " READING $ " );
    // page change, load paginated cache !
    log_d("#### will getPaginatedFolder");
    getPaginatedFolder( Nav->dir, Nav->start, itemsPerPage  );
    // simulate a push-release to prevent multiple action detection
    lastpush = millis();
  }

  int ispaginated = (Nav->start >= itemsPerPage || Nav->end-Nav->start < itemsPerPage) ? 1 : 0;

  // cap last page
  if( Nav->end > Nav->items->size()+1 ) {
    Nav->end = Nav->items->size()+ispaginated;
  }

  log_d("[%d]$> cur=%d page[%d/%d] range[%d=>%d] fsize=%d path=%s",
    ESP.getFreeHeap(),
    //folderToScan,
    Nav->pos,
    Nav->num+1,
    Nav->count,
    Nav->start,
    Nav->end,
    Nav->items->size(),
    Nav->items->get(Nav->pos%itemsPerPage).path.c_str()
  );
}



void SIDExplorer::drawPaginaged()
{

  int HeaderHeight = 16;
  int spriteYpos   = HeaderHeight;
  const uint16_t ydislistpos = lineHeight*1.5;

  //sptr = (uint16_t*)spritePaginate->createSprite(tft.width(), tft.height()-HeaderHeight );
  if( !sptr ) {
    log_v("Will use 8bits sprite for pagination");
    spritePaginate->setColorDepth(8);
    sptr = (uint16_t*)spritePaginate->createSprite(tft.width(), tft.height()-HeaderHeight );
    if( !sptr ) {
      log_e("Unable to create 8bits sprite for pagination, giving up");
      return;
    }
  }
  spriteHeight = spritePaginate->height(); // for bottom-alignment

  //tft.startWrite();

  spritePaginate->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
  spritePaginate->fillSprite( C64_DARKBLUE );
  spritePaginate->setTextWrap( false );
  spritePaginate->setTextDatum( TC_DATUM );
  spritePaginate->setFont( &Font8x8C64 );

  spritePaginate->setTextDatum( TL_DATUM );

  folderDepth = 0;
  for( int i=0; i<strlen(Nav->dir); i++ ) {
    if( Nav->dir[i] == '/' ) folderDepth++;
  }

  if( spritePaginate->textWidth( Nav->dir )+2 > spriteWidth  ) {
    String bname = gnu_basename( Nav->dir );
    snprintf( lineText, 256, ">> %s", bname.c_str() );
  } else {
    snprintf( lineText, 256, "$> %s", Nav->dir );
  }
  spritePaginate->drawString( lineText, 2, 0 );

  for(size_t i=Nav->start,j=0; i<Nav->end; i++,j++ ) {
    uint16_t yitempos = ydislistpos + j*lineHeight;
    switch( Nav->items->get(i%itemsPerPage).type )
    {
      case F_PLAYLIST:
        sprintf( lineText, "%s", "Play all"/*Nav->items->get(i).name.c_str()*/ );
        spritePaginate->drawJpg( iconlist_jpg, iconlist_jpg_len, 2, yitempos );
        break;
      case F_SUBFOLDER:
        sprintf( lineText, "%s", Nav->items->get(i%itemsPerPage).name.c_str() );
        spritePaginate->drawJpg( iconfolder2_jpg, iconfolder2_jpg_len, 2, yitempos );
        break;
      case F_SUBFOLDER_NOCACHE:
        sprintf( lineText, "%s", Nav->items->get(i%itemsPerPage).name.c_str() );
        spritePaginate->drawJpg( iconfolder1_jpg, iconfolder1_jpg_len, 2, yitempos );
        break;
      case F_PARENT_FOLDER:
        sprintf( lineText, "%s", Nav->items->get(i%itemsPerPage).name.c_str() );
        spritePaginate->drawJpg( iconup_jpg, iconup_jpg_len, 2, yitempos );
        break;
      case F_TOOL_CB:
        sprintf( lineText, "%s", Nav->items->get(i%itemsPerPage).name.c_str() );
        spritePaginate->drawJpg( icontool_jpg, icontool_jpg_len, 2, yitempos );
        break;
      case F_SID_FILE:
        String basename = gnu_basename( Nav->items->get(i%itemsPerPage).path.c_str() );
        sprintf( lineText, "%s", basename.c_str() );
        spritePaginate->drawJpg( iconsidfile_jpg, iconsidfile_jpg_len, 2, yitempos );
        break;
    }
    if( i==Nav->pos ) {
      // highlight
      spritePaginate->setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
      // also animate folder name
      if( strcmp( lineText, ".." ) == 0 ) {
          if( spritePaginate->textWidth( Nav->dir ) > minScrollableWidth-10 ) {
            scrollableText->setup( Nav->dir, 24, spriteYpos, minScrollableWidth-10, lineHeight, 30, false );
          } else {
            scrollableText->scroll = false;
          }
      } else if( spritePaginate->textWidth( lineText ) > minScrollableWidth ) {
        scrollableText->setup( lineText, 14, yitempos+spriteYpos, minScrollableWidth, lineHeight, 30 );
      } else {
        scrollableText->scroll = false;
      }
    } else {
      spritePaginate->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    }
    spritePaginate->drawString( lineText, 14, yitempos );
  }

  spritePaginate->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
  sprintf(lineText, "%d/%d", Nav->num+1, Nav->count );
  spritePaginate->setTextDatum( TL_DATUM );
  spritePaginate->drawString( lineText, 16, spriteHeight-lineHeight );
  spritePaginate->drawJpg( iconinfo_jpg, iconinfo_jpg_len, 2, spriteHeight-lineHeight ); // clear disk icon
  sprintf(lineText, "#%d", Nav->items->size() );
  spritePaginate->setTextDatum( TR_DATUM );
  spritePaginate->drawString( lineText, spriteWidth -2, spriteHeight-lineHeight );

  if( firstRender ) {
    firstRender = false;
    float speed = (float(tft.height())/float(tft.width()))*4.0;
    for( int i=spriteHeight; i>=spriteYpos; i-=speed ) {
      spritePaginate->pushSprite( 0, i );
    }
    drawHeader();
    setupTouchButtons( spritePaginate, 0, (spritePaginate->height()-(TouchPadHeight+16)), spritePaginate->width(), TouchPadHeight, 0, spriteYpos );
    drawTouchButtons();
    spritePaginate->pushSprite( 0, spriteYpos );
    //soundIntro( &sidPlayer->sid );
  } else {
    drawHeader();
    drawTouchButtons();
    spritePaginate->pushSprite( 0, spriteYpos );
  }
  //spritePaginate->deleteSprite();

}



bool SIDExplorer::getPaginatedFolder( const char* folderName, size_t offset, size_t maxitems, bool force_regen )
{
  Nav->items->clear();
  Nav->items->folderName = folderName;

  if( String( folderName ) != "/"  ) { // no parent link for root folder
    if( offset == 0 ) {
      String dirname = gnu_basename( String(folderName).c_str() );
      String parentFolder = String( folderName ).substring( 0, strlen( folderName ) - (dirname.length()+1) );
      if( parentFolder == "" ) parentFolder = "/";
      log_d("Pushing %s as '..' parent folder (from folderName=%s, dirname=%s)", parentFolder.c_str(), folderName, dirname.c_str() );
      Nav->items->push_back( { String(".."), parentFolder, F_PARENT_FOLDER } );
      //maxitems--;
    }
  }

  diskTickerWordWrap = 0;
  diskTickerMsec = millis();

  if( strcmp( folderName, cfg->md5folder ) == 0 ) {
    // md5 folder tools
    Nav->items->push_back( { "Update MD5",   "update-md5",         F_TOOL_CB } );
    Nav->items->push_back( { "Reset pcache", "reset-path-cache",   F_TOOL_CB } );
    Nav->items->push_back( { "Reset scache", "reset-spread-cache", F_TOOL_CB } );
    Nav->items->push_back( { "Reset wcache", "reset-words-cache",  F_TOOL_CB } );
    return true;
  }

  if( force_regen || ! SongCache->folderHasCache( folderName ) ) {
    infoWindow( " WRITING $ " );
    log_d("SongCache->scanFolder( '%s', cb, cb, %d, %s )", folderName, itemsPerPage, force_regen ? "true" : "false" );
    if( !SongCache->scanFolder( folderName, &addFolderElement, &diskTicker, itemsPerPage, force_regen ) ) {
      Nav->items->clear();
      log_e("Could not build a list of songs");
      return false;
    }
  } else {
    infoWindow( " READING $ " );
    log_d("SongCache->scanPaginatedFolder( '%s', cb, cb, %d, %d )", folderName, offset, itemsPerPage );
    if( !SongCache->scanPaginatedFolder( folderName, &addFolderElement, &diskTicker, offset, itemsPerPage ) ) {
      Nav->items->clear();
      log_e("Could not build a list of songs");
      return false;
    }
  }

  // only $itemsPerPage elements are present in Nav->items
  // but Nav->items->size() needs to report the full count
  size_t oldsize = Nav->items->size();
  size_t newsize = SongCache->getDirCacheSize( folderName );

  /*
  if( SongCache->folderHasPlaylist() ) {
    // add the "Playlist" element to the count
    newsize++;
  }
  */

  if( String( folderName ) != "/" && offset==0 ) {
    // add the ".." parent folder to the count, even if not visible
    newsize++;
  }

  if( oldsize != newsize ) {
    Nav->items->_size = newsize;
    log_d("Adjusted folder '%s' size from %d to %d (offset=%d, maxitems=%d)", folderName, oldsize, newsize, offset, maxitems );
    if( newsize == 0 ) {
      log_e("WTF");
      return false;
    }
  } else {
    log_d("Folder '%s' size =%d (offset=%d, maxitems=%d)", folderName, Nav->items->_size, offset, maxitems );
  }

  return true;
}



void SIDExplorer::drawToolPage( const char* title, const unsigned char* img, size_t img_len,  const char* text[], size_t linescount, uint8_t ttDatum, uint8_t txDatum )
{
  tft.fillScreen( C64_DARKBLUE );
  drawHeader();
  tft.setFont( &Font8x8C64 );
  tft.setTextDatum( ttDatum );
  tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
  tft.drawString( title, ttDatum == TC_DATUM ? tft.width()/2 : 8, 16 );
  tft.drawJpg( img, img_len, 4, 4 );
  tft.setTextDatum( txDatum );
  if( linescount>0 && text[0] != nullptr ) {
    log_w("rendering %d lines", linescount);
    for( int i=0;i<linescount;i++) {
      if( text[i] != nullptr && text[i][0] !='\0' ) {
        tft.drawString( text[i], 8, 32+(i*8) );
      }
    }
  }
}



void SIDExplorer::resetPathCache()
{
  drawToolPage( "Building hash cache" );
  cfg->fs->remove( cfg->md5idxpath );
  sidPlayer->MD5Parser->MD5Index->buildSIDPathIndex( cfg->md5filepath, cfg->md5idxpath );
  tft.drawString( "Done!", tft.width()/2, 42 );
  delay(3000);
}



void SIDExplorer::resetSpreadCache()
{
  drawToolPage( "Building spread cache" );
  sidPlayer->MD5Parser->MD5Index->buildSIDHashIndex( cfg->md5filepath, cfg->md5folder, true );
  tft.drawString( "Done!", tft.width()/2, 42 );
  delay(3000);
}



void SIDExplorer::resetWordsCache()
{
  drawToolPage( "Sharding words" );
  int cx = tft.width()/2, cy = cx;
  tft.drawJpg( insertdisk_jpg, insertdisk_jpg_len, cx - 44, cy - 9 ); // disk icon
  // TODO: create sprite
  tft.touch()->sleep();

  //UISprite->setPsram(false);
  UISprite->setColorDepth(8);
  UISprite->fillSprite( TFT_ORANGE ); // transp color
  sptr = (uint16_t*)UISprite->createSprite(tft.width(), tft.height() );
  if( !sptr ) {
    log_e("Unable to create 16bits sprite for pagination, trying 8 bits");
    return;
  }
  UISprite->setTextDatum( TL_DATUM );
  UISprite->setFont( &Font8x8C64 );
  UISprite->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );

  indexTaskRunning = true;
  xTaskCreatePinnedToCore( buildSIDWordsIndexTask, "buildSIDWordsIndex", 16384, this, 16, NULL, M5.SD_CORE_ID /*SID_DRAWTASK_CORE*/ ); // will trigger TFT writes

  while( indexTaskRunning ) vTaskDelay(1000);

/*
  if( ! SongCache->buildSIDWordsIndex( cfg->sidfolder, &keywordsTicker, &keyWordsProgress ) ) {
    log_e("DUH");
  }
*/

  UISprite->deleteSprite();

  tft.drawString( "Done!", tft.width()/2, 42 );
  delay(3000);
}



void SIDExplorer::updateMd5File()
{
  drawToolPage( "Downloading MD5 File" );
  deInitSprites(); // free some ram
  if( SidArchiveChecker == NULL )
    SidArchiveChecker = new SID_Archive_Checker( cfg );
  SidArchiveChecker->checkMd5File( true );
  tft.drawString( "Done!", tft.width()/2, 42 );
  delay(3000);
  #ifdef SID_DOWNLOAD_ARCHIVES
  if( SidArchiveChecker->wifi_occured ) {
    // at this stage the heap is probably too low to do anything else
    ESP.restart();
  }
  #endif
  initSprites();
}



void SIDExplorer::checkArchive( bool force_check )
{
  //afterHIDRead();
  //M5.sd_begin();
  //M5.sd_end();
  delay(100);
  log_v("#### will check/create md5 file path");
  mkdirp( fs, cfg->md5filepath ); // from ESP32-Targz: create traversing directories if none exist
  log_v("#### will getPaginatedFolder");
  bool folderSorted = getPaginatedFolder( Nav->dir, 0, maxitems );

  if( force_check || !folderSorted || !fs->exists( cfg->md5filepath ) ) {
    // first run ?
    //TODO: confirmation dialog
    drawToolPage( "Start WiFi?");
    uint8_t hidread = 0;

    beforeHIDRead();

    while( hidread==0 ) {
      delay(100);
      hidread = readHID();
    }

    afterHIDRead();

    if( hidread != HID_PROMPT_N && cfg->archive != nullptr ) {
      deInitSprites(); // free some ram
      SidArchiveChecker = new SID_Archive_Checker( cfg );
      SidArchiveChecker->checkArchive();
      #ifdef SID_DOWNLOAD_ARCHIVES
      if( SidArchiveChecker->wifi_occured ) {
        // at this stage the heap is probably too low to do anything else
        ESP.restart();
      }
      #endif
      initSprites();
    } else {
      log_e("No archive to check for .. aborting !");
      return;
    }
  } else {
    //TODO: confirmation dialog
    // give some hints for building the archives ?
  }
}



void SIDExplorer::initSprites()
{
  spriteText->setPsram(false);
  spriteText->setColorDepth(lgfx::palette_1bit);
  sptr = (uint16_t*)spriteText->createSprite( tft.width(), 8 );
  if( !sptr ) {
    log_e("Unable to create spriteText");
  } else {
    spriteText->setPaletteColor(0, C64_DARKBLUE );
    spriteText->setPaletteColor(1, C64_LIGHTBLUE );
  }
  for( int i=0;i<3;i++ ) {
    oscViews[i]->init( &sidPlayer->sid );
    //vTaskDelay(1);
  }
}



void SIDExplorer::deInitSprites()
{
  spriteText->deleteSprite();
  for( int i=0;i<3;i++ ) {
    oscViews[i]->deinit();
  }
}



void SIDExplorer::infoWindow( const char* text )
{
  int cx = tft.width()/2, cy = cx;
  log_d( "%s", text );

  tft.fillRect( cx-64, cy-36, 128, 82, C64_DARKBLUE );
  tft.drawRect( cx-50, cy-28, 100, 56, C64_LIGHTBLUE );

  tft.setTextStyle( spriteHeader->getTextStyle() );

  tft.setTextDatum( MC_DATUM );
  tft.setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
  tft.setFont( &Font8x8C64 );
  // disk icon is 88*26
  tft.drawJpg( insertdisk_jpg, insertdisk_jpg_len, cx - 44, cy - 13 ); // disk icon
  tft.drawString( text, cx, cy-28 );
}



void SIDExplorer::loadingScreen()
{
  tft.fillScreen( C64_DARKBLUE ); // Commodore64 blueish
  tft.drawJpg( header128x32_jpg, header128x32_jpg_len, 0, 0 );
  bool toggle = false;
  auto lastcheck = millis();
  // 88x26
  int xpos = tft.width()/2  - 88/2;
  int ypos = tft.height()/2 - 26/2;
  tft.drawJpg( insertdisk_jpg, insertdisk_jpg_len, xpos, ypos );
  tft.setTextDatum( MC_DATUM );
  tft.setFont( &Font8x8C64 );
  tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
  tft.drawString( "** Insert SD **", tft.width()/2, ypos-10 );

  while( !M5.sd_begin() )
  {
    // TODO: make a more fancy animation
    toggle = !toggle;
    tft.setTextColor( toggle ? TFT_BLACK : TFT_WHITE );
    tft.drawFastHLine( xpos+15, ypos+22, 4, toggle ? TFT_RED : TFT_ORANGE );
    delay( toggle ? 300 : 500 );
    // go to sleep after a minute, no need to hammer the SD Card reader
    if( lastcheck + 60000 < millis() ) {
      // TODO: deep sleep
    }
  }
  tft.fillRect( xpos, ypos, 88, 26, C64_DARKBLUE );


  tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
  tft.drawString( "Checking MD5", tft.width()/2, tft.height()/2 );
  tft.setFont( &Font0 );
  //delay( 1000 );
}



void SIDExplorer::animateView()
{
  if( millis() - inactive_delay < inactive_since ) { // still debouncing from last action
    stoprender = true; // send stop signal to drawVoices task
    vTaskDelay(10);
    renderVoicesTaskHandle = NULL;
    takeSidMuxSemaphore();
    scrollableText->doscroll(); // animate text overflow if any
  } else {
    if( sidPlayer->isPlaying() ) {
      if( renderVoicesTaskHandle == NULL  && ! adsrenabled ) {
        // ADSR not running, draw UI and run task
        log_d("[%d] launching renverVoices task (inactive_delay=%d, inactive_since=%d)", ESP.getFreeHeap(), inactive_delay, inactive_since );

        drawHeader();
        positionInPlaylist = -1; // make sure the title is displayed
        takeSidMuxSemaphore();
        tft.fillRect(0, 16, tft.width(), 16, C64_DARKBLUE ); // clear area under the logo
        xTaskCreatePinnedToCore( drawVoices, "drawVoices", 4096, this, SID_DRAWTASK_PRIORITY, &renderVoicesTaskHandle, M5.SD_CORE_ID /*SID_DRAWTASK_CORE*/ ); // will trigger TFT writes
        adsrenabled = true;
      } else {
        // ADSR already running or stopping
        return;
      }
    } else {
      stoprender = true; // send stop signal to drawVoices task
      vTaskDelay(10);
      renderVoicesTaskHandle = NULL;
      takeSidMuxSemaphore();
      scrollableText->doscroll(); // animate text overflow if any
      if( millis() > sleep_delay && millis() - sleep_delay > inactive_since ) {
        // go to sleep
        sidSleep();
      }
    }
  }
  giveSidMuxSemaphore();
}



void SIDExplorer::drawHeader()
{
  sptr = (uint16_t*)spriteHeader->createSprite( tft.width(), 16 );
  if( !sptr) {
    log_e("Unable to draw header");
    return;
  }
  spriteHeader->drawFastHLine( 0, 0, tft.width(), C64_MAROON_DARKER );
  spriteHeader->drawFastHLine( 0, 1, tft.width(), C64_MAROON_DARK );
  spriteHeader->fillRect( 0, 2, tft.width(), 12, C64_MAROON );
  spriteHeader->drawJpg( header48x12_jpg, header48x12_jpg_len, 41, 2 );
  spriteHeader->drawFastHLine( 0, 14, tft.width(), C64_MAROON_DARK );
  spriteHeader->drawFastHLine( 0, 15, tft.width(), C64_MAROON_DARKER );
  spriteHeader->pushSprite(0, 0);
  spriteHeader->deleteSprite();
}



bool SIDExplorer::drawSearchPage( char* searchstr, bool search, size_t itemselected, size_t maxitems )
{
  afterHIDRead();
  bool ret = false;

  if( searchstr != nullptr ) {
    // search_8x8_jpeg_len
    infoWindow("Searching...");
  } else {
    // todo: display hint "waiting for user input"
  }

  Nav->items->clear();

  if( searchstr != NULL && searchstr[0] != 0 ) {
    Nav->items->folderName = searchstr;
    Nav->items->push_back( { String("<- Back"), Nav->path, F_PARENT_FOLDER } );
    log_w("Will search for keyword: %s at offset %d with %d max results", searchstr, 0, maxitems );
    if( SongCache->find( searchstr, &addFolderElement, 0, maxitems ) ) {
      if( Nav->items->size() > 1 ) {
        std::sort(Nav->items->begin(), Nav->items->end(), [=](folderTypeItem_t a, folderTypeItem_t b) {
          if( a.type == F_PARENT_FOLDER ) return true;  // hoist
          if( b.type == F_PARENT_FOLDER ) return false; // hoist
          if( a.type != b.type ) {
            if( a.type == F_SUBFOLDER ) return true; // hoist
            if( b.type == F_SUBFOLDER ) return false; // hoist
            return false;
          } else {
            return a.name[0] < b.name[0]; // sort
          }
        });
      }
      Nav->setDir( searchstr );
      ret = true;
    } else {
      Nav->setDir( " " );
      Nav->pos = 0;
    }
  } else {
    Nav->setDir( " " );
    Nav->pos = 0;
  }
  setupPagination();
  drawPaginaged();
  beforeHIDRead();
  return ret;

  /*
  int HeaderHeight = 16;
  int cx = tft.width()/2, cy = cx - HeaderHeight;
  //int maxTextWidth = tft.width() - (2*8); // screen width minus 2 char
  int maxTextLen = 12;

  sptr = (uint16_t*)spritePaginate->createSprite(tft.width(), tft.height()-HeaderHeight );
  if( !sptr ) {
    log_e("Unable to create 16bits sprite for pagination, trying 8 bits");
    spritePaginate->setColorDepth(8);
    sptr = (uint16_t*)spritePaginate->createSprite(tft.width(), tft.height()-HeaderHeight );
    if( !sptr ) {
      log_e("Unable to create 8bits sprite for pagination, giving up");
      return;
    }
  }

  spritePaginate->fillRect( cx-64, cy-36, 128, 82, C64_DARKBLUE );
  spritePaginate->drawRect( cx-56, cy-28, 112, 24, C64_LIGHTBLUE );

  spritePaginate->setTextDatum( MC_DATUM );
  spritePaginate->setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
  spritePaginate->setFont( &Font8x8C64 );

  spritePaginate->drawString( " -= Search =- ", cx, cy-28 );

  if( searchstr != NULL && searchstr[0] != '\0' ) {
    spritePaginate->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    spritePaginate->setTextDatum( ML_DATUM );
    size_t len = strlen(searchstr);

    if( len > maxTextLen ) { // negative pad left
      int idx = len-maxTextLen;
      spritePaginate->drawString( (char*)&searchstr[idx], cx - 48, cy - 15 );
    } else {
      spritePaginate->drawString( searchstr, cx - 48, cy - 15 );
    }

    if( search ) {
      // DO SEARCH
      log_w("Will search for keyword: %s at offset %d with %d max results", searchstr, itemselected, maxitems );
      SongCache->findFolder( searchstr, &PrintSearchResult, itemselected, maxitems );
      drawPaginaged();
    }

  } else {
    //spritePaginate->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    //spritePaginate->fillRect( cx - 48, cy - 13, 8, 8, C64_LIGHTBLUE );
    spritePaginate->drawString( " ", cx - 48, cy - 15 );
  }

  spritePaginate->pushSprite( 0, HeaderHeight, TFT_BLACK );

  spritePaginate->deleteSprite();
  */

}



void SIDExplorer::drawVolumeIcon( int16_t x, int16_t y, uint8_t volume )
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



void SIDExplorer::drawSongHeader( SIDExplorer* mySIDExplorer, char* songNameStr, char* filenumStr, bool &scrolltext )
{

  mySIDExplorer->spriteText->fillSprite(C64_DARKBLUE);
  mySIDExplorer->spriteText->setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
  mySIDExplorer->spriteText->setFont( &Font8x8C64 );
  mySIDExplorer->spriteText->setTextSize(1);

  snprintf( songNameStr, 255, " %s by %s (c) %s ", sidPlayer->getName(), sidPlayer->getAuthor(), sidPlayer->getPublished() );
  sprintf( filenumStr, " Song:%2d/%-2d  Track:%3d/%-3d ", sidPlayer->getCurrentSong()+1, sidPlayer->getSongsCount(), SongCache->CacheItemNum+1, Nav->items->size() );

  tft.startWrite();

  tft.drawJpg( (const uint8_t*)mySIDExplorer->loopmodeicon, mySIDExplorer->loopmodeicon_len, 0, 16 );
  drawVolumeIcon( tft.width()-9, 16, maxVolume );
  uint16_t scrollwidth = tft.width();
  scrollwidth/=8;
  scrollwidth*=8;
  if( scrollwidth < tft.textWidth( songNameStr ) ) {
    //Serial.println("Reset scroll");
    if( !scrolltext ) {
      scrollableText->setup( songNameStr, 0, 32, scrollwidth, tft.fontHeight(), 30, true );
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



void SIDExplorer::drawSongProgress( unsigned long currentprogress, uint8_t deltaMM, uint8_t deltaSS )
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
    (sidPlayer->getCurrentTrackDuration()/60000)%60,
    (sidPlayer->getCurrentTrackDuration()/1000)%60
  );
  tft.drawString( timeStr, tft.width()-1, 4 );
  // progressbar
  tft.drawFastHLine( 0,         26, len_light, C64_MAROON );
  tft.drawFastHLine( 0,         27, len_light, TFT_WHITE );
  tft.drawFastHLine( 0,         28, len_light, C64_LIGHTBLUE );
  tft.drawFastHLine( 0,         29, len_light, C64_MAROON_DARK );
  tft.fillRect( len_light, 26, len_dark, 4, C64_DARKBLUE );

  tft.endWrite();
}



void SIDExplorer::drawVoices( void* param )
{

  log_d("Attached TFT Operations to core #%d", xPortGetCoreID() );
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
          lastMM           = 0xff;
  uint8_t deltaSS          = 0,
          lastSS           = 0xff;
  bool    draw_header      = false;
  bool    scrolltext       = false;

  if( param == NULL ) {
    log_e("Can't attach voice renderer to SIDExplorer");
    stoprender = false;
    log_d("[%d] Task will self-delete", ESP.getFreeHeap() );
    vTaskDelete( NULL );
  }

  mySIDExplorer  = (SIDExplorer *) param;
  playerloopmode = mySIDExplorer->playerloopmode;

  takeSidMuxSemaphore();
  mySIDExplorer->initSprites();
  giveSidMuxSemaphore();
  //tft.fillRect(0, 16, tft.width(), 16, C64_DARKBLUE ); // clear area under the logo

  stoprender = false; // make the task stoppable from outside
  render     = true;

  while( stoprender == false ) {
    //if( stoprender ) break;
    uint32_t song_duration = sidPlayer->getCurrentTrackDuration();

    if( song_duration == 0 || render == false ) {
      // silence ?
      vTaskDelay( 1 );
      continue;
    }

    // track progress
    auto currentprogress = (100*sidPlayer->currenttune->delta_song_duration) / song_duration;
    deltaMM = (sidPlayer->currenttune->delta_song_duration/60000)%60;
    deltaSS = (sidPlayer->currenttune->delta_song_duration/1000)%60;

    if( deltaMM != lastMM || deltaSS != lastSS ) {
      lastMM = deltaMM;
      lastSS = deltaSS;
      lastprogress = 0; // force progress render
    }

    if( lastsubsong != sidPlayer->currenttune->currentsong || SongCache->CacheItemNum != lastCacheItemNum ) {
      lastsubsong = sidPlayer->currenttune->currentsong;
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
      takeSidMuxSemaphore();
      scrollableText->doscroll();
      giveSidMuxSemaphore();
    }

    if( lastprogress != currentprogress ) {
      lastprogress = currentprogress;
      if( currentprogress > 0 ) {
        takeSidMuxSemaphore();
        drawSongProgress( currentprogress, deltaMM, deltaSS );
        giveSidMuxSemaphore();
      } else {
        // clear progress zone
        takeSidMuxSemaphore();
        tft.fillRect( 0, 25, tft.width(), 6, C64_DARKBLUE );
        giveSidMuxSemaphore();
      }
    }

    if( draw_header ) {
      draw_header = false;
      takeSidMuxSemaphore();
      drawSongHeader( mySIDExplorer, songNameStr, filenumStr, scrolltext );
      giveSidMuxSemaphore();
    }

    for( int voice=0; voice<3; voice++ ) {
      takeSidMuxSemaphore();
      oscViews[voice]->drawEnveloppe( voice, 0, HEADER_HEIGHT + ( voice*VOICE_HEIGHT ) +1 );
      giveSidMuxSemaphore();
      vTaskDelay(1);
    }


  }
  stoprender = false;
  log_d("[%d] Task will self-delete", ESP.getFreeHeap() );
  mySIDExplorer->deInitSprites();
  mySIDExplorer->explorer_needs_redraw = true;
  vTaskDelete( NULL );
}



void SIDExplorer::eventCallback( SIDTunesPlayer* player, sidEvent event )
{
  lastSIDEvent = event;
  lastEvent = millis();
  //vTaskDelay(1);

  switch( event ) {
    case SID_NEW_FILE:
      render = true;
      log_d( "[%d] SID_NEW_FILE: %s (%d songs)", ESP.getFreeHeap(), sidPlayer->getFilename(), sidPlayer->getSongsCount() );
    break;
    case SID_END_FILE:
      render = false;
      log_n( "[%d] SID_END_FILE: %s", ESP.getFreeHeap(), sidPlayer->getFilename() );
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
      /*
      if( autoplay ) {
        playNextTrack();
      } else {
        log_n("All %d tracks have been played\n", songList.size() );
        while(1) vTaskDelay(1);
        play_ended = true;
      }
      */
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
      log_d("[%d] SID_STOP_TRACK", ESP.getFreeHeap());
    break;
  }
}



void SIDExplorer::addFolderElement( const char* fullpath, folderItemType_t type )
{
  //static int elementsInFolder = 0;
  static String basepath = "";
  //elementsInFolder++;
  basepath = gnu_basename( fullpath );
  Nav->items->push_back( { basepath, String(fullpath), type } );
  switch( type )
  {
    case F_PLAYLIST:          log_d("[%d] Playlist Found %s/.sidcache : [%s]", ESP.getFreeHeap(), fullpath, basepath.c_str() ); break;
    case F_SUBFOLDER:         log_d("[%d] Sublist Found %s/.sidcache : [%s]",  ESP.getFreeHeap(), fullpath, basepath.c_str() ); break;
    case F_SUBFOLDER_NOCACHE: log_d("[%d] Subfolder Found :%s : %s",           ESP.getFreeHeap(), fullpath, basepath.c_str() ); break;
    case F_PARENT_FOLDER:     log_d("[%d] Parent folder: %s : %s",             ESP.getFreeHeap(), fullpath, basepath.c_str() ); break;
    case F_SID_FILE:          log_d("[%d] SID File: %s : %s",                  ESP.getFreeHeap(), fullpath, basepath.c_str() ); break;
    case F_TOOL_CB:           log_d("[%d] Tool file: %s : %s",                 ESP.getFreeHeap(), fullpath, basepath.c_str() ); break;
  }
  basepath = "";
  diskTicker( "", Nav->items->size() );
}



void SIDExplorer::buildSIDWordsIndexTask( void* param )
{
  SIDExplorer* mySIDExplorer = (SIDExplorer*)param;
  indexTaskRunning = true;
  if( ! SongCache->buildSIDWordsIndex( mySIDExplorer->cfg->sidfolder, &keywordsTicker, &keyWordsProgress ) ) {
    log_e("DUH");
  }
  indexTaskRunning = false;
  vTaskDelete(NULL);
}



bool SIDExplorer::keywordsTicker( const char* title, size_t totalitems )
{
  static const char* w[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
  static bool toggle = false;
  static auto lastTickerMsec = millis();
  for(int i=0; i<4; i++ ) {
    w[i] = w[i+1];
  }
  w[4] = title;

  if( millis() - lastTickerMsec > 150 ) {
    UISprite->fillSprite( TFT_ORANGE );
    int yoffset = UISprite->height()-40;
    char line[17] = {0};
    toggle = !toggle;
    UISprite->setTextDatum( TL_DATUM );
    UISprite->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    UISprite->drawFastHLine( (UISprite->width()/2-44)+15, (UISprite->height()/2-9)+22, 4, toggle ? TFT_RED : TFT_ORANGE );
    for(int i=0; i<5; i++ ) {
      int ypos = i*8 + yoffset;
      if( w[i] != nullptr ) {
        snprintf( line, 15, " %-14s", w[i] );
        UISprite->drawString( line, 0, ypos );
      }
    }
    UISprite->pushSprite(0, 0, TFT_ORANGE );
    lastTickerMsec = millis();
  }
  return true;
}



void SIDExplorer::keyWordsProgress( size_t current, size_t total, size_t words_count, size_t files_count, size_t folders_count  )
{
  static size_t lastprogress = 0;
  size_t progress = (100*current)/total;
  if( progress != lastprogress ) {
    lastprogress = progress;
    uint32_t ramUsed = ramSize - ESP.getFreePsram();
    uint32_t ramUsage = (ramUsed*100) / ramSize;
    char line[17] = {0};
    char _words[4], _files[4], _dirs[4];

    Serial.printf("Progress: %d%s\n", progress, "%" );
    UISprite->fillSprite( TFT_ORANGE );
    UIDrawProgressBar( progress, total, 8, 24, "Progress:" );
    UIDrawProgressBar( ramUsage, 100.0, 8, 32, "RAM used:" );
    UISprite->setTextDatum( TL_DATUM );
    UISprite->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    if( words_count > 999 ) {
      snprintf(_words, 4, "%2dK", words_count/1000 );
    } else {
      snprintf(_words, 4, "%3d", words_count );
    }
    if( files_count > 999 ) {
      snprintf(_files, 4, "%2dK", files_count/1000 );
    } else {
      snprintf(_files, 4, "%3d", files_count );
    }
    if( folders_count > 999 ) {
      snprintf(_dirs, 4, "%2dK", folders_count/1000 );
    } else {
      snprintf(_dirs, 4, "%3d", folders_count );
    }
    snprintf(line, 17, " %s  %s  %s", _words, _files, _dirs );

    UISprite->drawString( line, 8, 40 );
    UISprite->drawJpg( iconlist_jpg, iconlist_jpg_len, 8, 40 );
    UISprite->drawJpg( iconfolder2_jpg, iconfolder2_jpg_len, 48, 40 );
    UISprite->drawJpg( iconfolder1_jpg, iconfolder1_jpg_len, 88, 40 );
    UISprite->pushSprite(0, 0, TFT_ORANGE );
  }
}



bool SIDExplorer::diskTicker( const char* title, size_t totalitems )
{
  static bool toggle = false;
  static auto lastTickerMsec = millis();
  static char lineText[32] = {0};

  if( /*diskTickerWordWrap++%10==0 ||*/ millis() - lastTickerMsec > 500 ) { // animate "scanning folder"
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
  vTaskDelay(1); // actually do the tick
  return true;
}



void SIDExplorer::MD5ProgressCb( size_t current, size_t total )
{
  static size_t lastprogress = 0;
  size_t progress = (100*current)/total;
  if( progress != lastprogress ) {
    lastprogress = progress;
    Serial.printf("Progress: %d%s\n", progress, "%" );
    UIPrintProgressBar( progress, total, tft.width()/2, tft.height()/2 );
  }
  vTaskDelay(1);
}



void SIDExplorer::drawCursor( bool state, int32_t cursorX, int32_t cursorY, uint8_t fontWidth, uint8_t fontHeight, uint32_t fgcolor, uint32_t bgcolor )
{
  uint32_t color;
  if( state ) color = fgcolor;
  else color = bgcolor;
  tft.fillRect( cursorX, cursorY, fontWidth, fontHeight, color );
}



void SIDExplorer::blinkCursor( int32_t cursorX, int32_t cursorY, uint8_t fontWidth, uint8_t fontHeight, uint32_t fgcolor, uint32_t bgcolor )
{
  static bool cursorState = true;
  static unsigned long lastBlink = millis();
  if( millis() - lastBlink > 500 ) {
    cursorState = ! cursorState;
    afterHIDRead();
    drawCursor( cursorState, cursorX, cursorY, fontWidth, fontHeight, fgcolor, bgcolor );
    beforeHIDRead();
    lastBlink = millis();
  }
}


