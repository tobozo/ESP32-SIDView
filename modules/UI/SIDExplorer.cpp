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
  free( folderToScan );
  free( lineText     );
  free( oscViews     );
  free( currentPath  );
  deInitSprites();
  //sprite->deleteSprite();
  delete SongCache;
  delete sidPlayer;
  delete myVirtualFolder;
  SongCache = NULL;
  sidPlayer = NULL;
  myVirtualFolder = NULL;
}


SIDExplorer::SIDExplorer( MD5FileConfig *_cfg ) : cfg(_cfg)
{
  fs              = cfg->fs;
  cfg->progressCb = &MD5ProgressCb;

  folderToScan    = (char*)         sid_calloc( 256, sizeof(char) );
  lineText        = (char*)         sid_calloc( 256, sizeof(char) );
  currentPath     = (char*)         sid_calloc( 256, sizeof(char) );
  oscViews        = (OscilloView**) sid_calloc( 3, sizeof( OscilloView* ) );
  song            = (SID_Meta_t*)   sid_calloc( 1, sizeof( SID_Meta_t ) );


  spriteWidth     = tft.width();
  spriteHeight    = tft.height();
  spriteText      = new TFT_eSprite( &tft );

  for( int i=0; i<3; i++ ) {
    oscViews[i]   = new OscilloView( VOICE_WIDTH, VOICE_HEIGHT );
  }

  sidPlayer       = new SIDTunesPlayer( cfg->fs );
  SongCache       = new SongCacheManager( sidPlayer );
  myVirtualFolder = new VirtualFolderPsRam( SongCache );

  sidPlayer->setMD5Parser( cfg );

  spriteText->setPsram(false);
  spriteText->setFont( &Font8x8C64 );
  spriteText->setTextSize(1);

  lineHeight         = spriteText->fontHeight()+2;
  minScrollableWidth = spriteWidth - 14;
}


int32_t SIDExplorer::explore()
{
  randomSeed(analogRead(0)); // for random song

  loadingScreen();

  sidPlayer->setEventCallback( eventCallback );
  if( sidPlayer->begin( SID_CLOCK, SID_DATA, SID_LATCH, SID_CLOCK_PIN ) ) {
    log_w("SID Player Begin OK");
  } else {
    log_e("SID Player failed to start ... duh !");
    while(1);
  }

  playerloopmode = PlayerPrefs.getLoopMode();
  sidPlayer->setLoopMode( playerloopmode );
  setUIPlayerMode( playerloopmode );

  maxVolume = PlayerPrefs.getVolume();
  sidPlayer->setMaxVolume( maxVolume );

  PlayerPrefs.getLastPath( folderToScan );

  if(! fs->exists( folderToScan ) ) {
    sprintf( folderToScan, "%s", "/" );
  }

  if( myVirtualFolder->size() == 0 ) {
    checkArchive();
  }

  while( !exitloop ) {

    log_d("[%d] Setting up pagination", ESP.getFreeHeap() );
    setupPagination(); // handle pagination
    log_d("[%d] Drawing paginated", ESP.getFreeHeap() );
    drawPaginaged();   // draw page

    explorer_needs_redraw = false;
    adsrenabled = false;
    inactive_since = millis();
    inactive_delay = 5000;

    log_d("[%d] entering loop", ESP.getFreeHeap() );

    while( !explorer_needs_redraw ) { // wait for button activity
      handleAutoPlay();
      animateView();
      processHID();
    } // end while( !explorer_needs_redraw )

  } // end while( !exitloop )
  return itemClicked;
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
  if( SongCache->CacheItemNum > 0 && SongCache->getInfoFromCacheItem( currentPath, SongCache->CacheItemNum-1 ) ) {
    log_d("Playing track #%d from cached playlist file %s (%d elements)", SongCache->CacheItemNum, currentPath, SongCache->CacheItemSize );
    sidPlayer->playSID();
  } else {
    log_e("Next cache item is not playable");
  }
}



void SIDExplorer::nextInPlaylist()
{
  sidPlayer->setLoopMode( SID_LOOP_OFF );
  if( SongCache->getInfoFromCacheItem( currentPath, SongCache->CacheItemNum+1 ) ) {
    log_d("Playing track #%d from cached playlist file %s (%d elements)", SongCache->CacheItemNum, currentPath, SongCache->CacheItemSize );
    sidPlayer->playSID();
  } else {
    log_e("Next cache item is not playable");
  }
}



