#pragma once

#include "../config.h"

namespace SIDView
{

  class ScrollableItem
  {
    public:
      ~ScrollableItem();
      ScrollableItem(LGFX* display);

      bool scroll = false;
      bool invert = true;
      bool smooth = true;

      void setup( const char* _text, uint16_t _xpos, uint16_t _ypos, uint16_t _width, uint16_t _height, unsigned long _delay=300, bool _invert=true, bool _smooth=true );
      void doscroll();
      void scrollSmooth();
      void scrollUgly();
    private:
      char text[256];
      uint16_t xpos   = 0;
      uint16_t ypos   = 0;
      uint16_t width  = 0;
      uint16_t height = 0;
      unsigned long last = millis();
      unsigned long delay = 300; // milliseconds
      uint8_t smooth_offset = 0;
      void* sptr = nullptr;
      LGFX_Sprite *scrollSprite = nullptr;//new LGFX_Sprite( &tft );
  };

}


namespace UI_Utils
{
  extern SIDView::ScrollableItem *listScrollableText;
  extern SIDView::ScrollableItem *songScrollableText;
};
