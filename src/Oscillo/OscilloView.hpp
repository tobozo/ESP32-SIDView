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

struct OscilloInfo
{
  bool        gate{false};
  bool    ringmode{false};
  bool        sync{false};
  float pulsewidth{0.0f};
  float     attack{0.0f};
  float      decay{0.0f};
  float    sustain{0.0f};
  float    release{0.0f};
  uint8_t waveform{0};
  uint32_t    fout{0};
  bool has_filter{false};
  bool has_lowpass{false};
  bool has_highpass{false};
  bool has_bandpass{false};
  bool has_lowpass_filter{false};
  uint16_t LowHiFilterFreq{false};
  uint8_t ResonanceHex{false};
  float Cutoff{0.0f};
  float Resonance{0.0f};

  void load( SID_Registers_t* registers, int voice )
  {
    auto voiceregs = registers->voice(voice);

          gate = voiceregs.gate();
      ringmode = voiceregs.ringmode();
          sync = voiceregs.sync();
    pulsewidth = voiceregs.pulse();
        attack = voiceregs.attack();    // 0-15
         decay = voiceregs.decay();     // 0-15
       sustain = voiceregs.sustain();   // 0-15
       release = voiceregs.release();   // 0-15
      waveform = voiceregs.waveform();  // ADSR nibbles
          fout = (double)voiceregs.freq()/16.777216; // * 0.00596; // Fout = (Fn * 0.0596) Hz

    has_filter = false;

    switch( voice ) {
      case 0: has_filter = registers->filt1(); break;
      case 1: has_filter = registers->filt2(); break;
      case 2: has_filter = registers->filt3() && !registers->mode3off(); break;
      default: break;
    }

    // When set to a one, the low Pass output of the Filter is selected and sent to the audio output.
    // For a given Filter input Signal, all frequency components below the Filter Cutoff Frequency
    // are passed unaltered, while all frequency components above the Cutoff are attenuated at a rate
    // of 12 dB/Octave. The low Pass mode produces full-bodied sounds.
    has_lowpass = registers->lp(); // low pass

    // Same as bit 4 for the High Pass output. All frequency components above the Cutoff are passed
    // unaltered, while all frequency components below the Cutoff are attenuated at a rate of 12
    // dB/Octave. The High Pass mode produces tinny, buzzy sounds.
    has_highpass = registers->hp(); // high pass

    // Same as bit 4 for the Band Pass output. All frequency components above and below the Cutoff
    // are attenuated at a rate of 6 dB/Octave. The Band Pass mode produces thin, open sounds.
    has_bandpass = registers->bp(); // band pass

    // The approximate Cutoff Frequency ranges between 30Hz and 12KHz (11bit value)
    LowHiFilterFreq = registers->cutoff(); // sid->getFilterFrequency();

    // Bits 4-7 of this register (RES0-RES3) control the Resonance of the Filler, resonance of a
    // peaking effect which emphasizes frequency components at the Cutoff Frequency of the Filter,
    // causing a sharper sound. There are 16 Resonance settings ranging linearly from no resonance
    // (0) to maximum resonance (15 or #F).
    //int ResonanceHex = sid->getResonance();
    ResonanceHex = registers->resonance();

    // check if this voice has lowpass filter enabled
    has_lowpass_filter = has_filter && has_lowpass;

    if( has_lowpass_filter ) {
      // map those values to [0...1] float range
      Cutoff        = 1.0 - float( map( LowHiFilterFreq, 30, 10000, 0, 1000 ) ) / 1000.0f;
      Resonance     = float( map( ResonanceHex, 0, 15, 0, 1000 ) ) / 1000.0f;
      //lowPassFilter->setCutoffFreqAndResonance( Cutoff, Resonance );
    }

    attack  = getEnveloppeRateMs( RATE_ATTACK, attack );
    decay   = getEnveloppeRateMs( RATE_DECAY, decay );
    release = getEnveloppeRateMs( RATE_RELEASE, release );
    sustain = sustain/15.0f;

  }

  int getEnveloppeRateMs( rateType_t type, uint8_t rate );


};




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
    float    getAvgLevel() { return valuesCacheAvg; }
    float    getFreq() { return osc->freqHz; }
    void     draw( int32_t x, int32_t y ) { if( oscSprite ) oscSprite->pushSprite( x, y ); }
    void     setRegisters( SID_Registers_t* reg ) { registers = reg; }

  private:
    SID6581     *sid           = nullptr;
    Oscillator  *osc           = nullptr;//new Oscillator(1);
    ADSR        *env           = nullptr;//new ADSR;
    SID_Registers_t* registers = nullptr;
    LGFX_Sprite *oscSprite     = nullptr;//new LGFX_Sprite( &tft );
    LowPass     *lowPassFilter = nullptr;//new LowPass();
    float*      valuesCache    = nullptr;
    float       valuesCacheAvg = 0;
    void* sptr                 = nullptr;

    OscilloInfo info;

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

  private:
    // fps counter
    unsigned long framesCount = 0;
    uint32_t fstart = millis();
    int fpsInterval = 100;
    float fpsscale = 1000.0/fpsInterval; // fpi to fps
    int fps = 0;   // frames per second
    int lastfps = -1;
    float fpi = 0; // frames per interval
    bool showFPS = true;
    bool showWaveform = true;
    uint8_t showUnits = 0;

    void renderFPS();
    void drawIcon( lgfx::pixelcopy_t *icon_data, uint32_t xoffset, uint32_t yoffset=8, uint32_t width=16, uint32_t height=8 );
    void renderWaveform();

};


namespace UI_Utils
{
  static OscilloView **oscViews = nullptr;
};

