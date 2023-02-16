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

#include "./UI_Utils.hpp"
#include "../helpers/intro_sound.hpp"
#include "../helpers/led_vu_meter.hpp"



namespace SIDView
{

  using namespace UI_Utils;

  SIDTunesPlayer *sidPlayer = nullptr;

  const char* rebuildingPage[] =
  {
    "Rebuilding...",
  };


  SIDExplorer::~SIDExplorer()
  {
    free( oscViews );
    delete SongCache;
    delete sidPlayer;
    delete Nav->items;
    SongCache = NULL;
    sidPlayer = NULL;
    Nav->items = NULL;
  }


  SIDExplorer::SIDExplorer( MD5FileConfig *_cfg ) : cfg(_cfg)
  {

    #ifdef SD_UPDATABLE
      runSDUpdaterMenu();
    #endif

    sidPlayer = new SIDTunesPlayer( cfg->fs );

    Serial.printf("SID Player UI: %d*%d\n", M5.Lcd.width(), M5.Lcd.height() );
    UI_Utils::init(); // init display, sprites, dimensions, colors, calculate items per page

    this->setup(); // init MOS cpu emulator and SID player

    if( psramInit() ) ramSize = ESP.getPsramSize();

    cfg->progressCb = &MD5ProgressCb; // share basic progress handler with HVSC parser

    track     = (SID_Meta_t*)sid_calloc( 1, sizeof( SID_Meta_t ) );
    SongCache = new SongCacheManager( sidPlayer, UI_Utils::itemsPerPage );
    Nav       = new FolderNav( new VirtualFolderNoRam( SongCache ), UI_Utils::itemsPerPage );

    auto deepsid_cfg = deepSID.config();
    deepsid_cfg.fs_hvsc_dir = SID_FOLDER;
    deepsid_cfg.fs          = cfg->fs;

    deepSID.config( deepsid_cfg );

    randomSeed(analogRead(0)); // for random song
    //delay(100);      // let the "other core" fire the SID timer tasks
    //readHID();       // purge HID signal

    loadingScreen.drawStrings( "** Time Sync **", "Checking NTP" );
    loadingScreen.drawModem();

    if( PlayerPrefs.getNetMode() != 0 ) {
      WiFiManagerNS::setup(); // deepsid mode, init WiFiManager
    }

    // load prefs
    PlayerPrefs.getLastPath( Nav->dir );
    // store prefs in session
    songHeader.playerloopmode = PlayerPrefs.getLoopMode();
    maxVolume = PlayerPrefs.getVolume();
    // apply prefs to player
    sidPlayer->setLoopMode( songHeader.playerloopmode );
    setUIPlayerMode( songHeader.playerloopmode );

    if(Nav->dir[0] != '/' || ! cfg->fs->exists( Nav->dir ) ) {
      Nav->setDir( SID_FOLDER );
      //sprintf( Nav->dir, "%s", SID_FOLDER );
    } else {
      fs::File testDir = cfg->fs->open( Nav->dir );
      if( !testDir.isDirectory() ) {
        Nav->setDir( SID_FOLDER );
        //sprintf( Nav->dir, "%s", SID_FOLDER );
      }
      testDir.close();
    }

    bool folderSorted = getPaginatedFolder( 0 );

    // SID folder empty?
    if( !folderSorted ) {
      // TODO: switch to deepsid
      loadingScreen.drawStrings( "** HVSC Sync **", "Checking MD5" );
      loadingScreen.drawDisk();
      checkArchive();
    }

    xTaskCreatePinnedToCore( SIDExplorer::HIDTask, "HIDTask", 2048, this, SID_HIDTASK_PRIORITY, &HIDTaskHandle, SID_HID_CORE );

    timeout_runner_t timeout;
    uint32_t timeout_ms = 1000;
    if( ! timeout.whileMs( []() -> bool { return !HIDReady(); }, timeout_ms ) ) {
      log_e("HID was not ready after %d ms, no controls :-(", timeout_ms );
    }
    xTaskCreatePinnedToCore( SIDExplorer::mainTask, "sidExplorer", 16384, this, sidPlayer->SID_AUDIO_PRIORITY-1, &renderUITaskHandle, SID_MAINTASK_CORE );
  }


  void SIDExplorer::setup()
  {
    log_i("Init sidplayer");
    sidPlayer->sid.setInvertedPins( true ); // use this if pins D{0-7} to 75HC595 are in reverse order
    sidPlayer->setMD5Parser( cfg );
    sidPlayer->sid.setTaskCore( M5.SD_CORE_ID );
    sidPlayer->setEventCallback( eventCallback );
    if( sidPlayer->begin( SID_CLOCK, SID_DATA, SID_LATCH, SID_CLOCK_PIN ) ) {
      soundIntro( &sidPlayer->sid );//
      sidPlayer->setMaxVolume( maxVolume );
      log_i("MOS CPU Core + SID Player Started");

    } else {
      log_e("SID Player failed to start ... halting!");
      // TODO: print error on display
      while(1);
    }
  }


