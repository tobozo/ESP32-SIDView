
#include "ScrollableItem.hpp"
#include "UI_Utils.hpp"

namespace SIDView
{

  using namespace UI_Colors;

  static bool is_scrolling = false; // used to prevent simultaneous drawing when multithreaded


  ScrollableItem::~ScrollableItem()
  {
    if( sptr ) scrollSprite->deleteSprite();
  }

  ScrollableItem::ScrollableItem( LGFX* display )
  {
    if( !scrollSprite ) scrollSprite = new LGFX_Sprite( display );
    scrollSprite->setColorDepth(1);
    scrollSprite->setPsram( false );
    scrollSprite->setFont( &Font8x8C64 );
  };


  void ScrollableItem::setup( const char* _text, uint16_t _xpos, uint16_t _ypos, uint16_t _width, uint16_t _height, unsigned long _delay, bool _invert, bool _smooth )
  {
    snprintf( text, 255, " %s ", _text );
    xpos   = _xpos;
    ypos   = _ypos;
    width  = _width;
    height = _height;
    delay  = _delay;
    invert = _invert;
    smooth = _smooth;
    last   = millis()-_delay;
    scroll = true;

    log_d("text:%s, xpos:%d, ypos:%d, width:%d, height:%d, delay:%d, invert:%d, smooth", _text, _xpos, _ypos, _width, _height, _delay, _invert, _smooth);

    if( sptr ) scrollSprite->deleteSprite();

    if( smooth ) {
      uint8_t charwidth = scrollSprite->textWidth( "0" );
      sptr = scrollSprite->createSprite( width+charwidth, height );
    } else {
      sptr = scrollSprite->createSprite( width, height );
    }
    if( !sptr ) {
      log_e("Failed to create scroll sprite, aborting");
      return;
    }
    scrollSprite->setPaletteColor( 0, C64_DARKBLUE );
    scrollSprite->setPaletteColor( 1, C64_LIGHTBLUE );

    if( invert ) {
      scrollSprite->setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
    } else {
      scrollSprite->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    }
    //scrollSprite->fillSprite( C64_DARKBLUE );
    scrollSprite->setTextDatum( TL_DATUM );
  }


  void ScrollableItem::doscroll()
  {
    if( is_scrolling ) return; // thread safe
    if( !scroll ) return; // init safe
    if( scrollSprite == nullptr ) return; // mem safe
    if( millis()-last < delay )  return; // time safe

    last = millis();

    is_scrolling = true;

    //takeSidMuxSemaphore();

    if( smooth ) {
      scrollSmooth();
    } else {
      scrollUgly();
    }

    //giveSidMuxSemaphore();

    is_scrolling = false;
  }


  void ScrollableItem::scrollSmooth()
  {
    if( !sptr ) return;

    char c = text[0];
    size_t len = strlen( text );

    if( smooth_offset == 7 ) {
      memmove( text, text+1, len-1 );
      text[len-1] = c;
      text[len] = '\0';
      smooth_offset = 0;
    } else {
      smooth_offset++;
    }

    scrollSprite->drawString( text, -smooth_offset, 0 );
    scrollSprite->pushSprite( xpos, ypos );
  }


  void ScrollableItem::scrollUgly()
  {
    if( !sptr ) return;

    char c = text[0];
    size_t len = strlen( text );

    memmove( text, text+1, len-1 );
    text[len-1] = c;
    text[len] = '\0';

    scrollSprite->drawString( text, 0, 0 );
    scrollSprite->pushSprite( xpos, ypos );
  }


}; // end namespace SIDView
