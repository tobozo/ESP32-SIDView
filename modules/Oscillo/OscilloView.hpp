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
#ifndef _SID_OSCILLO_VIEW_H_
#define _SID_OSCILLO_VIEW_H_

#include "Oscillator.hpp"
#include "../Filters/LowPass.h"


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
    OscilloView( uint16_t _width, uint16_t _height );

    void     init( SID6581 *_sid, uint32_t freqHz=1, uint8_t oscType=0, std::uint32_t fgcol=C64_LIGHTBLUE, std::uint32_t bgcol=C64_DARKBLUE  );
    void     deinit();
    void     setADSR( float attack, float decay, float sustain, float release, float ratioA=0.05, float ratioDR=0.02 );
    void     setPulseWidth( uint16_t pw );
    void     setFreqHz( uint32_t hz );
    void     setOscType( uint8_t type );
    float    getOscFloatValue( float t );
    uint32_t getOscValue( float t );
    void     gate( uint8_t val );
    void     drawEnveloppe( int voice, int32_t x, int32_t y );
    int      getEnveloppeRateMs( rateType_t type, uint8_t rate );

  private:
    SID6581     *sid           = nullptr;
    Oscillator  *osc           = new Oscillator(1);
    ADSR        *env           = new ADSR;
    TFT_eSprite *oscSprite     = new TFT_eSprite( &tft );
    LowPass     *lowPassFilter = new LowPass();
    int32_t* valuesCache       = nullptr;

    int32_t  graphWidth;
    int32_t  spriteHeight;
    int32_t  graphHeight;
    int32_t  lasty;
    int32_t  voicepos = 0;
    uint32_t timeFrameWidth_ms = 15; // millis => width of the OscView
    uint32_t timeFrameWidth_us = timeFrameWidth_ms /*ms*/ * 1000; // micros
    uint32_t bgcolor = C64_DARKBLUE;
    uint32_t fgcolor = C64_LIGHTBLUE;
    uint16_t pulsewidth = 2048; // C64 register value, ranges 0 <-> 4095
    uint8_t  oscType = 0;
    float    timeFrameRatio; // = timeFrameWidth_us/graphWidth = how many micros per pixel
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


  // dafuq Arduino IDE ???
  #ifdef ARDUINO
    #include "OscilloView.cpp"
  #endif


#endif