  void SIDExplorer::loop()
  {
    switch (UI_mode) {
      default:
      case UI_mode_t::none:
        //updatePagination(); // handle pagination
        //drawPagedList();    // draw page
        explorer_needs_redraw = true;
        UI_mode = UI_mode_t::paged_listing;
      break;
      case UI_mode_t::paged_listing:
        if( explorer_needs_redraw ) {
          updatePagination(); // handle pagination
          drawPagedList();    // draw page
          explorer_needs_redraw = false;
        } else {
          handleAutoPlay();
        }
        vTaskDelay(1);
      break;
      case UI_mode_t::animated_views:
        drawWaveforms();
      break;
    }
    display->display();
    processHID();
    animateView(); // will set the UI_Mode
  }



  void SIDExplorer::mainTask( void *param )
  {
    vTaskDelay(1); // let the main loop() task self-delete (frees ~4Kb)
    log_d("Running SIDExplorer from core #%d", xPortGetCoreID() );
    SIDExplorer *Explorer = (SIDExplorer*) param;
    for(;;) {
      Explorer->loop();
    }
    vTaskDelete( NULL );
  }


  void SIDExplorer::HIDTask( void * param )
  {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = 150; // check every 150ms
    xLastWakeTime = xTaskGetTickCount();
    SIDExplorer *Explorer = (SIDExplorer*) param;
    initHID(); // start HID
    readHID(); // clear/purge HID queue
    log_d("Running HID Task from core #%d", xPortGetCoreID()  );
    Explorer->inactive_since = millis(); // reset activity timer

    HIDControls HIDAction     = HID_NONE;
    HIDControls LastHIDAction = HID_NONE;

    while(1) {
      HIDAction = readHID();
      if( HIDAction != HID_NONE && ( LastHIDAction != HIDAction || lastpush + debounce < millis() ) ) {
        Explorer->inactive_since = millis(); // reset activity timer
        LastHIDAction = HIDAction;           // memoize last action
        Explorer->HIDAction = HIDAction;     // transmit action to main thread
        while( Explorer->HIDAction != HID_NONE ) vTaskDelay(10); // wait until action is processed
      }
      vTaskDelayUntil( &xLastWakeTime, xFrequency ); // idle until next cycle
    }
    vTaskDelete(NULL); // this will never happen
  }


  void SIDExplorer::handleAutoPlay()
  {
    if( currentBrowsingType == F_PLAYLIST ) {
      switch( nextSIDEvent ) {
        case SID_START_PLAY: nextInPlaylist(); break;
        default: break;
      }
      nextSIDEvent = SID_END_PLAY;
    }
  }


  void SIDExplorer::prevInPlaylist()
  {
    sidPlayer->setLoopMode( SID_LOOP_OFF );
    //render = false;
    vTaskDelay(1);
    if( SongCache->CacheItemNum > 0 && SongCache->getInfoFromItem( Nav->path, SongCache->CacheItemNum-1 ) ) {
      log_d("Playing track #%d from cached playlist file %s (%d elements)", SongCache->CacheItemNum, Nav->path, SongCache->CacheItemSize );
      sidPlayer->playSID( SongCache->currenttrack );
      inactive_delay = 500;
      lastpush = millis();
      explorer_needs_redraw = true;
    } else {
      log_e("Prev cache item (#%d) is not playable", SongCache->CacheItemNum-1);
    }
    vTaskDelay(1);
  }


  void SIDExplorer::nextInPlaylist()
  {
    sidPlayer->setLoopMode( SID_LOOP_OFF );
    //render = false;
    vTaskDelay(1);
    if( SongCache->getInfoFromItem( Nav->path, SongCache->CacheItemNum+1 ) ) {
      log_d("Playing track #%d from cached playlist file %s (%d elements)", SongCache->CacheItemNum, Nav->path, SongCache->CacheItemSize );
      sidPlayer->playSID( SongCache->currenttrack );
      inactive_delay = 500;
      lastpush = millis();
      explorer_needs_redraw = true;
    } else {
      log_e("Next cache item (#%d) is not playable", SongCache->CacheItemNum+1);
    }
    vTaskDelay(1);
  }


  void SIDExplorer::handleHIDDown()
  {
    Nav->next();
    explorer_needs_redraw = true;
    inactive_delay = 5000;
    delay(10);
  }


