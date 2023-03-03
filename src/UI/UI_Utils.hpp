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

#include "../config.h"
#include "../assets/assets.h"
#include "./ScrollableItem.hpp"
#include "../Oscillo/OscilloView.hpp"
#include "../helpers/HID/HID_Common.hpp"

#define takeSidMuxSemaphore() if( SIDView::mux ) { xSemaphoreTake(SIDView::mux, portMAX_DELAY); /*log_v("Took Semaphore");*/ }
#define giveSidMuxSemaphore() if( SIDView::mux ) { xSemaphoreGive(SIDView::mux); /*log_v("Gave Semaphore");*/ }

namespace UI_Dimensions
{
  // some UI dimensions
  extern const uint16_t HeaderHeight  ; // based on header image size
  extern const uint16_t TouchPadHeight; // value calibrated for footer in 240x320 portrait mode
  extern const uint16_t HEADER_HEIGHT ; // arbitrarily chosen height, contains logo + song header + progress bar
  // calculated at boot
  extern uint16_t VOICE_HEIGHT        ;  // ((display->height()-HEADER_HEIGHT)/3); // = 29 on a 128x128
  extern uint16_t VOICE_WIDTH         ;  // display->width();
  extern uint16_t spriteWidth         ;  // display->width();
  extern uint16_t spriteHeight        ;  // display->height()-HeaderHeight;
  extern uint16_t spriteFooterY       ;  // display->height()-fontSize()+1
  extern uint16_t lineHeight          ;  // fontSize()*1.5
  extern uint16_t minScrollableWidth  ;  // display->width()-14
};


namespace UI_Colors
{
  // hail the c64 rgb888 colors !
  #define C64_DARKBLUE      0x4338ccU
  #define C64_LIGHTBLUE     0xC9BEFFU
  #define C64_MAROON        0xA1998BU
  #define C64_MAROON_DARKER 0x797264U
  #define C64_MAROON_DARK   0x938A7DU
  // hail the c64 rgb565 colors !
  #define C64_DARKBLUE_565      display->color565(0x43, 0x38, 0xcc )
  #define C64_LIGHTBLUE_565     display->color565(0xC9, 0xBE, 0xFF )
  #define C64_MAROON_565        display->color565(0xA1, 0x99, 0x8B )
  #define C64_MAROON_DARKER_565 display->color565(0x79, 0x72, 0x64 )
  #define C64_MAROON_DARK_565   display->color565(0x93, 0x8A, 0x7D )
};


namespace UI_Utils
{
  using namespace UI_Dimensions;
  using namespace UI_Colors;

  extern uint8_t folderDepth;
  extern uint16_t itemsPerPage;
  extern uint8_t* loopmodeicon;
  extern size_t loopmodeicon_len;
  extern UI_mode_t UI_mode;
  //extern uint32_t inactive_since;// = 0; // UI debouncer
  extern bool explorer_needs_redraw;// = false; // sprite ready to be pushed
  extern bool songheader_needs_redraw;

  void init();
  void sleep();
  void drawWaveforms();
  void drawHeader();
  void drawPagedList();

  void drawCursor( bool state, int32_t cursorX, int32_t cursorY, uint8_t fontWidth, uint8_t fontHeight, uint32_t fgcolor, uint32_t bgcolor );
  void blinkCursor( int32_t cursorX, int32_t cursorY, uint8_t fontWidth, uint8_t fontHeight, uint32_t fgcolor, uint32_t bgcolor );

  void infoWindow( const char* txt );
  void drawInfoWindow( const char* text, const lgfx::TextStyle& style, LoadingScreen_t* loadingScreen, bool is_fs=true );

  void drawToolPage( const char* title, const unsigned char* img, size_t img_len, const char* text[]={nullptr},
                     size_t linescount=0, uint8_t ttDatum=TC_DATUM, uint8_t txDatum=TL_DATUM );

  // progress ticker
  bool diskTicker( const char* title, size_t totalitems );

  // generic progress bar
  void UIPrintProgressBar( float progress, float total=100.0, uint16_t xpos = 64, uint16_t ypos = 64, const char *text="Progress:" );

  // HVSC songlengths parsing progress
  void MD5ProgressCb( size_t current, size_t total );

  // icons
  void drawVolumeIcon( int16_t x, int16_t y, uint8_t volume );
  void drawC64logo( int32_t posx, int32_t posy, float output_size=100.0, uint32_t fgcolor=C64_DARKBLUE, uint32_t bgcolor=C64_LIGHTBLUE );

  #if defined HID_TOUCH
    struct TouchButtonAction_t
    {
      TouchButton *button;
      const char* label;
      uint8_t font_size;
      HIDControls action;
      int16_t x,y,w,h;
      TouchButton::drawCb cb;
    };

    struct TouchButtonWrapper_t
    {
      bool iconRendered = false;
      LGFX_Sprite *buttonsSprite;
      int16_t spritePosX, spritePosY;
      void handlePressed( TouchButton *btn, bool pressed, int16_t x, int16_t y);
      void handleJustPressed( TouchButton *btn, const char* label );
      bool justReleased( TouchButton *btn, bool pressed, const char* label );
      void draw();
    };

    static TouchButtonAction_t UIBtns[8]; // will be inited from HID_Touch.cpp
    extern TouchButtonWrapper_t tbWrapper;
    extern int touchSpriteOffset;

    void registerButtonAction( LGFX_Sprite *sprite, TouchButtonAction_t *btna );
    void setupUIBtns( LGFX_Sprite *sprite, int x, int y, int w, int h, int xoffset, int yoffset );

    void drawTouchButtons();
    void releaseTouchButtons();

    void shapeTriangleUpCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label );
    void shapeTriangleDownCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label );
    void shapeTrianglePrevCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label );
    void shapeTriangleNextCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label );
    void shapeTriangleRightCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label );
    void shapeLoopToggleCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label );
    void shapePlusCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label );
    void shapeMinusCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label );
  #endif


  #if defined SID_DOWNLOAD_ARCHIVES
    // archive manager
    void UIPrintTitle( const char* title, const char* message = nullptr );
    void UIProgressBarPrinter( float progress, float total=100.0 );
  #endif

  #if defined ENABLE_HVSC_SEARCH
    // search
    void initSearchUI();
    void deinitSearchUI();
    bool keywordsTicker( const char* title, size_t totalitems );
    void keyWordsProgress( size_t current, size_t total, size_t words_count, size_t files_count, size_t folders_count  );
    void UIDrawProgressBar( float progress, float total=100.0, uint16_t xpos = 64, uint16_t ypos = 64, const char *text="Progress:" );
  #endif


}; // end namespace
