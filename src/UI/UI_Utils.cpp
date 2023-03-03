
#include "UI_Utils.hpp"

#include "../Cache/SIDVirtualFolders.hpp"

namespace SIDView
{
  LGFX *display         = nullptr;
  SIDExplorer* Explorer = nullptr;
  xSemaphoreHandle mux  = NULL; // this is needed to mitigate SPI collisions

};


namespace UI_Dimensions
{
  // some UI dimensions
  const uint16_t HeaderHeight    = 16; // based on header image size
  const uint16_t TouchPadHeight  = 64; // value calibrated for footer in 240x320 portrait mode
  const uint16_t HEADER_HEIGHT   = 41; // arbitrarily chosen height, contains logo + song header + progress bar
  // calculated at boot
  uint16_t VOICE_HEIGHT          = 0;  // ((display->height()-HEADER_HEIGHT)/3); // = 29 on a 128x128
  uint16_t VOICE_WIDTH           = 0;  // display->width();
  uint16_t spriteWidth           = 0;  // display->width();
  uint16_t spriteHeight          = 0;  // display->height()-HeaderHeight;
  uint16_t spriteFooterY         = 0;  // display->height()-fontSize()+1
  uint16_t lineHeight            = 0;  // fontSize()*1.5
  uint16_t minScrollableWidth    = 0;  // display->width()-14
};


namespace UI_Utils
{
  using namespace UI_Dimensions;
  using SIDView::SongCache;
  using SIDView::Nav;
  using SIDView::sidPlayer;
  using SIDView::ScrollableItem;
  using SIDView::display;
  using SIDView::mux;

  LoadingScreen_t loadingScreen;
  SongHeader_t songHeader;
  vumeter_builtin_led_t led_meter;

  bool explorer_needs_redraw = false; // sprite ready to be pushed
  bool songheader_needs_redraw = false;
  bool firstRender          = true;  // for ui initial loading effect
  char lineText[264]        = {0};
  uint8_t* loopmodeicon     = (uint8_t*)iconloop_jpg;
  uint8_t folderDepth       = 0;
  uint16_t itemsPerPage     = 8;
  size_t loopmodeicon_len   = iconloop_jpg_len;
  //uint32_t inactive_since   = 0; // UI debouncer
  UI_mode_t UI_mode         = UI_mode_t::none;

  //OscilloView **oscViews         = nullptr;
  //ScrollableItem *scrollableText = nullptr;
  ScrollableItem *listScrollableText = nullptr;
  ScrollableItem *songScrollableText = nullptr;

  LGFX_Sprite *spriteText     = nullptr; // width*8           1bit    song header text only
  LGFX_Sprite *spriteHeader   = nullptr; // width*16          16bits  header image + merged text
  LGFX_Sprite *spriteFooter   = nullptr; // width*8           8bits   footer text + icons
  LGFX_Sprite *spritePaginate = nullptr; // width*pageHeight  8bits   page listing, text + icons

  #if defined ENABLE_HVSC_SEARCH
    LGFX_Sprite *UISprite       = nullptr; // progress bar ptr, unused but linked to tft
    LGFX_Sprite *pgSprite       = nullptr; // progress bar composite canvas
    LGFX_Sprite *pgSpriteLeft   = nullptr; // progress bar left
    LGFX_Sprite *pgSpriteRight  = nullptr; // progress bar right
  #endif

  void init()
  {
    display = &M5.Lcd;
    mux = xSemaphoreCreateMutex();

    display->setRotation( TFT_ROTATION ); // apply setting from config.h
    //display->setBrightness( 50 );
    display->getPanel()->setSleep(false); // wakeup display

    spriteHeight = display->height()-HeaderHeight; // for bottom-alignment
    spriteWidth  = display->width();

    VOICE_HEIGHT = ( ( min( (uint16_t)display->height(), (uint16_t)160 ) - HEADER_HEIGHT )/3 ); // = 29 on a 128x128
    VOICE_WIDTH  = min( spriteWidth, (uint16_t)256 );

    minScrollableWidth = spriteWidth - 14;

    if( !listScrollableText ) listScrollableText = new ScrollableItem( display );
    if( !songScrollableText ) songScrollableText = new ScrollableItem( display );
    if( !spriteText     ) spriteText     = new LGFX_Sprite( display );
    if( !spriteHeader   ) spriteHeader   = new LGFX_Sprite( display );
    if( !spriteFooter   ) spriteFooter   = new LGFX_Sprite( display );
    if( !spritePaginate ) spritePaginate = new LGFX_Sprite( display );

    #if defined ENABLE_HVSC_SEARCH
      if( !UISprite       ) UISprite      = new LGFX_Sprite( display );
      if( !pgSprite       ) pgSprite      = new LGFX_Sprite( UISprite );
      if( !pgSpriteLeft   ) pgSpriteLeft  = new LGFX_Sprite( pgSprite );
      if( !pgSpriteRight  ) pgSpriteRight = new LGFX_Sprite( pgSprite );
    #endif

    static void *songHeaderCreated = nullptr;
    if( !songHeaderCreated ) {
      spriteText->setPsram(false);
      spriteText->setColorDepth(lgfx::palette_1bit);
      songHeaderCreated = spriteText->createSprite( spriteWidth, 8 );
      if( !songHeaderCreated ) {
        log_e("Unable to create spriteText");
        return;
      }
      spriteText->setFont( &Font8x8C64 );
      spriteText->setTextSize(1);
      spriteText->setPaletteColor(0, C64_DARKBLUE );
      spriteText->setPaletteColor(1, C64_LIGHTBLUE );
      lineHeight = spriteText->fontHeight()*1.5;
      log_i("Line height for lists=%d", lineHeight );
    }

    static void *headerCreated = nullptr;
    if( !headerCreated) {
      spriteHeader->setPsram(false);
      headerCreated = spriteHeader->createSprite( spriteWidth, 16 );
      if( !headerCreated) {
        log_e("Unable to draw header");
        return;
      }
      spriteHeader->setFont( &Font8x8C64 );
      spriteHeader->setTextSize(1);
    }

    static void *paginationCreated = nullptr;
    if( !paginationCreated ) {
      log_v("Will use 8bits sprite for pagination");
      spritePaginate->setPsram(false);
      spritePaginate->setColorDepth(8);
      paginationCreated = spritePaginate->createSprite(spriteWidth, spriteHeight );
      if( !paginationCreated ) {
        log_e("Unable to create 8bits sprite for pagination, giving up");
        return;
      }
      spritePaginate->setFont( &Font8x8C64 );
      spritePaginate->setTextSize(1);
    }


    static void* footerCreated = nullptr;
    if( !footerCreated ) {
      spriteFooter->setPsram(false);
      spriteFooter->setColorDepth(8);
      footerCreated = spriteFooter->createSprite( spriteWidth, 8 );
      if( !footerCreated ) {
        log_e("Unable to create spriteFooter");
        return;
      }
      spriteFooter->setFont( &Font8x8C64 );
      spriteFooter->setTextSize(1);
      spriteFooter->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE ); // not sure about that, maybe spriteFooter->setTextColor( 0, 1 ) instead ?
      spriteFooterY = spriteHeight-(spritePaginate->fontHeight()+1);
    }