  void SIDExplorer::handleHIDUp()
  {
    Nav->prev();
    explorer_needs_redraw = true;
    inactive_delay = 5000;
    delay(10);
  }


  void SIDExplorer::handleHIDRight()
  {
    if( sidPlayer->isPlaying() ) {
      switch( currentBrowsingType ) {
        case F_SID_FILE: sidPlayer->playPrevSongInSid(); break;
        case F_PLAYLIST: prevInPlaylist(); break;
        default: /* Should not happen but compiler complains otherwise */ break;
      }
      inactive_delay = 500;
    }
  }


  void SIDExplorer::handleHIDLeft()
  {
    if( sidPlayer->isPlaying() ) {
      switch( currentBrowsingType ) {
        case F_SID_FILE: sidPlayer->playNextSongInSid(); break;
        case F_PLAYLIST:
          sidPlayer->stop();
          nextSIDEvent = SID_START_PLAY; // dispatch to handleAutoPlay
          //stoprender = true;
          vTaskDelay(10);
        break;
        default: /* Should not happen but compiler complains otherwise */ break;
      }
    }
  }


  void SIDExplorer::handleSIDFile()
  {
    if( SongCache->getCacheType() == CACHE_DEEPSID ) {

      // TODO: Nav->realpos() based on list item count (exclude dynamic items)
      auto item = SongCache->fetchDeepSIDItem( Nav->pos - Nav->items->_actionitem_size );

      if( item.type != Deepsid::TYPE_FILE ) {
        log_e("%s is not a file", Nav->path );
        return;
      }

      if( strcmp( item.path().c_str(), Nav->path ) != 0 ) {
        log_e("Names don't match!! item.path()=%s Nav->path=%s", item.path().c_str(), Nav->path );
        return;
      }

      if( ! Deepsid::DirItem::getSIDMeta( item, track ) ) {
        log_e("No Deepsid meta info available for %s", Nav->path );
        return;
      }

      sidPlayer->playSID( track );

      timeout_runner_t timeout;

      if( ! timeout.whileMs( []() -> bool { return !sidPlayer->isPlaying(); }, 1000 ) ) {
        log_e("Song still not playing after 1000ms!");
      }

      inactive_delay = 500;
      inactive_since = millis();

    } else {

      if( sidPlayer->getInfoFromSIDFile( Nav->path, track ) ) {
        sidPlayer->playSID( track );
        inactive_delay = 500;
        inactive_since = millis();
      }
    }
  }


  void SIDExplorer::handlePlaylist()
  {
    sidPlayer->setLoopMode( SID_LOOP_OFF );

    if( SongCache->getCacheType() == CACHE_DOTFILES ) {
      if( SongCache->getInfoFromItem( Nav->path, 0 ) ) {
        sidPlayer->playSID( SongCache->currenttrack ); // play first song in this cache
        inactive_delay = 500;
        inactive_since = millis();
      }
    } else { // deepsid download all
      infoWindow( " SYNCING $ " );
      //if( SongCache->syncDeepSIDFiles( &diskTicker ) > 0 ) {
        int synced = SongCache->syncDeepSIDFiles( &diskTicker );
        if( SongCache->deepsidDir() != nullptr ) {
          Nav->setPath( SongCache->deepsidDir()->sidFolder().c_str() );
          SongCache->setCacheType( CACHE_DOTFILES );
          SongCache->deleteCache( Nav->path );
          PlayerPrefs.setNetMode(0);
        }
      //}
      handleFolder();
    }
  }


  void SIDExplorer::handleFolder()
  {
    Nav->setDir( Nav->path );
    PlayerPrefs.setLastPath( Nav->dir );
    if( Nav->pos==0 && currentBrowsingType == F_PARENT_FOLDER ) {
      // going up
      //if( folderDepth > 0 ) {
        // set cursor on last selected item
        Nav->pos = lastItemCursor[folderDepth-1];
      //} else {
        // set cursor on first item
        //Nav->pos = 0;
      //}
      lastItemCursor[folderDepth] = 0;
    } else {
      // going down
      lastItemCursor[folderDepth] = Nav->pos;
      Nav->pos = 0;
    }
    if( getPaginatedFolder( 0 ) ) {
      Nav->last_page_num = 0;
      //updatePagination();
      explorer_needs_redraw = true;
      inactive_delay = 5000;
    } else {
      log_e("Fatal Error, halting");
      while(1) vTaskDelay(1);
    }
  }


