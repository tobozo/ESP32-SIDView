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

#include "Oscillator.hpp"
#include "../Filters/LowPass.hpp"

#pragma GCC diagnostic ignored "-Wunused-variable"


typedef enum
{
  RATE_ATTACK,
  RATE_DECAY,
  RATE_RELEASE
} rateType_t;


class OscilloView
{

  public:

    ~OscilloView();
    OscilloView( LGFX* display, uint16_t _width, uint16_t _height );

    void     init( SID6581 *_sid, std::uint32_t fgcol, std::uint32_t bgcol, uint32_t freqHz=1, uint8_t oscType=0  );
    void     deinit();
    void     setADSR( float attack, float decay, float sustain, float release, float ratioA=0.05, float ratioDR=0.02 );
    void     setPulseWidth( uint16_t pw );
    void     setFreqHz( uint32_t hz );
    void     setOscType( uint8_t type );
    float    getOscFloatValue( float t );
    uint32_t getOscValue( float t );
    void     gate( uint8_t val );
    void     process( int voice );
    int      getEnveloppeRateMs( rateType_t type, uint8_t rate );
    float    getAvgLevel() { return valuesCacheAvg; }
    float    getFreq() { return osc->freqHz; }
    void     draw( int32_t x, int32_t y ) { if( oscSprite ) oscSprite->pushSprite( x, y ); }

  private:
    SID6581     *sid           = nullptr;
    Oscillator  *osc           = nullptr;//new Oscillator(1);
    ADSR        *env           = nullptr;//new ADSR;
    LGFX_Sprite *oscSprite     = nullptr;//new LGFX_Sprite( &tft );
    LowPass     *lowPassFilter = nullptr;//new LowPass();
    float*      valuesCache    = nullptr;
    float       valuesCacheAvg = 0;
    void* sptr                 = nullptr;

    int32_t  graphWidth;
    int32_t  spriteHeight;
    int32_t  graphHeight;
    int32_t  lasty;
    int32_t  samplepos = 0;
    uint32_t sampleRate_ms = 15; // millis => width of the OscView
    uint32_t sampleRate_us = sampleRate_ms /*ms*/ * 1000; // micros
    uint32_t bgcolor = 0xffffffu;
    uint32_t fgcolor = 0x000000u;
    uint16_t pulsewidth = 2048; // C64 register value, ranges 0 <-> 4095
    uint8_t  oscType = 0;
    float    timeFrameRatio; // = sampleRate_us/graphWidth = how many micros per pixel
    float    ticks;
    float    pan;
    bool     gated;


    // fps counter
    unsigned long framesCount = 0;
    uint32_t fstart = millis();
    int fpsInterval = 100;
    float fpsscale = 1000.0/fpsInterval; // fpi to fps
    int fps = 0;   // frames per second
    int lastfps = -1;
    float fpi = 0; // frames per interval
    bool showFPS = true;

    void renderFPS();

};


namespace UI_Utils
{
  extern OscilloView **oscViews;
};