    static bool oscViewsCreated = false;
    if( ! oscViewsCreated ) {
      log_d("Init OSC views");
      oscViews = (OscilloView**) sid_calloc( 3, sizeof( OscilloView* ) );
      if( !oscViews ) {
        log_e("Unable to allocate memory for oscViews creation, giving up");
        return;
      }
      oscViewsCreated = true;
      for( int i=0; i<3; i++ ) {
        oscViews[i] = new OscilloView( display, VOICE_WIDTH, VOICE_HEIGHT );
        oscViews[i]->init( &sidPlayer->sid, C64_LIGHTBLUE, C64_DARKBLUE );
      }
    }

    // calculate max "items per page" depending on the context and panel size
    #if defined HID_TOUCH
      float ipp = ( float(display->height()) - float(lineHeight) - TouchPadHeight ) / float(lineHeight);
    #else
      float ipp = ( float(display->height()) - float(lineHeight) - lineHeight*2) / float(lineHeight);
    #endif
    itemsPerPage = floor(ipp);
    log_i("Items per page for lists=%d", itemsPerPage );

    #if defined LED_VU_PIN
      led_meter.pin = LED_BUILTIN;
      led_meter.init();
    #else
      led_meter.pin = -1;
    #endif

    loadingScreen.init( spriteWidth, spriteHeight ); // animate loading screen
  }




  // VU-Meter for single LED (builtin or external) via PWM

  // attach pwm to channel
  void vumeter_builtin_led_t::init()
  {
    if( pin ) {
      ledcSetup(channel, freq_hz, resolution);
      ledcAttachPin(pin, channel);
      off();
    }
  }

  // detach
  void vumeter_builtin_led_t::deinit()
  {
    if( pin ) ledcDetachPin(pin);
  }


  void vumeter_builtin_led_t::setFreq( int _freq_hz )
  {
    if( !pin ) return;
    _freq_hz = std::max( 10, std::min(_freq_hz, 5000) );
    if( _freq_hz != freq_hz ) {
      freq_hz = _freq_hz;
      if( !ledcSetup(channel, freq_hz, resolution) ) {
        log_e("Can't set freq_hz %d for channel %d", freq_hz, channel );
      }
    }
  }

  // positive values between 0 (dark) and 1 (bright)
  void vumeter_builtin_led_t::set( float value, int _freq_hz )
  {
    if( !pin ) return;
    setFreq( _freq_hz );
    value = std::max( value, std::min(value, 1.0f) );
    uint8_t brightness = map( value*100, 0, 100, off_value, on_value );
    ledcWrite(channel, brightness);
  }

  // turn led off
  void vumeter_builtin_led_t::off()
  {
    if( pin ) ledcWrite(channel, off_value);
  }


  // Loading screen, visible when something is loading (filesystem, network)

  void LoadingScreen_t::init( int width, int height )
  {
    setStyles();
    imgposx   = width/2  - imgwidth/2;
    imgposy   = height/2 - imgheight/2;
    titleposx = width/2; // hmiddle (MC_DATUM)
    titleposy = imgposy - (display->fontHeight()+2);
    msgposx   = width/2;
    msgposy   = imgposy + imgheight + display->fontHeight() + 1;
    display->fillScreen( C64_DARKBLUE ); // clear screen, fill with Commodore64 blueish
    display->drawJpg( header128x32_jpg, header128x32_jpg_len, 0, 0 ); // draw header image
    drawC64logo( 52, height-28, 24 ); // draw C64 Logo
    _inited = true; // set init bit
    initSD(); // check sd card
  }

  void LoadingScreen_t::drawModem()
  {
    if( !_inited ) return;
    display->drawJpg( acoustic_coupler_70x26_jpg, acoustic_coupler_70x26_jpg_len, imgposx+9, imgposy );
  }

  void LoadingScreen_t::drawDisk()
  {
    if( !_inited ) return;
    display->drawJpg( insertdisk_jpg, insertdisk_jpg_len, imgposx, imgposy, imgwidth, imgheight );
  }

  void LoadingScreen_t::clearDisk()
  {
    if( !_inited ) return;
    display->fillRect( imgposx, imgposy, imgwidth, imgheight, C64_DARKBLUE ); // clear disk zone
  }

  void LoadingScreen_t::blinkDisk()
  {
    _toggle = !_toggle;
    display->drawFastHLine( imgposx+15, imgposy+22, 4, _toggle ? TFT_RED : TFT_ORANGE );
  }

  void LoadingScreen_t::drawTitle( const char* title )
  {
    if( !_inited ) return;
    setStyles();
    display->drawString( title, titleposx, titleposy );
  }

  void LoadingScreen_t::drawMsg(  const char* msg )
  {
    if( !_inited ) return;
    setStyles();
    display->drawString( msg, msgposx, msgposy );
  }

  void LoadingScreen_t::drawStrings( const char* title, const char* msg )
  {
    if( !_inited ) return;
    if( title ) drawTitle( title );
    if( msg ) drawMsg( msg );
  }

  void LoadingScreen_t::initSD()
  {
    drawDisk();
    auto lastcheck = millis();
    drawTitle( "** Insert SD **" );
    while( !M5.sd_begin() ) {
      blinkDisk();
      delay( _toggle ? 300 : 500 );
      if( lastcheck + 60000 < millis() ) { // No need to hammer the SD Card reader more than a minute
        sleep(); // go to sleep
      }
    }
    clearDisk();
  }

  void LoadingScreen_t::setStyles()
  {
    if( !_inited ) return;
    display->setTextDatum( MC_DATUM );
    display->setFont( &Font8x8C64 );
    display->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
  }




  // Song header, visible when a song is playing

  void SongHeader_t::loop()
  {
    if( ! sidPlayer->isPlaying() ) return;
    uint32_t song_duration = sidPlayer->getCurrentTrackDuration();
    if( song_duration == 0 ) return;

    bool draw_header = false;

    if( songheader_needs_redraw ) {
      setSong();
      draw_header = true;
      songheader_needs_redraw = false;
    }

    // track progress
    currentprogress = (100*sidPlayer->currenttune->delta_song_duration) / song_duration;
    deltaMM = (sidPlayer->currenttune->delta_song_duration/60000)%60;
    deltaSS = (sidPlayer->currenttune->delta_song_duration/1000)%60;

    // 1) time progress changed => drawProgress() / clearProgress()
    // 2) subsong / track changed => setSong(), drawSong()
    // 3) playerloopmode changed => drawSong()
    // 4) volume changed => drawSong()
    // 5) scroll position changed => songScrollableText->doscroll();

    if( deltaMM != lastMM || deltaSS != lastSS ) {
      lastMM = deltaMM;
      lastSS = deltaSS;
      lastprogress = 0; // force progress render
    }

    // song changed, update headers
    if( lastsubsong != sidPlayer->currenttune->currentsong || SongCache->CacheItemNum != lastCacheItemNum ) {
      lastsubsong = sidPlayer->currenttune->currentsong;
      lastCacheItemNum = SongCache->CacheItemNum;
      currentprogress = 0;
      lastMM = 0;
      lastSS = 0;
      draw_header = true;
      vTaskDelay(10);
    }

    if( lastplayerloopmode != playerloopmode ) {
      lastplayerloopmode = playerloopmode;
      draw_header = true;
    }

    if( lastvolume != sidPlayer->getMaxVolume() ) {
      lastvolume = sidPlayer->getMaxVolume();
      draw_header = true;
    }

    /*
    if( sidPlayer->getPositionInPlaylist() != Explorer->positionInPlaylist ) {
      currentprogress = 0;
      lastMM = 0;
      lastSS = 0;
      song_name_needs_scrolling = false; // wait for the next redraw
      draw_header = true;
    }
    */

    if( song_name_needs_scrolling ) {
      songScrollableText->doscroll();
    }

    if( lastprogress != currentprogress ) {
      lastprogress = currentprogress;
      if( currentprogress > 0 ) {
        drawProgress( (sidPlayer->getCurrentTrackDuration()/60000)%60, (sidPlayer->getCurrentTrackDuration()/1000)%60 );
      } else {
        // clear progress zone
        clearProgress();
      }
    }

    if( draw_header ) {
      drawSong();
      draw_header = false;
    }
  }

  void SongHeader_t::setSong()
  {
    lastMM = 0xff;
    lastSS = 0xff;

    snprintf( songNameStr, 255, " %s by %s (c) %s ", sidPlayer->getName(), sidPlayer->getAuthor(), sidPlayer->getPublished() );
    sprintf( filenumStr, " Song:%2d/%-2d  Track:%3d/%-3d ", sidPlayer->getCurrentSong()+1, sidPlayer->getSongsCount(), SongCache->CacheItemNum+1, Nav->size() );

    uint16_t scrollwidth = spriteWidth;
    scrollwidth/=8;
    scrollwidth*=8;
    if( scrollwidth < display->textWidth( songNameStr ) ) {
      songScrollableText->setup( songNameStr, 0, 32, scrollwidth, display->fontHeight(), 30, true );
      song_name_needs_scrolling = true;
    } else {
      song_name_needs_scrolling = false;
    }
  }

  void SongHeader_t::drawSong()
  {
    using namespace SIDView;
    spriteText->fillSprite(C64_DARKBLUE);
    spriteText->setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
    spriteText->setFont( &Font8x8C64 );
    spriteText->setTextSize(1);

    //takeSidMuxSemaphore();
    //display->startWrite();

    display->drawJpg( (uint8_t*)loopmodeicon, loopmodeicon_len, 0, 16 );
    drawVolumeIcon( spriteWidth-9, 16, sidPlayer->getMaxVolume() );

    display->setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
    display->setTextDatum( TL_DATUM );
    display->setFont( &Font8x8C64 );
    display->drawString( filenumStr, 12, 16 );

    if( !song_name_needs_scrolling ) {
      spriteText->setTextDatum( TL_DATUM );
      spriteText->drawString( songNameStr, 16, 0 );
    }
    spriteText->pushSprite(0,32);

    //display->endWrite();
    //giveSidMuxSemaphore();
  }


  void SongHeader_t::drawProgress( uint8_t lenMM, uint8_t lenSS )
  {

    char timeStr[12] = {0};
    uint32_t len_light = (currentprogress*spriteWidth)/100;
    uint32_t len_dark  = spriteWidth-len_light;

    //takeSidMuxSemaphore();
    //display->startWrite();

    // progress text
    display->setTextColor( C64_DARKBLUE, C64_MAROON );
    display->setTextDatum( TL_DATUM );
    display->setFont( &Font8x8C64 );

    sprintf(timeStr, "%02d:%02d", deltaMM, deltaSS ); // remaining
    display->drawString( timeStr, 0, 4 );
    // song length text
    display->setTextDatum( TR_DATUM );
    sprintf(timeStr, "%02d:%02d", lenMM, lenSS ); // total duration
    display->drawString( timeStr, spriteWidth-1, 4 );
    // progressbar
    display->drawFastHLine( 0,         26, len_light, C64_MAROON );
    display->drawFastHLine( 0,         27, len_light, TFT_WHITE );
    display->drawFastHLine( 0,         28, len_light, C64_LIGHTBLUE );
    display->drawFastHLine( 0,         29, len_light, C64_MAROON_DARK );
    display->fillRect( len_light, 26, len_dark, 4, C64_DARKBLUE );

    //display->endWrite();
    //giveSidMuxSemaphore();
  }


  void SongHeader_t::clearProgress()
  {
    //takeSidMuxSemaphore();
    display->fillRect( 0, 25, spriteWidth, 6, C64_DARKBLUE );
    //giveSidMuxSemaphore();
  }


  uint8_t regs[29];
  SID_Registers_t registers = SID_Registers_t(regs);
  //SID_Registers_t* registersPtr = nullptr;


  void drawWaveforms()
  {
    float avglevel[3]; // stores average voice levels for led meter
    float avgfreq = 0; // stores average voice frequencies for led meter

    //sidPlayer->sid.copyRegisters( &registers );
    auto registersPtr = sidPlayer->sid.getRegisters();
    //registers = SID_Registers_t(registersPtr);

    // xSemaphoreTake(sidPlayer->sid.reg_mux, portMAX_DELAY);
    // auto regPtr = sidPlayer->sid.getRegisters();
    // auto registers = SID_Registers_t( regPtr ); // copy 29 bytes
    // xSemaphoreGive(sidPlayer->sid.reg_mux);

    for( int voice=0; voice<3; voice++ ) {
      //oscViews[voice]->setRegisters( &registers );
      oscViews[voice]->setRegisters( (SID_Registers_t*)registersPtr );
      oscViews[voice]->process( voice );
      avglevel[voice] = oscViews[voice]->getAvgLevel();
      avgfreq += oscViews[voice]->getFreq();
      vTaskDelay(1);
    }

    //display->startWrite();
    for( int voice=0; voice<3; voice++ ) {
      //takeSidMuxSemaphore();
      oscViews[voice]->draw( 0, HEADER_HEIGHT + ( voice*VOICE_HEIGHT ) -1 );
      //giveSidMuxSemaphore();
      vTaskDelay(1);
    }
    //display->endWrite();

    if( led_meter.pin  ) {
      // TODO: FFT this
      float avglevels = fabs( avglevel[0] + avglevel[1] + avglevel[2] );
      avgfreq /= 3.0f;
      led_meter.set( avglevels, avgfreq );
    }

  }



  void drawCursor( bool state, int32_t cursorX, int32_t cursorY, uint8_t fontWidth, uint8_t fontHeight, uint32_t fgcolor, uint32_t bgcolor )
  {
    uint32_t color;
    if( state ) color = fgcolor;
    else color = bgcolor;
    display->fillRect( cursorX, cursorY, fontWidth, fontHeight, color );
  }


  void blinkCursor( int32_t cursorX, int32_t cursorY, uint8_t fontWidth, uint8_t fontHeight, uint32_t fgcolor, uint32_t bgcolor )
  {
    static bool cursorState = true;
    static unsigned long lastBlink = millis();
    if( millis() - lastBlink > 500 ) {
      cursorState = ! cursorState;
      drawCursor( cursorState, cursorX, cursorY, fontWidth, fontHeight, fgcolor, bgcolor );
      lastBlink = millis();
    }
  }


  void drawPagedList()
  {
    using namespace SIDView;
    int spriteYpos   = HeaderHeight;
    const uint16_t ydislistpos = lineHeight*1.5;

    spritePaginate->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    spritePaginate->fillSprite( C64_DARKBLUE );
    spritePaginate->setTextWrap( false );
    spritePaginate->setTextDatum( TC_DATUM );
    spritePaginate->setFont( &Font8x8C64 );
    spritePaginate->setTextSize(1);

    spritePaginate->setTextDatum( TL_DATUM );

    folderDepth = Nav->depth();

    if( spritePaginate->textWidth( Nav->dir )+2 > spriteWidth  ) {
      String bname = gnu_basename( Nav->dir );
      sprintf( lineText, ">> %s", bname.c_str() );
    } else {
      sprintf( lineText, "$> %s", Nav->dir );
    }
    spritePaginate->drawString( lineText, 2, 0 );

    for(size_t i=Nav->pageStart(),j=0; i<Nav->pageEnd(); i++,j++ ) {
      uint16_t yitempos = ydislistpos + j*lineHeight;
      auto pagedItem = Nav->getPagedItem(i);
      if( pagedItem == nullFolder ) {
        break;
      }
      switch( pagedItem.type )
      {
        case F_PLAYLIST:
          sprintf( lineText, "%s", SongCache->getCacheType() == CACHE_DOTFILES ? "Play all" : "Download all" );
          spritePaginate->drawJpg( iconlist_jpg, iconlist_jpg_len, 2, yitempos );
          break;
        case F_DEEPSID_FOLDER:
          sprintf( lineText, "%s", pagedItem.name.c_str() );
          spritePaginate->drawJpg( iconfolder1_jpg, iconfolder1_jpg_len, 2, yitempos );
          break;
        case F_SUBFOLDER:
          sprintf( lineText, "%s", pagedItem.name.c_str() );
          spritePaginate->drawJpg( iconfolder2_jpg, iconfolder2_jpg_len, 2, yitempos );
          break;
        case F_SUBFOLDER_NOCACHE:
          sprintf( lineText, "%s", pagedItem.name.c_str() );
          spritePaginate->drawJpg( iconfolder1_jpg, iconfolder1_jpg_len, 2, yitempos );
          break;
        case F_PARENT_FOLDER:
          sprintf( lineText, "%s", pagedItem.name.c_str() );
          spritePaginate->drawJpg( iconup_jpg, iconup_jpg_len, 2, yitempos );
          break;
        case F_TOOL_CB:
          sprintf( lineText, "%s", pagedItem.name.c_str() );
          spritePaginate->drawJpg( icontool_jpg, icontool_jpg_len, 2, yitempos );
          break;
        case F_SYMLINK:
        case F_UNSUPPORTED_FILE:
          sprintf( lineText, "%s", gnu_basename( pagedItem.path.c_str() ).c_str() );
          spritePaginate->drawJpg( dotted_square_8x8_jpg, dotted_square_8x8_jpg_len, 2, yitempos );
          break;
        case F_SID_FILE:
          sprintf( lineText, "%s", gnu_basename( pagedItem.path.c_str() ).c_str() );
          spritePaginate->drawJpg( iconsidfile_jpg, iconsidfile_jpg_len, 2, yitempos );
          break;
      }
      if( i==Nav->pos ) {
        // highlight
        spritePaginate->setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
        // also animate folder name
        if( strcmp( lineText, ".." ) == 0 ) {
          if( spritePaginate->textWidth( Nav->dir ) > minScrollableWidth-10 ) {
            listScrollableText->setup( Nav->dir, 24, spriteYpos, minScrollableWidth-10, lineHeight, 30, false );
          } else {
            listScrollableText->scroll = false;
          }
        } else if( spritePaginate->textWidth( lineText ) > minScrollableWidth ) {
          listScrollableText->setup( lineText, 14, yitempos+spriteYpos, minScrollableWidth, lineHeight, 30 );
        } else {
          listScrollableText->scroll = false;
        }
      } else {
        spritePaginate->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
      }
      spritePaginate->drawString( lineText, 14, yitempos );
    }

    spriteFooter->fillSprite( C64_DARKBLUE );
    sprintf(lineText, "%d/%d", Nav->page_num+1, Nav->pages_count );
    spriteFooter->setTextDatum( TL_DATUM );
    spriteFooter->drawString( lineText, 16, 0/*spriteHeight- (display->fontHeight()+1)*/ /*lineHeight*/ );
    spriteFooter->drawJpg( iconinfo_jpg, iconinfo_jpg_len, 2, 0/*spriteHeight- (display->fontHeight()+1)*/ ); // clear disk icon
    sprintf(lineText, "#%d", Nav->size() );
    spriteFooter->setTextDatum( TR_DATUM );
    spriteFooter->drawString( lineText, spriteFooter->width()-2, 0/*spriteHeight- (display->fontHeight()+1)*/ /*lineHeight*/ );
    spriteFooter->pushSprite( spritePaginate, 0, spriteFooterY, C64_DARKBLUE );

    if( firstRender ) {
      firstRender = false;
      float speed = (float(display->height())/float(spriteWidth))*4.0;
      for( int i=spriteHeight; i>=spriteYpos; i-=speed ) {
        spritePaginate->pushSprite( 0, i );
      }
      drawHeader();
      #if defined HID_TOUCH
        setupUIBtns( spritePaginate, 0, (spritePaginate->height()-(TouchPadHeight+16)), spritePaginate->width(), TouchPadHeight, 0, spriteYpos );
      #endif
    }
    #if defined HID_TOUCH
      drawTouchButtons();
    #endif
    spritePaginate->pushSprite( 0, spriteYpos );
    UI_mode = UI_mode_t::paged_listing; // schedule for page drawing
  }




  void infoWindow( const char* txt )
  {
    drawInfoWindow( txt, spriteHeader->getTextStyle(), &loadingScreen, SongCache->getCacheType() == SIDView::CACHE_DOTFILES );
  }


  void drawInfoWindow( const char* text, const lgfx::TextStyle& style, LoadingScreen_t* loadingScreen, bool is_fs )
  {
    using namespace UI_Dimensions;
    assert( display );
    int cx = spriteWidth/2, cy = cx;
    log_d( "%s", text );
    display->fillRect( 0, HeaderHeight, spriteWidth, spriteHeight, C64_DARKBLUE );
    //display->fillRect( cx-64, cy-38, 128, 92, C64_DARKBLUE );
    display->drawRect( cx-50, cy-28, 100, 56, C64_LIGHTBLUE );
    display->setTextStyle( style );
    display->setTextDatum( MC_DATUM );
    display->setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
    display->setFont( &Font8x8C64 );
    // disk and modem icons are both 88*26
    if( is_fs ) {
      loadingScreen->drawDisk();
    } else {
      loadingScreen->drawModem();
    }
    display->drawString( text, cx, cy-28 );
  }



  void drawHeader()
  {
    spriteHeader->drawFastHLine( 0, 0, spriteWidth, C64_MAROON_DARKER );
    spriteHeader->drawFastHLine( 0, 1, spriteWidth, C64_MAROON_DARK );
    spriteHeader->fillRect( 0, 2, spriteWidth, 12, C64_MAROON );
    spriteHeader->drawJpg( header48x12_jpg, header48x12_jpg_len, 41, 2 );
    spriteHeader->drawFastHLine( 0, 14, spriteWidth, C64_MAROON_DARK );
    spriteHeader->drawFastHLine( 0, 15, spriteWidth, C64_MAROON_DARKER );
    spriteHeader->pushSprite(0, 0);
  }

  void drawToolPage( const char* title, const unsigned char* img, size_t img_len, const char* text[], size_t linescount, uint8_t ttDatum, uint8_t txDatum )
  {
    display->fillScreen( C64_DARKBLUE );
    drawHeader();
    display->setFont( &Font8x8C64 );
    display->setTextDatum( ttDatum );
    display->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    display->drawString( title, ttDatum == TC_DATUM ? spriteWidth/2 : 8, 16 );
    display->drawJpg( img, img_len, 4, 4 );
    display->setTextDatum( txDatum );
    if( linescount>0 && text[0] != nullptr ) {
      log_d("rendering %d lines", linescount);
      for( int i=0;i<linescount;i++) {
        if( text[i] != nullptr && text[i][0] !='\0' ) {
          display->drawString( text[i], 8, 32+(i*8) );
        }
      }
    }
  }


  void drawVolumeIcon( int16_t x, int16_t y, uint8_t volume )
  {
    display->fillRect(x, y, 8, 8, C64_DARKBLUE );
    if( volume == 0 ) {
      // speaker off icon
      display->drawJpg( soundoff_jpg, soundoff_jpg_len, x, y );
    } else {
      // draw volume
      for( uint8_t i=0;i<volume;i+=2 ) {
        display->drawFastVLine( x+i/2, y+7-i/2, i/2, i%4==0 ? C64_LIGHTBLUE : TFT_WHITE );
      }
    }
  }


  void drawC64logo( int32_t posx, int32_t posy, float output_size, uint32_t fgcolor, uint32_t bgcolor )
  {
    //float output_size = size;
    float size = 100.0;
    LGFX_Sprite *sprite = new LGFX_Sprite( display );
    float height = 0.97*size;      // i
    float vmid   = 0.034*size;     // b
    float v1     = 0.166*size;     // c
    float v2     = 0.300*size;     // d
    float h1     = 0.364*size;     // e
    float rv     = 0.520*size/2.0; // f
    float rh     = 0.530*size/2.0; // g
    float h2     = 0.636*size;     // h
    sprite->setColorDepth(1);
    sprite->setPsram( false );
    void *sptr = sprite->createSprite( size, size );
    if( !sptr ) return;
    sprite->setPaletteColor( 0, fgcolor );
    sprite->setPaletteColor( 1, bgcolor );
    sprite->fillSprite( 0 );
    sprite->fillEllipse( size/2, height/2, size/2, height/2, 1 );
    sprite->fillEllipse( size/2, height/2, rh, rv, 0 );
    sprite->fillRect( h2, v2, h1, v1+vmid+v1, 1 ); // prepare
    sprite->fillRect( h2, 0, h1, v2, 0 );    // crop upper white square
    sprite->fillRect( h2, v2+v1, h1, vmid, 0 ); // crop middle stroke
    sprite->fillRect( h2, v2+v1+vmid+v1, h1, v2+2, 0 ); // crop lower white square
    sprite->fillTriangle( size, v2, size, v2+v1+vmid+v1, h2+h1/2, v2+v1+vmid/2, 0 );
    if( size == output_size ) {
      sprite->pushSprite( display, posx, posy/*, 1*/ );
    } else {
      display->setPivot( posx + output_size/2, posy + output_size/2  );
      float zoom = output_size/size;
      sprite->pushRotateZoomWithAA( display, 0.0, zoom, zoom/*, fgcolor*/ );
    }
    sprite->deleteSprite();
  }


  bool diskTicker( const char* title, size_t totalitems )
  {
    static bool toggle = false;
    static auto lastTickerMsec = millis();
    static char lineText[32] = {0};

    if( millis() - lastTickerMsec > 500 ) { // animate "scanning folder"
      display->setTextDatum( TR_DATUM );
      display->setFont( &Font8x8C64 );
      display->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );

      //uint16_t ypos = display->height()-(display->fontHeight()+1); // bottom aligned
      display->fillRect( 0, spriteFooterY, spriteWidth, 8, C64_DARKBLUE ); // clear footer

      // TODO: disk or modem image?
      if( toggle ) {
        display->drawJpg( icondisk_jpg, icondisk_jpg_len, 2, spriteFooterY );
      }
      // blink led on disk image
      display->drawFastHLine( (spriteWidth/2-44)+15, (display->height()/2-13)+22, 4, toggle ? TFT_RED : TFT_ORANGE );

      snprintf(lineText, 12, "%s: #%d", title, totalitems );
      display->drawString( lineText, spriteWidth -2, spriteFooterY );
      toggle = !toggle;
      lastTickerMsec = millis();
    }
    vTaskDelay(1); // actually do the tick
    return true;
  }


  void MD5ProgressCb( size_t current, size_t total )
  {
    static size_t lastprogress = 0;
    size_t progress = (100*current)/total;
    if( progress != lastprogress ) {
      lastprogress = progress;
      Serial.printf("Progress: %d%s\n", progress, "%" );
      UIPrintProgressBar( progress, total, spriteWidth/2, display->height()/2 );
    }
    vTaskDelay(1);
  }


  void sleep()
  {
    //sidPlayer->kill();
    display->setBrightness(0);
    display->getPanel()->setSleep(true);
    // see if display has backlight PWM driver
    auto backlight = (lgfx::Light_PWM*)display->getPanel()->getLight();
    if( backlight != nullptr ) {
      auto cfg = backlight->config();
      int16_t pin_bl = cfg.pin_bl;
      if( pin_bl > -1 ) {
        log_d("Display has backlight PWM driver, signaling off");
        gpio_hold_en((gpio_num_t) pin_bl ); // keep TFT_LED off while in deep sleep
        gpio_deep_sleep_hold_en();
      }
    }
    // see if panel config has backlight pin
    int16_t pin_bl = ( (lgfx::Light_PWM*) display->getPanel()->getLight() )->config().pin_bl;
    if( pin_bl > -1 ) {
      log_d("Display has backlight pin, turning off");
      gpio_hold_en((gpio_num_t) pin_bl ); // keep TFT_LED off while in deep sleep
      gpio_deep_sleep_hold_en();
    }
    // pin interrupt kbd
    // esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, 0); // gpio39 == touch INT
    log_n("Going to sleep, wake me up with the RST button");
    esp_deep_sleep_start();
    // this code is unreachable, right? wrong! it may be reached when the other core has unfinished business (e.g. taskQueue being flushed)
    while(1) vTaskDelay(1); // prevent the function from releasing scope
  }


  void UIPrintProgressBar( float progress, float total, uint16_t xpos, uint16_t ypos, const char *text)
  {
    static int8_t lastprogress = -1;
    if( (uint8_t)progress != lastprogress ) {
      lastprogress = (uint8_t)progress;
      char progressStr[64] = {0};
      snprintf( progressStr, 64, "%s %3d%s", text, int(progress), "%" );
      display->setTextDatum( TC_DATUM );
      display->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
      display->drawString( progressStr, xpos, ypos );
      //display->setCursor( display->height()-(display->fontHeight()+2), 10 );
      //display->printf("    heap: %6d    ", ESP.getFreeHeap() );
    }
  }



  #if defined HID_TOUCH

    TouchButtonWrapper_t tbWrapper;
    int touchSpriteOffset = 0;

    void TouchButtonWrapper_t::handlePressed( TouchButton *btn, bool pressed, int16_t x, int16_t y)
    {
      if (pressed && btn->contains(x, y)) {
        log_v("Press at [%d:%d]", x, y );
        btn->press(true); // tell the button it is pressed
      } else {
        if( pressed ) {
          log_v("Outside Press at [%d:%d]", x, y );
        }
        btn->press(false); // tell the button it is NOT pressed
      }
    }


    void TouchButtonWrapper_t::handleJustPressed( TouchButton *btn, const char* label )
    {
      if ( btn->justPressed() ) {
        btn->drawButton(true, label);
        draw();
        //pushIcon( label );
      }
    }


    bool TouchButtonWrapper_t::justReleased( TouchButton *btn, bool pressed, const char* label )
    {
      bool ret = false;
      if ( btn->justReleased() && (!pressed)) {
        // callable
        ret = true;
      } else if ( btn->justReleased() && (pressed)) {
        // state change but not callable
        ret = false;
      } else {
        // no change, no need to draw
        return false;
      }
      btn->drawButton(false, label);
      draw();
      //pushIcon( label );
      return ret;
    }

    void TouchButtonWrapper_t::draw()
    {
      buttonsSprite->pushSprite( spritePosX, spritePosY );
    }


    void shapeTriangleUpCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label )
    {
      uint_fast8_t r = (w < h ? w : h) >> 2;
      _gfx->fillTriangle( x+w/2, r+y, r+x, y+h-r, x+w-r, y+h-r, invert ? C64_DARKBLUE : C64_LIGHTBLUE );
      if( invert ) {
        _gfx->drawTriangle( x+w/2, r+y, r+x, y+h-r, x+w-r, y+h-r, C64_LIGHTBLUE );
      }
      _gfx->drawRoundRect( x, y, w, h, r, C64_LIGHTBLUE );
    }

    void shapeTriangleDownCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label )
    {
      uint_fast8_t r = (w < h ? w : h) >> 2;
      _gfx->fillTriangle( r+x, r+y, x+w-r, r+y, x+w/2, y+h-r, invert ? C64_DARKBLUE : C64_LIGHTBLUE );
      if( invert ) {
        _gfx->drawTriangle( r+x, r+y, x+w-r, r+y, x+w/2, y+h-r, C64_LIGHTBLUE);
      }
      _gfx->drawRoundRect( x, y, w, h, r, C64_LIGHTBLUE );
    }

    void shapeTrianglePrevCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label )
    {
      uint_fast8_t r = (w < h ? w : h) >> 2;
      _gfx->fillTriangle( x+w-r-1, y+h-r, x+w-r-1, y+r, r*2+x-2, y+h/2, invert ? C64_DARKBLUE : C64_LIGHTBLUE );
      _gfx->fillRect( r+x+1, y+r+1, r-1, h-r*2, invert ? C64_DARKBLUE : C64_LIGHTBLUE );
      if( invert ) {
        _gfx->drawTriangle( x+w-r-1, y+h-r, x+w-r-1, y+r, r*2+x-2, y+h/2, C64_LIGHTBLUE );
        _gfx->drawRect( r+x+1, y+r+1, r-1, h-r*2, C64_LIGHTBLUE );
      }
      _gfx->drawRoundRect( x, y, w, h, r, C64_LIGHTBLUE );
    }

    void shapeTriangleNextCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label )
    {
      uint_fast8_t r = (w < h ? w : h) >> 2;
      _gfx->fillTriangle( x+r, y+r, x+w-r*2, y+h/2, x+r, y+h-r, invert ? C64_DARKBLUE : C64_LIGHTBLUE );

      _gfx->fillRect( x+w-r*2, y+r+1, r-1, h-r*2, invert ? C64_DARKBLUE : C64_LIGHTBLUE );
      if( invert ) {
        _gfx->drawRect( x+w-r*2, y+r+1, r-1, h-r*2, C64_LIGHTBLUE );
        _gfx->drawTriangle( x+r, y+r, x+w-r*2, y+h/2, x+r, y+h-r, C64_LIGHTBLUE );
      }
      _gfx->drawRoundRect( x, y, w, h, r, C64_LIGHTBLUE );
    }

    void shapeTriangleRightCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label )
    {
      uint_fast8_t r = (w < h ? w : h) >> 2;
      _gfx->fillRoundRect( x, y, w, h, r, C64_DARKBLUE );
      if( SIDView::sidPlayer->isPlaying() ) {
        _gfx->fillRoundRect( x+r, y+r, w-r*2, h-r*2, r/2, invert ? C64_DARKBLUE : C64_LIGHTBLUE );
        if( invert )
          _gfx->drawRoundRect( x+r, y+r, w-r*2, h-r*2, r/2, C64_LIGHTBLUE );
      } else {
        _gfx->fillTriangle( x+r, y+r, x+w-r, y+h/2, x+r, y+h-r, invert ? C64_DARKBLUE : C64_LIGHTBLUE );
        if( invert )
          _gfx->drawTriangle( x+r, y+r, x+w-r, y+h/2, x+r, y+h-r, C64_LIGHTBLUE);
      }
      _gfx->drawRoundRect( x, y, w, h, r, C64_LIGHTBLUE );
    }

    void shapeLoopToggleCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label )
    {
      uint_fast8_t r = (w < h ? w : h) >> 2;
      _gfx->fillRoundRect( x, y, w, h, r, C64_DARKBLUE );
      if( invert ) {
        _gfx->drawJpg( iconloop_jpg, iconloop_jpg_len, x+r*1.5, y+r, 0, 0, 0, 0, 1.5F, 1.5F, TL_DATUM );
      } else {
        _gfx->drawJpg( iconloop_jpg, iconloop_jpg_len, 1+x+r, y-1+r, 0, 0, 0, 0, 2.0F, 2.0F, TL_DATUM );
      }
      _gfx->drawRoundRect( x, y, w, h, r, C64_LIGHTBLUE );
    }

    void shapePlusCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label )
    {
      _gfx->fillEllipse( x+w/2, y+h/2, w/2, h/2, invert ? C64_LIGHTBLUE : C64_DARKBLUE );
      _gfx->setTextDatum( MC_DATUM );
      _gfx->setTextSize(2);
      _gfx->setTextColor( invert ? C64_DARKBLUE : C64_LIGHTBLUE );
      _gfx->drawString("+", x-1+w/2, y+1+h/2 );
      _gfx->drawEllipse( x+w/2, y+h/2, w/2, h/2, invert ? C64_DARKBLUE : C64_LIGHTBLUE );
    }

    void shapeMinusCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label )
    {
      _gfx->fillEllipse( x+w/2, y+h/2, w/2, h/2, invert ? C64_LIGHTBLUE : C64_DARKBLUE );
      _gfx->setTextDatum( MC_DATUM );
      _gfx->setTextSize(2);
      _gfx->setTextColor( invert ? C64_DARKBLUE : C64_LIGHTBLUE );
      _gfx->drawString("-", x-1+w/2, y+1+h/2 );
      _gfx->drawEllipse( x+w/2, y+h/2, w/2, h/2, invert ? C64_DARKBLUE : C64_LIGHTBLUE );
    }


    void drawTouchButtons()
    {
      for( int i=0;i<8;i++ ) {
        UIBtns[i].button->drawButton();
      }
    }


    void releaseTouchButtons()
    {
      for( int i=0;i<8;i++ ) {
        UIBtns[i].button->press(false);
      }
    }


    void registerButtonAction( LGFX_Sprite *sprite, TouchButtonAction_t *btna )
    {
      btna->button = new TouchButton();
      btna->button->initButton(
          sprite,
          btna->x, btna->y, btna->w, btna->h,
          C64_DARKBLUE, C64_LIGHTBLUE, C64_DARKBLUE, // outline, fill, text
          (char*)btna->label, btna->font_size
      );
      if( btna->cb != nullptr ) {
        btna->button->setDrawCb( btna->cb );
      }
      btna->button->setLabelDatum(0, 0, MC_DATUM);
      btna->button->press(false);
    }


    void setupUIBtns( LGFX_Sprite *sprite, int x, int y, int w, int h, int xoffset, int yoffset )
    {
      tbWrapper.buttonsSprite = sprite;
      tbWrapper.spritePosX = xoffset;
      tbWrapper.spritePosY = yoffset;

      touchSpriteOffset = yoffset;

      for( int i=0;i<8;i++ ) {
        registerButtonAction( sprite, &UIBtns[i] );
      }
    }

  #endif



  #if defined SID_DOWNLOAD_ARCHIVES

    void UIPrintTitle( const char* title, const char* message )
    {
      display->setTextDatum( MC_DATUM );
      display->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
      int yPos = ( display->height()/2 - display->fontHeight()*4 ) + 2;
      display->fillRect( 0, yPos - display->fontHeight()/2, spriteWidth, display->fontHeight()+1, C64_DARKBLUE );
      display->drawString( title, spriteWidth/2, yPos );
      if( message != nullptr ) {
        yPos = ( display->height()/2 - display->fontHeight()*2 ) + 2;
        display->fillRect( 0, yPos - display->fontHeight()/2, spriteWidth, display->fontHeight()+1, C64_DARKBLUE );
        display->drawString( message, spriteWidth/2, yPos );
      }
    }

    void UIProgressBarPrinter( float progress, float total )
    {
      UIPrintProgressBar( progress, total );
    }

  #endif


  #if defined ENABLE_HVSC_SEARCH

    void initSearchUI()
    {
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
    }


    void deinitSearchUI()
    {
      UISprite->deleteSprite();
    }



    bool keywordsTicker( const char* title, size_t totalitems )
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
            snprintf( line, 16, " %-14s", w[i] );
            UISprite->drawString( line, 0, ypos );
          }
        }
        UISprite->pushSprite(0, 0, TFT_ORANGE );
        lastTickerMsec = millis();
      }
      return true;
    }



    void keyWordsProgress( size_t current, size_t total, size_t words_count, size_t files_count, size_t folders_count  )
    {
      static size_t lastprogress = 0;
      size_t progress = (100*current)/total;
      if( progress != lastprogress ) {
        lastprogress = progress;
        uint32_t ramUsed = SIDView::ramSize - ESP.getFreePsram();
        uint32_t ramUsage = (ramUsed*100) / SIDView::ramSize;
        char line[17] = {0};
        char _words[4], _files[4], _dirs[4];

        Serial.printf("Progress: %d%s\n", progress, "%" );
        UISprite->fillSprite( TFT_ORANGE );
        UIDrawProgressBar( progress, total, 8, 24, "Progress:" );
        UIDrawProgressBar( ramUsage, 100.0, 8, 32, "RAM used:" );
        UISprite->setTextDatum( TL_DATUM );
        UISprite->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
        if( words_count > 999 ) {
          snprintf(_words, 4, "%2dK", (uint16_t)words_count/1000 );
        } else {
          snprintf(_words, 4, "%3d", words_count );
        }
        if( files_count > 999 ) {
          snprintf(_files, 4, "%2dK", (uint16_t)files_count/1000 );
        } else {
          snprintf(_files, 4, "%3d", files_count );
        }
        if( folders_count > 999 ) {
          snprintf(_dirs, 4, "%2dK", (uint16_t)folders_count/1000 );
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



    void UIDrawProgressBar( float progress, float total, uint16_t xpos, uint16_t ypos, const char *text )
    {
      static int8_t lastprogress = -1;
      if( (uint8_t)progress != lastprogress ) {
        lastprogress = (uint8_t)progress;
        char progressStr[64] = {0};
        snprintf( progressStr, 64, "%s %3d%s", text, int(progress), "%" );

        int spritewidth = strlen(progressStr)*8;
        int barwidth = (progress*spritewidth)/100;

        pgSprite->createSprite(spritewidth, 8);
        pgSprite->fillSprite( C64_DARKBLUE );

        pgSprite->setTextDatum( TC_DATUM );
        pgSprite->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );

        int pgCenter = pgSprite->width()/2;

        if( barwidth <= 0 ) {
          pgSprite->drawString( progressStr, pgCenter, 0 );
        } else {

          pgSpriteLeft->createSprite(barwidth, 8);
          pgSpriteRight->createSprite(spritewidth-barwidth, 8);

          pgSpriteLeft->setTextDatum( TC_DATUM );
          pgSpriteLeft->setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
          pgSpriteLeft->setFont( &Font8x8C64 );

          pgSpriteRight->setTextDatum( TC_DATUM );
          pgSpriteRight->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
          pgSpriteRight->setFont( &Font8x8C64 );

          pgSpriteLeft->drawString( progressStr, pgCenter, 0 );
          pgSpriteRight->drawString( progressStr, pgCenter-barwidth, 0 );

          pgSpriteLeft->pushSprite(0, 0);
          pgSpriteRight->pushSprite(barwidth, 0);

          pgSpriteLeft->deleteSprite();
          pgSpriteRight->deleteSprite();

        }
        pgSprite->pushSprite(xpos, ypos);
        pgSprite->deleteSprite();
      }
    }
  #endif


  /*
  struct vec2 {
      float x, y;
      vec2() : x(0), y(0) {}
      vec2(float x, float y) : x(x), y(y) {}
  };

  vec2 operator + (vec2 a, vec2 b) {
      return vec2(a.x + b.x, a.y + b.y);
  }

  vec2 operator - (vec2 a, vec2 b) {
      return vec2(a.x - b.x, a.y - b.y);
  }

  vec2 operator * (float s, vec2 a) {
      return vec2(s * a.x, s * a.y);
  }

  vec2 getBezierPoint( vec2* points, int numPoints, float t ) {
    static vec2 pointCache[129];
    memcpy(pointCache, points, numPoints * sizeof(vec2));
    int i = numPoints - 1;
    while (i > 0) {
        for (int k = 0; k < i; k++)
            pointCache[k] = pointCache[k] + t * ( pointCache[k+1] - pointCache[k] );
        i--;
    }
    vec2 answer = pointCache[0];
    return answer;
  }


  void drawBezierCurve( LGFX_Sprite &scrollSprite, vec2* points, int numPoints, std::uint32_t color ) {
    int lastx = -1, lasty = -1;
    for( float i = 0 ; i < 1 ; i += 0.05 ) {
      vec2 coords = getBezierPoint( points, numPoints, i );
      if( lastx==-1 || lasty==-1 ) {
        scrollSprite.drawPixel( coords.x , coords.y , color );
      } else {
        scrollSprite.drawLine( lastx, lasty, coords.x, coords.y, color );
      }
      lastx = coords.x;
      lasty = coords.y;
    }
  }
  */

  /*
  * non recursive bézier curve with hardcoded 5 points

  int getPt( int n1 , int n2 , float perc ) {
    int diff = n2 - n1;
    return n1 + ( diff * perc );
  }

  void drawBezierCurve( LGFX_Sprite &scrollSprite, int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, int x5, int y5, uint16_t color ) {

    int lastx = -1, lasty = -1;

    for( float i = 0 ; i < 1 ; i += 0.01 ) {
      // The Green Line
      int xa = getPt( x1 , x2 , i );
      int ya = getPt( y1 , y2 , i );
      int xb = getPt( x2 , x3 , i );
      int yb = getPt( y2 , y3 , i );
      int xc = getPt( x3 , x4 , i );
      int yc = getPt( y3 , y4 , i );
      int xd = getPt( x4 , x5 , i );
      int yd = getPt( y4 , y5 , i );

      // The Blue line
      int xm = getPt( xa , xb , i );
      int ym = getPt( ya , yb , i );
      int xn = getPt( xb , xc , i );
      int yn = getPt( yb , yc , i );
      int xo = getPt( xc , xd , i );
      int yo = getPt( yc , yd , i );

      // The Orange line
      int xt = getPt( xm , xn , i );
      int yt = getPt( ym , yn , i );
      int xu = getPt( xn , xo , i );
      int yu = getPt( yn , yo , i );

      // The black dot

      int x = getPt( xt , xu , i );
      int y = getPt( yt , yu , i );

      if( lastx==-1 || lasty==-1 ) {
        scrollSprite.drawPixel( x , y , color );
      } else {
        scrollSprite.drawLine( lastx, lasty, x, y, color );
      }
      lastx = x;
      lasty = y;

    }

  }*/

}; // end namespace