  void SIDExplorer::handleTool()
  {
    log_d("[%d] Invoking tool %s", ESP.getFreeHeap(), Nav->path );
    if       ( strcmp( Nav->path, "reset-path-cache"   ) == 0 ) {
      sidPlayer->kill();
      resetPathCache();
    } else if( strcmp( Nav->path, "reset-spread-cache" ) == 0 ) {
      sidPlayer->kill();
      resetSpreadCache();
    } else if( strcmp( Nav->path, "go-deep"            ) == 0 ) {
      runDeepsidModal();
    } else if( strcmp( Nav->path, "go-hvsc"            ) == 0 ) {
      runHVCSCModal();
    #ifdef SID_DOWNLOAD_ARCHIVES
    } else if( strcmp( Nav->path, "update-md5"         ) == 0 ) {
      sidPlayer->kill();
      updateMd5File();
    #endif
    #if defined ENABLE_HVSC_SEARCH
    } else if( strcmp( Nav->path, "reset-words-cache"  ) == 0 ) {
      sidPlayer->kill();
      resetWordsCache();
    #endif
    }
    inactive_delay = 5000;
  }


  void SIDExplorer::handleHIDPlayStop()
  {
    if( !sidPlayer->isPlaying() /*!adsrenabled*/ ) {

      switch( currentBrowsingType )
      {
        case F_SID_FILE: handleSIDFile();  break;
        case F_PLAYLIST: handlePlaylist(); break;
        case F_TOOL_CB:  handleTool();     break;
        case F_DEEPSID_FOLDER:
        case F_PARENT_FOLDER:
        case F_SUBFOLDER:
        case F_SUBFOLDER_NOCACHE:
          handleFolder();
          break;
        case F_SYMLINK:
        case F_UNSUPPORTED_FILE:
        default:
          break;
      }
    } else {
      log_i("Stopping sidPlayer");
      sidPlayer->stop();
      inactive_delay = 5000;
      vTaskDelay(100);
    }
    explorer_needs_redraw = true;
  }