int SIDExplorer::readHID()
{
  if( !wire_begun ) {
    Wire.begin( SDA, SCL ); // connect XPadShield
    wire_begun = true;
  }
  Wire.requestFrom(0x10, 1); // scan buttons from XPadShield
  return Wire.read();
}


void SIDExplorer::processHID()
{
  int resp = readHID();
  if( resp!=0 && ( lastresp != resp || lastpush + debounce < millis() ) ) {
    log_d("HID Action = %d", resp );

    currentBrowsingType = myVirtualFolder->get(itemCursor%itemsPerPage).type;
    snprintf( currentPath, 255, "%s", myVirtualFolder->get(itemCursor%itemsPerPage).path.c_str() );

    switch(resp) {
      case 0x01: // down
        if( itemCursor < myVirtualFolder->size()-1 ) itemCursor++;
        else itemCursor = 0;
        explorer_needs_redraw = true;
        stoprender = true;
        delay(10);
      break;
      case 0x02: // up
        if( itemCursor > 0 ) itemCursor--;
        else itemCursor = myVirtualFolder->size()-1;
        explorer_needs_redraw = true;
        stoprender = true;
        delay(10);
      break;
      case 0x04: // right
        if( sidPlayer->isPlaying() ) {
          switch( currentBrowsingType ) {
            case F_SID_FILE: sidPlayer->playNextSongInSid(); break;
            case F_PLAYLIST: nextInPlaylist(); break;
            default: /* SHOULD NOT BE THERE */; break;
          }
        }
      break;
      case 0x08: // left
        if( sidPlayer->isPlaying() ) {
          switch( currentBrowsingType ) {
            case F_SID_FILE: sidPlayer->playPrevSongInSid(); break;
            case F_PLAYLIST: prevInPlaylist(); break;
            default: /* SHOULD NOT BE THERE */; break;
          }
        }
      break;
      break;
      case 0x10: // B (play/stop button)
        if( !adsrenabled ) {

          switch( currentBrowsingType )
          {
            case F_SID_FILE:
              sidPlayer->setLoopMode( SID_LOOP_ON );
              if( sidPlayer->getInfoFromSIDFile( currentPath ) ) {
                sidPlayer->playSID();
              }
              inactive_since = millis() - inactive_delay;
            break;
            case F_PLAYLIST:
              sidPlayer->setLoopMode( SID_LOOP_OFF );
              if( SongCache->getInfoFromCacheItem( currentPath, 0 ) ) {
                // playing first song in this cache
                sidPlayer->playSID();
              }
              inactive_since = millis() - inactive_delay;
            break;
            case F_PARENT_FOLDER:
            case F_SUBFOLDER:
            case F_SUBFOLDER_NOCACHE:
              snprintf(folderToScan, 256, "%s", currentPath );
              PlayerPrefs.setLastPath( folderToScan );
              getSortedFolder( folderToScan, maxitems );
              if( itemCursor==0 && currentBrowsingType == F_PARENT_FOLDER ) {
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
              explorer_needs_redraw = true;
            break;
            case F_TOOL_CB:
              sidPlayer->kill();
              log_d("[%d] Invoking tool %s", ESP.getFreeHeap(), currentPath );
              if       ( strcmp( currentPath, "update-md5"         ) == 0 ) {
                updateMd5File();
              } else if( strcmp( currentPath, "reset-path-cache"   ) == 0 ) {
                resetPathCache();
              } else if( strcmp( currentPath, "reset-spread-cache" ) == 0 ) {
                resetSpreadCache();

              }
            break;
          }
        } else {
          sidPlayer->stop();
          vTaskDelay(100);
        }
        explorer_needs_redraw = true;
      break;
      case 0x20: // Action button
        // circular toggle
        if( adsrenabled ) {
          switch (playerloopmode) {
            case SID_LOOP_OFF:    setUIPlayerMode( SID_LOOP_ON );     break;
            case SID_LOOP_ON:     setUIPlayerMode( SID_LOOP_RANDOM ); break;
            case SID_LOOP_RANDOM: setUIPlayerMode( SID_LOOP_OFF );    break;
          };
          PlayerPrefs.setLoopMode( playerloopmode ); // save to NVS
          sidPlayer->setLoopMode( playerloopmode );
        } else {
          // only when the ".." folder is being selected
          if( currentBrowsingType == F_PARENT_FOLDER ) {
            log_d("Refresh folder -> regenerate %s Dir List and Songs Cache", folderToScan );
            delete( sidPlayer->MD5Parser );
            sidPlayer->MD5Parser = NULL;
            getSortedFolder( folderToScan, maxitems, true );
            sidPlayer->setMD5Parser( cfg );
            explorer_needs_redraw = true;
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
          sidPlayer->setMaxVolume( maxVolume ); //value between 0 and 15
          log_d("New volume level: %d", maxVolume );
          PlayerPrefs.setVolume( maxVolume );
        }
      break;
      case 0x80: // D
        if( maxVolume < 15 ) {
          maxVolume++;
          sidPlayer->setMaxVolume( maxVolume ); //value between 0 and 15
          log_d("New volume level: %d", maxVolume );
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
  uint16_t oldPageNum = pageNum;
  pageNum     = itemCursor/itemsPerPage;
  if( myVirtualFolder->size() > 0 ) {
    pageTotal   = ceil( float(myVirtualFolder->size())/float(itemsPerPage) );
  } else {
    pageTotal   = 1;
  }
  pageStart   = pageNum*itemsPerPage;
  pageEnd     = pageStart+itemsPerPage;

  if( pageNum != oldPageNum ) {
    infoWindow( " READING $ " );
    // page change, load paginated cache !
    getPaginatedFolder( folderToScan, pageStart, itemsPerPage  );
    // simulate a push-release to prevent multiple action detection
    lastpush = millis();
  }

  ydirnamepos = lineHeight*1.5 + 2;
  ydislistpos = ydirnamepos + lineHeight*1.5;

  int ispaginated = (pageStart >= itemsPerPage || pageEnd-pageStart < itemsPerPage) ? 1 : 0;

  // cap last page
  if( pageEnd > myVirtualFolder->size()+1 ) {
    pageEnd = myVirtualFolder->size()+ispaginated;
  }

  log_d("[%d]$> %s -> c:%d, [%d/%d], page:[%d=>%d], fsize: %d", ESP.getFreeHeap(), folderToScan, itemCursor, pageNum+1, pageTotal, pageStart, pageEnd,  myVirtualFolder->size() );
}


void SIDExplorer::drawPaginaged()
{

  tft.startWrite();

  tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
  tft.fillScreen( C64_DARKBLUE );
  tft.setTextWrap( false );
  tft.setTextDatum( TC_DATUM );
  tft.setFont( &Font8x8C64 );

  //tft.drawString( "-=SID Explorer=-", spriteWidth /2, 2 );
  drawHeader();

  tft.setTextDatum( TL_DATUM );

  folderDepth = 0;
  for( int i=0; i<strlen(folderToScan); i++ ) {
    if( folderToScan[i] == '/' ) folderDepth++;
  }

  if( tft.textWidth( folderToScan )+2 > spriteWidth  ) {
    String bname = gnu_basename( folderToScan );
    snprintf( lineText, 256, ">> %s", bname.c_str() );
  } else {
    snprintf( lineText, 256, "$> %s", folderToScan );
  }
  tft.drawString( lineText, 2, ydirnamepos );

  tft.setSwapBytes(false);

  for(size_t i=pageStart,j=0; i<pageEnd; i++,j++ ) {
    yitempos = ydislistpos + j*lineHeight;
    switch( myVirtualFolder->get(i%itemsPerPage).type )
    {
      case F_PLAYLIST:
        sprintf( lineText, "%s", "Play all"/*myVirtualFolder->get(i).name.c_str()*/ );
        tft.drawJpg( iconlist_jpg, iconlist_jpg_len, 2, yitempos );
        break;
      case F_SUBFOLDER:
        sprintf( lineText, "%s", myVirtualFolder->get(i%itemsPerPage).name.c_str() );
        tft.drawJpg( iconfolder2_jpg, iconfolder2_jpg_len, 2, yitempos );
        break;
      case F_SUBFOLDER_NOCACHE:
        sprintf( lineText, "%s", myVirtualFolder->get(i%itemsPerPage).name.c_str() );
        tft.drawJpg( iconfolder1_jpg, iconfolder1_jpg_len, 2, yitempos );
        break;
      case F_PARENT_FOLDER:
        sprintf( lineText, "%s", myVirtualFolder->get(i%itemsPerPage).name.c_str() );
        tft.drawJpg( iconup_jpg, iconup_jpg_len, 2, yitempos );
        break;
      case F_TOOL_CB:
        sprintf( lineText, "%s", myVirtualFolder->get(i%itemsPerPage).name.c_str() );
        tft.drawJpg( icontool_jpg, icontool_jpg_len, 2, yitempos );
        break;
      case F_SID_FILE:
        String basename = gnu_basename( myVirtualFolder->get(i%itemsPerPage).path.c_str() );
        sprintf( lineText, "%s", basename.c_str() );
        tft.drawJpg( iconsidfile_jpg, iconsidfile_jpg_len, 2, yitempos );
        break;
    }
    if( i==itemCursor ) {
      // highlight
      tft.setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
      // also animate folder name
      if( strcmp( lineText, ".." ) == 0 ) {
          if( tft.textWidth( folderToScan ) > minScrollableWidth-10 ) {
            scrollableText->setup( folderToScan, 24, ydirnamepos, minScrollableWidth-10, lineHeight, 300, false );
          } else {
            scrollableText->scroll = false;
          }
      } else if( tft.textWidth( lineText ) > minScrollableWidth ) {
        scrollableText->setup( lineText, 14, yitempos, minScrollableWidth, lineHeight );
      } else {
        scrollableText->scroll = false;
      }
    } else {
      tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    }
    tft.drawString( lineText, 14, yitempos );
  }

  tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
  sprintf(lineText, "%d/%d", pageNum+1, pageTotal );
  tft.setTextDatum( TL_DATUM );
  tft.drawString( lineText, 16, spriteHeight-lineHeight );
  tft.drawJpg( iconinfo_jpg, iconinfo_jpg_len, 2, spriteHeight-lineHeight ); // clear disk icon
  sprintf(lineText, "#%d", myVirtualFolder->size() );
  tft.setTextDatum( TR_DATUM );
  tft.drawString( lineText, spriteWidth -2, spriteHeight-lineHeight );

  if( firstRender ) {
    firstRender = false;
    /*for( int i=spriteHeight; i>=0; i-- ) {
      tft.pushSprite( 0, i );
    }*/
    soundIntro( &sidPlayer->sid );
  } else {
    //tft.pushSprite( 0, 0 );
  }

  //sprite->deleteSprite();
  tft.endWrite();
}


bool SIDExplorer::getPaginatedFolder( const char* folderName, size_t offset, size_t maxitems, bool force_regen )
{
  myVirtualFolder->clear();
  myVirtualFolder->folderName = folderName;

  if( String( folderName ) != "/"  ) { // no parent link for root folder
    if( offset == 0 ) {
      String dirname = gnu_basename( folderName );
      String parentFolder = String( folderName ).substring( 0, strlen( folderName ) - (dirname.length()+1) );
      if( parentFolder == "" ) parentFolder = "/";
      myVirtualFolder->push_back( { String(".."), parentFolder, F_PARENT_FOLDER } );
      maxitems--;
    }
  }

  diskTickerWordWrap = 0;
  diskTickerMsec = millis();

  if( strcmp( folderName, cfg->md5folder ) == 0 ) {
    // md5 folder tools
    myVirtualFolder->push_back( { "Update MD5",   "update-md5",         F_TOOL_CB } );
    myVirtualFolder->push_back( { "Reset pcache", "reset-path-cache",   F_TOOL_CB } );
    myVirtualFolder->push_back( { "Reset scache", "reset-spread-cache", F_TOOL_CB } );
    return true;
  }

  if( force_regen || ! SongCache->folderHasCache( folderName ) ) {
    infoWindow( " WRITING $ " );
    if( !SongCache->scanFolder( folderName, &addFolderElement, &diskTicker, itemsPerPage, force_regen ) ) {
      myVirtualFolder->clear();
      log_e("Could not build a list of songs");
      return false;
    }/* else {
      if( ! SongCache->scanPaginatedFolderCache( folderName, offset, itemsPerPage, &addFolderElement, &diskTicker ) ) {
        log_e("Could paginate cached folder");
        return false;
      }
    }*/
  } else {
    infoWindow( " READING $ " );
    if( !SongCache->scanPaginatedFolder( folderName, &addFolderElement, &diskTicker, offset, itemsPerPage ) ) {
      myVirtualFolder->clear();
      log_e("Could not build a list of songs");
      return false;
    }
  }

  size_t oldsize = 0;
  oldsize = myVirtualFolder->_size;
  if( String( folderName ) != "/"  ) { // no parent link for root folder
    myVirtualFolder->_size = SongCache->getDirCacheSize( folderName ) + (offset == 0 ? 1 : 0 );
  } else {
    myVirtualFolder->_size = SongCache->getDirCacheSize( folderName );
  }
  log_d("Adjusted folder size from %d to %d (offset=%d, maxitems=%d)", oldsize, myVirtualFolder->_size, offset, maxitems );

  return true;
}


bool SIDExplorer::getSortedFolder( const char* folderName, size_t maxitems, bool force_regen )
{
  bool ret = getPaginatedFolder( folderName, 0, maxitems, force_regen );
  return ret;
}


void SIDExplorer::drawToolPage( const char* title )
{
  tft.fillScreen( C64_DARKBLUE );
  drawHeader();
  tft.setTextDatum( MC_DATUM );
  tft.drawString( title, tft.width()/2, 32 );
  tft.setTextDatum( TL_DATUM );
  tft.drawJpg( icontool_jpg, icontool_jpg_len, 4, 4 );
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

  mkdirp( fs, cfg->md5filepath ); // from ESP32-Targz: create traversing directories if none exist
  bool folderSorted = getSortedFolder( folderToScan, maxitems );

  if( force_check || !folderSorted || !fs->exists( cfg->md5filepath ) ) {
    // first run ?
    //TODO: confirmation dialog
    drawToolPage( "Download Archive?");
    while( readHID()==0 ) {
      delay(100);
    }

    if( cfg->archive != nullptr ) {
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
  spriteText->createSprite( tft.width(), 8 );
  spriteText->setPaletteColor(0, C64_DARKBLUE );
  spriteText->setPaletteColor(1, C64_LIGHTBLUE );

  for( int i=0;i<3;i++ ) {
    oscViews[i]->init( &sidPlayer->sid );
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
  /*
  for( int i=2;i<48;i+=2 ) {
    int i2 = i+1;
    tft.drawRect( cx-i2, cy-i2/2, i2*2, i2, C64_LIGHTBLUE );
    tft.drawRect( cx-i,  cy-i/2,  i*2,  i,  C64_DARKBLUE );
  }*/

  tft.fillRect( cx-64, cy-36, 128, 82, C64_DARKBLUE );
  tft.drawRect( cx-50, cy-28, 100, 56, C64_LIGHTBLUE );

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

  while( !SID_FS.begin() )
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

  if( millis() - inactive_delay < inactive_since ) {
    scrollableText->doscroll(); // animate text overflow if any
    stoprender = true; // send stop signal to task
    delay(10);
    renderVoicesTaskHandle = NULL;
  } else {
    if( sidPlayer->isRunning() ) {
      if( renderVoicesTaskHandle == NULL  && ! adsrenabled ) {
        // ADSR not running, draw UI and run task
        log_d("[%d] launching renverVoices task", ESP.getFreeHeap() );
        drawHeader();
        positionInPlaylist = -1; // make sure the title is displayed
        xTaskCreatePinnedToCore( drawVoices, "drawVoices", 2048, this, 0, &renderVoicesTaskHandle, /*SID_PLAYER_CORE/*/SID_CPU_CORE ); // will trigger TFT writes
        adsrenabled = true;
      } else {
        // ADSR already running or stopping
      }
    } else {
      scrollableText->doscroll(); // animate text overflow if any
      stoprender = true; // send stop signal to task
      delay(10);
      renderVoicesTaskHandle = NULL;
    }
  }
}


void SIDExplorer::drawHeader()
{
  //tft.fillRect( 0, 0, tft.width(), 16, C64_DARKBLUE ); // Commodore64 blueish
  tft.drawFastHLine( 0, 0, tft.width(), C64_MAROON_DARKER );
  tft.drawFastHLine( 0, 1, tft.width(), C64_MAROON_DARK );
  tft.fillRect( 0, 2, tft.width(), 12, C64_MAROON );
  tft.drawJpg( header48x12_jpg, header48x12_jpg_len, 41, 2 );
  tft.drawFastHLine( 0, 14, tft.width(), C64_MAROON_DARK );
  tft.drawFastHLine( 0, 15, tft.width(), C64_MAROON_DARKER );
}



