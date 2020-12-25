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

#include "assets.h"
TFT_eSprite spriteText = TFT_eSprite( &tft );


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


void drawBezierCurve( TFT_eSprite &sprite, vec2* points, int numPoints, std::uint32_t color ) {
  int lastx = -1, lasty = -1;
  for( float i = 0 ; i < 1 ; i += 0.05 ) {
    vec2 coords = getBezierPoint( points, numPoints, i );
    if( lastx==-1 || lasty==-1 ) {
      sprite.drawPixel( coords.x , coords.y , color );
    } else {
      sprite.drawLine( lastx, lasty, coords.x, coords.y, color );
    }
    lastx = coords.x;
    lasty = coords.y;
  }
}

/*
 * non recursive bézier curve with hardcoded 5 points

int getPt( int n1 , int n2 , float perc ) {
  int diff = n2 - n1;
  return n1 + ( diff * perc );
}

void drawBezierCurve( TFT_eSprite &sprite, int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, int x5, int y5, uint16_t color ) {

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
      sprite.drawPixel( x , y , color );
    } else {
      sprite.drawLine( lastx, lasty, x, y, color );
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