  void SIDExplorer::handleHIDToggleLoopMode()
  {
    if( UI_mode==UI_mode_t::animated_views ) {
      switch (songHeader.playerloopmode) {
        case SID_LOOP_OFF:    setUIPlayerMode( SID_LOOP_ON );     break;
        case SID_LOOP_ON:     setUIPlayerMode( SID_LOOP_RANDOM ); break;
        case SID_LOOP_RANDOM: setUIPlayerMode( SID_LOOP_OFF );    break;
      };
      PlayerPrefs.setLoopMode( songHeader.playerloopmode ); // save to NVS
      sidPlayer->setLoopMode( songHeader.playerloopmode );
      inactive_delay = 500;
    } else {
      // only when the ".." folder is being selected
      if( currentBrowsingType == F_PARENT_FOLDER ) {
        log_d("Refresh folder -> regenerate %s Dir List and Songs Cache", Nav->dir );
        delete( sidPlayer->MD5Parser );
        sidPlayer->MD5Parser = NULL;
        log_d("#### will getPaginatedFolder"); // such confidence :-)
        getPaginatedFolder( 0, true );
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
  }


  void SIDExplorer::handleHIDVolumeDown()
  {
    if( maxVolume > 0 ) {
      maxVolume--;
      sidPlayer->setMaxVolume( maxVolume ); //value between 0 and 15
      log_d("New volume level: %d", maxVolume );
      PlayerPrefs.setVolume( maxVolume );
    }
  }


  void SIDExplorer::handleHIDVolumeUp()
  {
    if( maxVolume < 15 ) {
      maxVolume++;
      sidPlayer->setMaxVolume( maxVolume ); //value between 0 and 15
      log_d("New volume level: %d", maxVolume );
      PlayerPrefs.setVolume( maxVolume );
    }
  }


  void SIDExplorer::processHID()
  {
    if( HIDAction!=HID_NONE /*&& ( LastHIDAction != HIDAction || lastpush + debounce < millis() )*/ ) {

      UI_mode = UI_mode_t::paged_listing;

      log_d("HID Action #%d = %s", HIDAction, HIDControlStr[HIDAction] );

      auto pagedItem = Nav->getPagedItem(Nav->pos);
      currentBrowsingType = pagedItem.type;
      Nav->setPath( pagedItem.path.c_str() );

      switch(HIDAction) {
        case HID_SLEEP:    UI_Utils::sleep();          break;
        case HID_DOWN:     handleHIDDown();            inactive_delay = 5000; break; // down
        case HID_UP:       handleHIDUp();              inactive_delay = 5000; break; // up
        case HID_RIGHT:    handleHIDRight();           break; // right
        case HID_LEFT:     handleHIDLeft();            break; // left
        case HID_PLAYSTOP: handleHIDPlayStop();        inactive_delay = 100;  break; // B (play/stop button)
        case HID_ACTION1:  handleHIDToggleLoopMode();  break; // circular toggle
        case HID_ACTION2:  handleHIDVolumeDown();      break; // C
        case HID_ACTION3:  handleHIDVolumeUp();        break; // D
        #if defined ENABLE_HVSC_SEARCH
        case HID_SEARCH:   handleHIDSearch();   break;
        #endif
        default: log_w("Unhandled button combination: %X", HIDAction ); inactive_delay = 5000; break; // simultaneous buttons push ?
      }
      vTaskDelay(1);
      lastpush = millis();
      //LastHIDAction = HIDAction;
      HIDAction = HID_NONE; // reset read value, readiness signal for next HID poll
    }
  }


  void SIDExplorer::setUIPlayerMode( loopmode mode )
  {
    switch (mode) {
      case SID_LOOP_OFF:    loopmodeicon = (const char*)single_jpg;     loopmodeicon_len = single_jpg_len;     break;
      case SID_LOOP_ON:     loopmodeicon = (const char*)iconloop_jpg;   loopmodeicon_len = iconloop_jpg_len;   break;
      case SID_LOOP_RANDOM: loopmodeicon = (const char*)iconrandom_jpg; loopmodeicon_len = iconrandom_jpg_len; break;
    };
    songHeader.playerloopmode = mode;
  }


  void SIDExplorer::eventCallback( SIDTunesPlayer* player, sidEvent event )
  {
    lastSIDEvent = event;
    lastEvent = millis();

    switch( event ) {
      case SID_END_FILE: log_n( "[%d] SID_END_FILE: %s", ESP.getFreeHeap(), sidPlayer->getFilename() );
        switch( currentBrowsingType ) {
          case F_PLAYLIST:
            if( SongCache->CacheItemNum < SongCache->CacheItemSize ) {
              nextSIDEvent = SID_START_PLAY;
            } else {
              nextSIDEvent = SID_END_PLAY;
              log_d("End of playlist (playlist size:%d, position: %d", SongCache->CacheItemSize, SongCache->CacheItemNum );
              UI_mode = UI_mode_t::paged_listing;
            }
          break;
          case F_SID_FILE: UI_mode = UI_mode_t::animated_views; break;
          default: UI_mode = UI_mode_t::paged_listing; break;
        }
      break;
      case SID_NEW_TRACK:
        //render = true;
        log_d( "[%d] SID_NEW_TRACK: %s (%02d:%02d) %d/%d subsongs",
          ESP.getFreeHeap(),
          sidPlayer->getName(),
          sidPlayer->getCurrentTrackDuration()/60000, (sidPlayer->getCurrentTrackDuration()/1000)%60,
          sidPlayer->currentsong+1, sidPlayer->subsongs
        );
        //songHeader.setSong();
      break;
      case SID_NEW_FILE: inactive_since = millis(); log_d( "[%d] SID_NEW_FILE: %s (%d songs)", ESP.getFreeHeap(), sidPlayer->getFilename(), sidPlayer->getSongsCount() ); break;
      case SID_START_PLAY:  log_d( "[%d] SID_START_PLAY: %s",  ESP.getFreeHeap(), sidPlayer->getFilename() ); break;
      case SID_END_PLAY:    UI_mode = UI_mode_t::paged_listing; log_d( "[%d] SID_END_PLAY: %s",    ESP.getFreeHeap(), sidPlayer->getFilename() ); break;
      case SID_PAUSE_PLAY:  UI_mode = UI_mode_t::paged_listing; log_d( "[%d] SID_PAUSE_PLAY: %s",  ESP.getFreeHeap(), sidPlayer->getFilename() ); break;
      case SID_RESUME_PLAY: log_d( "[%d] SID_RESUME_PLAY: %s", ESP.getFreeHeap(), sidPlayer->getFilename() );  break;
      case SID_END_TRACK:   log_d( "[%d] SID_END_TRACK",       ESP.getFreeHeap()); break;
      case SID_STOP_TRACK:  UI_mode = UI_mode_t::paged_listing; log_d( "[%d] SID_STOP_TRACK",      ESP.getFreeHeap()); break;
    }
  }


  void SIDExplorer::addFolderElement( const char* fullpath, folderItemType_t type )
  {
    static String basepath = "";
    basepath = gnu_basename( fullpath );
    Nav->pushItem( { basepath, String(fullpath), type } );
    basepath = "";
    diskTicker( "", Nav->size() );
  }


  void SIDExplorer::updatePagination()
  {
    if( Nav->page_changed() ) {
      // infoWindow( " READING $ " );
      // page change, load paginated cache !
      log_d("#### will getPaginatedFolder"); // such confidence :-)
      getPaginatedFolder( Nav->start/*, Nav->itemsPerPage*/  );
      // simulate a push-release to prevent multiple action detection
      lastpush = millis();

      log_d("[%d]$> cur=%d page[%d/%d] range[%d=>%d] fsize=%d path=%s, actionitems=%d, listitems=%d",
        ESP.getFreeHeap(),
        Nav->pos,
        Nav->page_num+1, Nav->pages_count,
        Nav->pageStart(), Nav->pageEnd()-1,
        Nav->size(),
        Nav->getPagedItem(Nav->pos).path.c_str(),
        Nav->items->_actionitem_size, Nav->items->_listitem_size
      );
      explorer_needs_redraw = true;
    }
  }


  size_t SIDExplorer::getFolder( size_t offset, size_t maxitems, bool force_regen )
  {
    const char* folderName = Nav->dir;
    size_t itemsCount = Nav->items->_actionitem_size;//Nav->size() + (Nav->page_num>0?1:0);
    if( force_regen || ! SongCache->folderHasCache( folderName ) ) {
      infoWindow( " CACHING $ " );
      if( !SongCache->scanFolder( folderName, &addFolderElement, &diskTicker, &itemsCount, maxitems, force_regen ) ) {
        Nav->items->clear();
        log_e("Failed to scan folder %s", folderName );
        return 0;
      }
      //Nav->items->_size = SongCache->getDirSize( folderName ) + 1;
    } else {
      infoWindow( " READING $ " );
      if( !SongCache->scanPaginatedFolder( folderName, &addFolderElement, &diskTicker, &itemsCount, offset, maxitems ) ) {
        Nav->items->clear();
        log_e("Failed to scan cached folder %s", folderName );
        return 0;
      }
    }
    log_d("Items count: %d, Nav size: %d", itemsCount, Nav->size() );
    Nav->items->_size = itemsCount;
    return itemsCount;
  }


  bool SIDExplorer::getPaginatedFolder( size_t offset, bool force_regen )
  {
    Nav->clearFolder(); // clear

    if( strcmp( Nav->dir, "/" ) == 0 ) {
      // root dir has no parent folder
      switch( SongCache->getCacheType() ) {
        case CACHE_DEEPSID:  Nav->pushItem( { "> Go Offline <", "go-hvsc", F_TOOL_CB } ); break;
        case CACHE_DOTFILES: Nav->pushItem( { "> Go Online <",  "go-deep", F_TOOL_CB } ); break;
      }
    } else {
      Nav->pushItem( Nav->parentFolder("..") );
      if( SongCache->getCacheType() == CACHE_DEEPSID ) {
        infoWindow( " LURKING $ " );
        auto dir = SongCache->getDeepsidDir(Nav->dir, &diskTicker);
        if( dir != nullptr && dir->files_count() > 0 ) {
          Nav->pushItem({ SongCache->getCacheType() == CACHE_DOTFILES ? "Play all" : "Download all", Nav->dir, F_PLAYLIST });
        }
      } else {
        if( SongCache->folderHasPlaylist( Nav->dir ) ) {
          Nav->pushItem({ SongCache->getCacheType() == CACHE_DOTFILES ? "Play all" : "Download all", Nav->dir, F_PLAYLIST });
        }
      }
    }

    if( strcmp( Nav->dir, cfg->md5folder ) == 0 ) {
      // md5 folder tools
      Nav->pushItem( { "Update MD5",   "update-md5",         F_TOOL_CB } );
      Nav->pushItem( { "Reset pcache", "reset-path-cache",   F_TOOL_CB } );
      Nav->pushItem( { "Reset scache", "reset-spread-cache", F_TOOL_CB } );
      #if defined ENABLE_HVSC_SEARCH
        Nav->pushItem( { "Reset wcache", "reset-words-cache",  F_TOOL_CB } );
      #endif
      return true;
    }

    size_t itemsCount = getFolder( offset, Nav->itemsPerPage, force_regen );

    if( itemsCount == 0 ) {
      return false;
    }
    return true;
  }


  void SIDExplorer::animateView()
  {
    if( millis() - inactive_delay < inactive_since ) { // still debouncing from last action
      listScrollableText->doscroll(); // animate text overflow if any
      UI_mode = UI_mode_t::paged_listing;
      led_meter.off();
    } else {
      if( sidPlayer->isPlaying() ) {
        if( UI_mode!=UI_mode_t::animated_views) {
          // ADSR not running, draw UI and run task
          //log_d("[%d] launching renverVoices task (inactive_delay=%d, inactive_since=%d)", ESP.getFreeHeap(), inactive_delay, inactive_since );
          drawHeader();
          songHeader.setSong();
          UI_mode = UI_mode_t::animated_views;
        }
        songHeader.loop();
      } else {
        UI_mode = UI_mode_t::paged_listing;
        listScrollableText->doscroll(); // animate text overflow if any
        if( millis() > sleep_delay && millis() - sleep_delay > inactive_since ) {
          // go to sleep
          UI_Utils::sleep();
        }
        led_meter.off();
      }
    }
  }


  void SIDExplorer::runHVCSCModal()
  {
    SongCache->setCacheType( CACHE_DOTFILES );
    PlayerPrefs.setNetMode(0);
    Nav->cdRoot();
    handleFolder();
  }


  void SIDExplorer::runDeepsidModal()
  {
    const char* deepSIDPage[] = { "Scraping ...", };
    drawToolPage( "Deepsid scraper", icontool_jpg, icontool_jpg_len, deepSIDPage, 1 );
    SongCache->setCacheType( CACHE_DEEPSID );
    PlayerPrefs.setNetMode(1);
    Nav->cdRoot();
    handleFolder();
  }


  void SIDExplorer::resetPathCache()
  {
    drawToolPage( "Hash cache", icontool_jpg, icontool_jpg_len, rebuildingPage, 1, TL_DATUM );
    cfg->fs->remove( cfg->md5idxpath );
    sidPlayer->MD5Parser->MD5Index->buildSIDPathIndex( cfg->md5filepath, cfg->md5idxpath );
    display->drawString( "Done!", spriteWidth/2, 42 );
    delay(3000);
  }


  void SIDExplorer::resetSpreadCache()
  {
    drawToolPage( "Spread cache", icontool_jpg, icontool_jpg_len, rebuildingPage, 1, TL_DATUM );
    sidPlayer->MD5Parser->MD5Index->buildSIDHashIndex( cfg->md5filepath, cfg->md5folder, true );
    display->drawString( "Done!", spriteWidth/2, 42 );
    delay(3000);
  }


  void SIDExplorer::checkArchive( bool force_check )
  {
    delay(100);
    log_v("#### will check/create md5 file path");
    mkdirp( cfg->fs, cfg->md5filepath ); // mkdirp(): from ESP32-Targz: create traversing directories if none exist
    //log_v("#### will getPaginatedFolder");
    //bool folderSorted = getPaginatedFolder( 0/*, maxitems*/ );

    if( force_check /*|| !folderSorted*/ || !cfg->fs->exists( cfg->md5filepath ) ) {
      #if defined SID_DOWNLOAD_ARCHIVES
        // first run ?
        //TODO: confirmation dialog
        drawToolPage( "Start WiFi?", icontool_jpg, icontool_jpg_len );
        uint8_t hidread = 0;

        while( hidread==0 ) {
          delay(100);
          hidread = readHID();
        }

        if( hidread != HID_PROMPT_N && cfg->archive != nullptr ) {
          //deInitSprites(); // free some ram
          SidArchiveChecker = new SID_Archive_Checker( cfg );
          SidArchiveChecker->checkArchive();

          if( SidArchiveChecker->wifi_occured ) {
            // at this stage the heap is probably too low to do anything else
            ESP.restart();
          }

          //initSprites();
        } else {
          log_e("No archive to check for .. aborting !");
          return;
        }
      #else
        // TODO: infoWindow("You must manually download HVSC Archive and MD5File to the following paths:\nSID_PATH: %s, MD5 FIle Path: %s")
      #endif
    } else {
      //TODO: confirmation dialog
      // give some hints for building the archives ?
    }
  }




  #if defined SID_DOWNLOAD_ARCHIVES

    void SIDExplorer::updateMd5File()
    {
      drawToolPage( "Downloading MD5 File", icontool_jpg, icontool_jpg_len );
      //deInitSprites(); // free some ram
      if( SidArchiveChecker == NULL )
        SidArchiveChecker = new SID_Archive_Checker( cfg );
      SidArchiveChecker->checkMd5File( true );
      display->drawString( "Done!", spriteWidth/2, 42 );
      delay(3000);
      if( SidArchiveChecker->wifi_occured ) {
        // at this stage the heap is probably too low to do anything else
        ESP.restart();
      }
      //initSprites();
    }
  #endif




  #if defined ENABLE_HVSC_SEARCH

    bool SIDExplorer::drawSearchPage( char* searchstr, bool search, size_t itemselected, size_t maxitems )
    {

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
        Nav->pushItem( { String("<- Back"), Nav->path, F_PARENT_FOLDER } );
        log_w("Will search for keyword: %s at offset %d with %d max results", searchstr, 0, maxitems );
        if( SongCache->find( searchstr, &addFolderElement, 0, maxitems ) ) {
          if( Nav->size() > 1 ) {
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
      updatePagination();
      drawPagedList();

      return ret;

      /*
      int HeaderHeight = 16;
      int cx = spriteWidth/2, cy = cx - HeaderHeight;
      //int maxTextWidth = spriteWidth - (2*8); // screen width minus 2 char
      int maxTextLen = 12;

      sptr = (uint16_t*)spritePaginate->createSprite(spriteWidth, display->height()-HeaderHeight );
      if( !sptr ) {
        log_e("Unable to create 16bits sprite for pagination, trying 8 bits");
        spritePaginate->setColorDepth(8);
        sptr = (uint16_t*)spritePaginate->createSprite(spriteWidth, display->height()-HeaderHeight );
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
          drawPagedList();
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



    void SIDExplorer::handleHIDSearch()
    {

      // memoize current position/folder
      Nav->freeze();

      scrollableText->scroll = false;
      explorer_needs_redraw = true;
      //stoprender = true;
      //adsrenabled = false;
      UI_mode = UI_mode_t::none;
      inactive_delay = 5000;
      delay(10);

      bool itemsfound = true;
      char *searchstr = (char*)sid_calloc(255,1);
      uint8_t len = 0;

      Nav->pos = 0;
      Nav->page_num = 0;
      Nav->last_page_num = 0;

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
                    itemsfound = drawSearchPage( searchstr, true, Nav->pos, Nav->itemsPerPage );
                  }
                break;
              }
            break;
            case HID_ACTION4: // BACKSPACE
              if( len > 0 ) {
                len--;
                searchstr[len] = '\0';
                Nav->pos = 0;
                itemsfound = drawSearchPage( searchstr, true, Nav->pos, Nav->itemsPerPage );
              }
            break;
            case HID_ACTION5: // ESCAPE
              itemsfound = false;
              Nav->pos = 0;
              dismissed = true;
            break;
            case HID_DOWN: // arrow_down or pg_down
              Nav->next();
              //if( Nav->pos < Nav->size()-1 ) Nav->pos++;
              //else Nav->pos = 0;
              //itemsfound = drawSearchPage( searchstr, true, Nav->pos, Nav->itemsPerPage );

              //drawPagedList();

            break;
            case HID_UP: // arrow_up or pg_up
              Nav->prev();
              //if( Nav->pos > 0 ) Nav->pos--;
              //else Nav->pos = Nav->size()-1;
              //itemsfound = drawSearchPage( searchstr, true, Nav->pos, Nav->itemsPerPage );

              //drawPagedList();

            break;
            default:
              // blink cursor
              //
              //
            break; // other controls (e.g. escape key?)
          }

          vTaskDelay(1);
        } while( dismissed == false );



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
            itemsfound = drawSearchPage( searchstr, true, Nav->pos, Nav->itemsPerPage );
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
        getPaginatedFolder( Nav->page_num/*, Nav->itemsPerPage*/  );
        explorer_needs_redraw = true;
        //stoprender = true;
        inactive_delay = 5000;

      } else {

        auto pagedItem = Nav->getPagedItem(Nav->pos);
        currentBrowsingType = pagedItem.type;
        Nav->setPath( pagedItem.path.c_str() );

        // #if defined HID_USB
        //   goto HID_PLAYSTOP;
        // #endif
      }

    }


    void SIDExplorer::buildSIDWordsIndexTask( void* param )
    {
      SIDExplorer* Explorer = (SIDExplorer*)param;
      indexTaskRunning = true;
      if( ! SongCache->buildSIDWordsIndex( Explorer->cfg->sidfolder, &keywordsTicker, &keyWordsProgress ) ) {
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


    void SIDExplorer::resetWordsCache()
    {
      drawToolPage( "Sharding words", icontool_jpg, icontool_jpg_len, rebuildingPage, 1, TL_DATUM );
      //int cx = spriteWidth/2, cy = cx;
      //display->drawJpg( insertdisk_jpg, insertdisk_jpg_len, cx - 44, cy - 9 ); // disk icon
      loadingScreen.drawDisk();
      #if defined HAS_TOUCH
        display->touch()->sleep();
      #endif

      //UISprite->setPsram(false);
      UISprite->setColorDepth(8);
      UISprite->fillSprite( TFT_ORANGE ); // transp color
      void *sptr = UISprite->createSprite(spriteWidth, display->height() );
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

      display->drawString( "Done!", spriteWidth/2, 42 );
      delay(3000);
    }

  #endif


}; // end namespace SIDView
