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

#ifndef _SID_VIEWER_H_
#define _SID_VIEWER_H_

#include "UI_Utils.h"
#include "SIDExplorer.h"

#include "../Oscillo/Oscillator.h"

OscilloView oscViews[3];

static void initSprites()
{
  spriteText.setPsram(false);
  spriteText.setColorDepth(lgfx::palette_1bit);
  spriteText.createSprite( tft.width(), 8 );
  spriteText.setPaletteColor(0, C64_DARKBLUE );
  spriteText.setPaletteColor(1, C64_LIGHTBLUE );

  for( int i=0;i<3;i++ ) {
    oscViews[i].init( VOICE_WIDTH, VOICE_HEIGHT );
  }
}


static void deInitSprites()
{
  spriteText.deleteSprite();
  for( int i=0;i<3;i++ ) {
    oscViews[i].deinit();
  }
}

void drawVolumeIcon( int16_t x, int16_t y, uint8_t volume )
{
  tft.fillRect(x, y, 8, 8, C64_DARKBLUE );
  if( volume == 0 ) {
    // speaker off icon
    tft.drawJpg( soundoff_jpg, soundoff_jpg_len, x, y );
  } else {
    // draw volume
    for( uint8_t i=0;i<volume;i+=2 ) {
      tft.drawFastVLine( x+i/2, y+7-i/2, i/2, i%4==0 ? C64_LIGHTBLUE : TFT_WHITE );
      //tft.drawLine( x+i/2, y+7, x+i/2, y+7-i/2, i%4==0 ? C64_LIGHTBLUE : TFT_WHITE );
    }
  }
}


static void drawSongHeader( SIDExplorer* mySIDExplorer, char* songNameStr, char* filenumStr, bool &scrolltext )
{

  positionInPlaylist = sidPlayer->getPositionInPlaylist();
  spriteText.fillSprite(C64_DARKBLUE);
  spriteText.setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );

  snprintf( songNameStr, 255, " %s by %s (c) %s ", sidPlayer->getName(), sidPlayer->getAuthor(), sidPlayer->getPublished() );
  sprintf( filenumStr, " S:%-2d - T:%-2d ", sidPlayer->currentsong+1, positionInPlaylist );

  //textPosX = 16;
  //textPosY = 0;

  tft.startWrite();

  tft.drawJpg( (const uint8_t*)mySIDExplorer->loopmodeicon, mySIDExplorer->loopmodeicon_len, 0, 16 );
  drawVolumeIcon( tft.width()-8, 16, maxVolume );
  uint16_t scrollwidth = tft.width();
  scrollwidth/=8;
  scrollwidth*=8;
  if( scrollwidth < tft.textWidth( songNameStr ) ) {
    //Serial.println("Reset scroll");
    if( !scrolltext ) {
      scrollableText.setup( songNameStr, 0, 32, scrollwidth, tft.fontHeight(), 300, true );
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
    spriteText.setTextDatum( TL_DATUM );
    spriteText.drawString( songNameStr, 16, 0 );
  }
  spriteText.pushSprite(0,32);
  //Serial.printf("New track : %s\n", songNameStr );
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


static void drawVoices( void* param = NULL )
{

  SIDExplorer *mySIDExplorer = nullptr;
  loopmode playerloopmode = MODE_SINGLE_TRACK;

  char    filenumStr[32]   = {0};
  char    songNameStr[255] = {0};
  int32_t lastprogress     = 0;
  int16_t textPosX         = 0,
          textPosY         = 0;
  uint8_t lastvolume       = maxVolume;
  uint8_t lastsubsong      = 0;
  uint8_t deltaMM          = 0,
          lastMM           = 0;
  uint8_t deltaSS          = 0,
          lastSS           = 0;
  bool    draw_header      = false;
  bool    scrolltext       = false;

  if( param == NULL ) {
    log_e("Can't attach voice renderer to SIDExplorer");
    stoprender = false;
    log_w("Task will self-delete");
    deInitSprites();
    vTaskDelete( NULL );
  }

  stoprender     = false; // make the task stoppable from outside
  mySIDExplorer  = (SIDExplorer *) param;
  playerloopmode = mySIDExplorer->playerloopmode;

  initSprites();
  spriteText.setFont( &Font8x8C64 );
  spriteText.setTextSize(1);
  tft.fillRect(0, 16, tft.width(), 8, C64_DARKBLUE ); // clear area under the logo

  while(1) {
    if( stoprender ) break;
    if( render == false ) {
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

    if( lastsubsong != sidPlayer->currentsong ) {
      lastsubsong = sidPlayer->currentsong;
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
    if( sidPlayer->getPositionInPlaylist() != positionInPlaylist ) {
      currentprogress = 0;
      lastMM = 0;
      lastSS = 0;
      scrolltext = false; // wait for the next redraw
      draw_header = true;
    }

    if( scrolltext ) {
      scrollableText.doscroll();
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
      oscViews[voice].drawEnveloppe( voice, 0, HEADER_HEIGHT + ( voice*VOICE_HEIGHT ) );
    }

    if( showFPS ) renderFPS();
    vTaskDelay(10);
  }
  stoprender = false;
  log_w("Task will self-delete");
  deInitSprites();
  vTaskDelete( NULL );
}


#endif
