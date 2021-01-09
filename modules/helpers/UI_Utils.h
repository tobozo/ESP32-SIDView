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

#ifndef _SID_UI_UTILS_H
#define _SID_UI_UTILS_H

// some UI dimensions
#define HEADER_HEIGHT 41                               // arbitrarily chosen height
#define VOICE_HEIGHT  ((tft.height()-HEADER_HEIGHT)/3) // = 29 on a 128x128
#define VOICE_WIDTH   tft.width()


class ScrollableItem
{

  public:

    ScrollableItem()
    {
      scrollSprite->setColorDepth(1);
    };

    bool scroll = false;
    bool invert = true;
    bool smooth = true;

    void setup( const char* _text, uint16_t _xpos, uint16_t _ypos, uint16_t _width, uint16_t _height, unsigned long _delay=300, bool _invert=true, bool _smooth=true )
    {
      snprintf( text, 255, " %s ", _text );
      xpos   = _xpos;
      ypos   = _ypos;
      width  = _width;
      height = _height;
      last   = millis()-_delay;
      delay  = _delay;
      invert = _invert;
      smooth = _smooth;
      scroll = true;
      scrollSprite->setPsram( false );
    }

    void doscroll()
    {
      if(!scroll) return;
      if( scrollSprite == nullptr ) return;
      if( millis()-last < delay )  return;
      last = millis();

      if( smooth ) {
        scrollSmooth();
      } else {
        scrollUgly();
      }

    }


    void scrollSmooth()
    {

      scrollSprite->setFont( &Font8x8C64 );
      uint8_t charwidth = scrollSprite->textWidth( "0" );
      scrollSprite->createSprite( width+charwidth, height );
      scrollSprite->setPaletteColor( 0, C64_DARKBLUE );
      scrollSprite->setPaletteColor( 1, C64_LIGHTBLUE );

      if( invert ) {
        scrollSprite->setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
      } else {
        scrollSprite->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
      }

      scrollSprite->fillSprite( C64_DARKBLUE );
      scrollSprite->setTextDatum( TL_DATUM );

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

      scrollSprite->deleteSprite();
    }


    void scrollUgly()
    {
      scrollSprite->createSprite( width, height );
      scrollSprite->setPaletteColor( 0, C64_DARKBLUE );
      scrollSprite->setPaletteColor( 1, C64_LIGHTBLUE );
      scrollSprite->setFont( &Font8x8C64 );

      if( invert ) {
        scrollSprite->setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
      } else {
        scrollSprite->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
      }

      scrollSprite->fillSprite( C64_DARKBLUE );
      scrollSprite->setTextDatum( TL_DATUM );

      char c = text[0];
      size_t len = strlen( text );

      memmove( text, text+1, len-1 );
      text[len-1] = c;
      text[len] = '\0';

      scrollSprite->drawString( text, 0, 0 );
      scrollSprite->pushSprite( xpos, ypos );

      scrollSprite->deleteSprite();
    }


  private:

    char text[256];
    uint16_t xpos   = 0;
    uint16_t ypos   = 0;
    uint16_t width  = 0;
    uint16_t height = 0;
    unsigned long last = millis();
    unsigned long delay = 300; // milliseconds
    uint8_t smooth_offset = 0;

    TFT_eSprite *scrollSprite = new TFT_eSprite( &tft );

};



static void UIPrintTitle( const char* title, const char* message = nullptr )
{
  tft.setTextDatum( MC_DATUM );
  tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
  int yPos = ( tft.height()/2 - tft.fontHeight()*4 ) + 2;
  tft.fillRect( 0, yPos - tft.fontHeight()/2, tft.width(), tft.fontHeight()+1, C64_DARKBLUE );
  tft.drawString( title, tft.width()/2, yPos );
  if( message != nullptr ) {
    yPos = ( tft.height()/2 - tft.fontHeight()*2 ) + 2;
    tft.fillRect( 0, yPos - tft.fontHeight()/2, tft.width(), tft.fontHeight()+1, C64_DARKBLUE );
    tft.drawString( message, tft.width()/2, yPos );
  }
}


static void UIPrintProgressBar( float progress, float total=100.0 )
{
  static int8_t lastprogress = -1;
  if( (uint8_t)progress != lastprogress ) {
    lastprogress = (uint8_t)progress;
    char progressStr[64] = {0};
    snprintf( progressStr, 64, "Progress: %3d%s", int(progress), "%" );
    tft.setTextDatum( MC_DATUM );
    tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    tft.drawString( progressStr, tft.width()/2, tft.height()/2 );
    //tft.setCursor( tft.height()-(tft.fontHeight()+2), 10 );
    //tft.printf("    heap: %6d    ", ESP.getFreeHeap() );
  }
}

static void UIPrintGzProgressBar( uint8_t progress )
{
  UIPrintProgressBar( (float)progress );
}

static void UIPrintTarProgress(const char* format, ...)
{
  //char buffer[256];
  char *buffer = (char *)sid_calloc( 256, sizeof(char) );
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, 255, format, args);
  va_end(args);
  //Serial.println(buffer);
  tft.setTextDatum( MC_DATUM );
  tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
  int yPos = (tft.height()/2 - tft.fontHeight()*2) + 2;
  tft.fillRect( 0, yPos - tft.fontHeight()/2, tft.width(), tft.fontHeight()+1, C64_DARKBLUE );
  tft.drawString( buffer, tft.width()/2, yPos );
  free( buffer );
}

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


void drawBezierCurve( TFT_eSprite &scrollSprite, vec2* points, int numPoints, std::uint32_t color ) {
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

void drawBezierCurve( TFT_eSprite &scrollSprite, int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, int x5, int y5, uint16_t color ) {

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


// fps counter
unsigned long framesCount = 0;
uint32_t fstart = millis();
int fpsInterval = 100;
float fpsscale = 1000.0/fpsInterval; // fpi to fps
int fps = 0;   // frames per second
int lastfps = 0;
float fpi = 0; // frames per interval
bool showFPS = false;

void renderFPS() {
  unsigned long nowMillis = millis();
  if(nowMillis - fstart >= fpsInterval) {
    fpi = float(framesCount * fpsInterval) / float(nowMillis - fstart);
    fps = int(fpi*fpsscale);
    fstart = nowMillis;
    framesCount = 0;
  } else {
    framesCount++;
  }
  if( fps != lastfps ) {
    tft.setCursor( 0, 0 );
    tft.setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
    tft.setTextDatum( TL_DATUM );
    tft.printf( "fps: %3d  ", fps );
  }
}

#endif
