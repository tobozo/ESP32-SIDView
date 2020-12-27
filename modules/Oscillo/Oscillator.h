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

#ifndef _SID_OSCILLATOR_H_
#define _SID_OSCILLATOR_H_

#include "../ADSR/ADSR.h"


enum RateType
{
  RATE_ATTACK,
  RATE_DECAY,
  RATE_RELEASE
};


struct Oscillator
{
  uint32_t freqHz; // cycles per second ( Hertz )
  uint32_t freqms; // cycles per microsecond
  int32_t period; // in microseconds

  float invfreqms;
  bool state = false;

  Oscillator() {};

  Oscillator( uint32_t freq )
  {
    setFreqHz( freq );
  };

  void setFreqHz( uint32_t hz )
  {
    freqHz     = hz;
    freqms     = hz * 1000.0;
    if( hz == 0 ) return;
    period = 1000000 / freqHz;
    log_d("freqHz: %4d, freqms: %8d, periodms: %5d\n", freqHz, freqms, period );
  }

  float getTriangle( int32_t utime )
  {
    // ex: 3951Hz (or 3.951KHz) : 1 period = 253 microseconds (or 0.253 milliseconds)
    int32_t xpos       = utime < 0  ? period + ((utime+1)%-period) : utime%period;
    int32_t doublexpos = xpos*2;
    int32_t ypos       = doublexpos%period;
    int32_t yprogress  = (ypos*50)/(period);
    int32_t doubleyprogress = (doublexpos > period ? yprogress : 50.0-yprogress);
    return 0.5 - (doubleyprogress / 50.0);
  }

  float getSawTooth( int32_t utime )
  {
    int32_t ypos       = utime < 0  ? period + ((utime+1)%-period) : utime%period;
    int32_t yprogress  = 50 - ( (ypos*50)/(period) );
    return 0.5 - (yprogress / 50.0);
  }

  float getSquare( int32_t utime, int PWn )
  {
    // Where PWn is the 12-bit number in the Pulse Width registers.
    // Range from 0 to 4095 ($FFF) in the C64 pulse width registers
    // A value of 2048 ($800) will produce a square wave
    // A value of 4095 will not produce a constant DC output
    int32_t xpos       = utime < 0  ? period + ((utime+1)%-period) : utime%period;
    float pwn          = (PWn/4095.0) - 0.5; // PWn to per one range -0.5 <-> 0.5
    float xlow         = period/2 + period*pwn;
    bool state         = xpos > xlow;
    return (state ? 0.5 : -0.5);
  }

  float getNoise( int32_t utime, int PWn  )
  {
    int32_t xpos       = utime < 0  ? period + ((utime+1)%-period) : utime%period;
    float pwn          = (PWn/4095.0) - 0.5; // PWn to per one range -0.5 <-> 0.5
    float xlow         = period/2 +  period*pwn;
    int32_t ylevel     = rand()%50;
    bool state         = xpos > xlow;
    return (state ? ylevel/100.0 : -ylevel/100.0);
  }
};




struct OscilloView
{
  Oscillator *osc      = nullptr;
  ADSR *env            = nullptr;
  TFT_eSprite *sprite  = nullptr;
  int32_t* valuesCache = nullptr;
  int32_t  graphWidth;
  int32_t  graphHeight;
  int32_t  voicepos = 0;
  int32_t  lasty;
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
  bool     inited = false;


  ~OscilloView()
  {
    deinit();
  }


  OscilloView()
  {
    // justs an empty constructor
  };


  void init( uint16_t width, uint16_t height, uint32_t freqHz=1, uint8_t oscType=0, std::uint32_t fgcol=C64_LIGHTBLUE, std::uint32_t bgcol=C64_DARKBLUE  )
  {
    if( inited ) return;
    osc = new Oscillator( freqHz );
    env = new ADSR;
    sprite = new TFT_eSprite( &tft );
    //sprite->setPsram(false);
    sprite->setColorDepth(lgfx::palette_1bit);
    sprite->createSprite( width, height );
    sprite->setPaletteColor(0, bgcol );
    sprite->setPaletteColor(1, fgcol );
    graphWidth  = width;
    graphHeight = height - 2; // add some margin
    timeFrameRatio = float(timeFrameWidth_us) / float(graphWidth); // how many micros per pixel
    bgcolor = bgcol;
    fgcolor = fgcol;
    pan = osc->period*2;
    ticks = pan/graphWidth;
    valuesCache = (int32_t*)sid_calloc( width+1, sizeof( int32_t ) );
    log_w("OscView init [%dx%d]", graphWidth, graphHeight );
    if( valuesCache != NULL ) inited = true;
  }


  void deinit()
  {
    if( ! inited ) return;
    if( valuesCache != nullptr ) valuesCache=NULL;
    sprite->deleteSprite();
    //if( osc != nullptr) delete osc;
    //if( env != nullptr) delete env;
    inited = false;
  }


  void setADSR( float attack, float decay, float sustain, float release, float ratioA=0.05, float ratioDR=0.02 )
  {
    //env->reset();
    env->setAttackRate( attack ); // attackTime in ticks... // use adsr1.setAttackRate(12 * samplerate)
    env->setDecayRate( decay );
    env->setReleaseRate( release );
    env->setSustainLevel( sustain );
    env->setTargetRatioA( ratioA );
    env->setTargetRatioDR( ratioDR );
  }


  void setPulseWidth( uint16_t pw )
  {
    pulsewidth = pw;
  }


  void setFreqHz( uint32_t hz )
  {
    osc->setFreqHz( hz );
    pan = osc->period*2;
    ticks = pan/(graphWidth-1);
    //tt = 0;
  }


  void setOscType( uint8_t type )
  {
    oscType = type;
  }


  float getOscFloatValue( float t )
  {
    float ret = 0;
    float units = 0.0;
    if( oscType & SID_WAVEFORM_TRIANGLE ) {
      ret += osc->getTriangle( t );
      units++;
    }
    if( oscType & SID_WAVEFORM_SAWTOOTH ) {
      ret += osc->getSawTooth( t );
      units++;
    }
    if( oscType & SID_WAVEFORM_PULSE ) {
      ret += osc->getSquare( t, pulsewidth );
      units++;
    }
    if( oscType & SID_WAVEFORM_NOISE ) {
      ret += osc->getNoise( t, pulsewidth );
      units++;
    }
    if( units > 1 ) {
      ret /= units;
    }
    return ret;
  }


  uint32_t getOscValue( float t )
  {
    return getOscFloatValue( t ) * graphHeight;
  }


  void gate( uint8_t val )
  {
    if( val == 1 ) {
      env->reset();
    }
    env->gate( val );
  }


  void drawEnveloppe( int voice, int32_t x, int32_t y )
  {
    sprite->fillSprite( bgcolor );

    float    pulsewidth = sid.getPulse(voice);
    uint8_t  waveform   = sid.getWaveForm(voice);
    float    attack     = sid.getAttack(voice);  // 0-15
    float    decay      = sid.getDecay(voice);   // 0-15
    float    sustain    = sid.getSustain(voice); // 0-15
    float    release    = sid.getRelease(voice); // 0-15
    uint32_t fout       = sid.getFrequencyHz(voice);// * 0.00596; // Fout = (Fn * 0.0596) Hz
    bool     gate       = sid.getGate( voice );
    int  ringmode       = sid.getRingMode( voice );
    int      sync       = sid.getSync( voice );
    //snprintf( hexEnveloppe, 5, "%x%x%x%x", int(attack), int(decay), int(sustain), int(release) );
    //Serial.printf("[%s] -> ", hexEnveloppe );
    float sampleRate = timeFrameWidth_ms; // float( fps );

    attack  = getEnveloppeRateMs( RATE_ATTACK, attack );// / 1000.0;
    decay   = getEnveloppeRateMs( RATE_DECAY, decay );// / 1000.0;
    release = getEnveloppeRateMs( RATE_RELEASE, release );// / 1000.0;
    sustain /= 15.0;

    setADSR( attack  * sampleRate, decay   * sampleRate, sustain, release * sampleRate, 1.0, 1.0 );
    setFreqHz( fout );
    setOscType( waveform );
    setPulseWidth( pulsewidth );

    if( gated != gate ) {
      if( gate ) env->reset();
      env->gate( gate );
      gated = gate;
      voicepos = 0;
    }

    voicepos = -timeFrameRatio*(graphWidth/2);

    for( int i=0; i<graphWidth; i++ ) {
      float adsramplitude = env->process();
      voicepos += timeFrameRatio;
      float oscVal = getOscFloatValue( voicepos ); // range -0.5 <-> 0.5
      //outputArray[i] = (oscVal*graphHeight)+graphHeight/2;
      //float output = adsramplitude; // ADSR only range 0 <-> 1
      float output = adsramplitude*oscVal; // Oscillator + ADSR
      //float output  = oscVal; // Oscillator only
      valuesCache[i] = (output*graphHeight)+graphHeight/2;
      // TODO: add filter
    }

    for( int i=1; i<graphWidth; i++ ) {
      sprite->drawLine( i-1, valuesCache[i-1], i, valuesCache[i], fgcolor );
    }
    sprite->pushSprite( x, y );
  }


  int getEnveloppeRateMs( RateType type, uint8_t rate )
  {
    /*-----------ENVELOPE RATES-----------------\
    |   VALUE   | ATTACK RATE  | DECAY/RELEASE  |
    |-------------------------------------------|
    | DEC (HEX) | (Time/Cycle) |   (Time/Cycle) |
    |-------------------------------------------|
    | 0  | 0x0  |      2 mS    |       6 mS     |
    | 1  | 0x1  |      8 mS    |      24 mS     |
    | 2  | 0x2  |     16 mS    |      48 mS     |
    | 3  | 0x3  |     24 mS    |      72 mS     |
    | 4  | 0x4  |     38 mS    |     114 mS     |
    | 5  | 0x5  |     56 mS    |     168 mS     |
    | 6  | 0x6  |     68 mS    |     204 mS     |
    | 7  | 0x7  |     80 mS    |     240 mS     |
    | 8  | 0x8  |    100 mS    |     300 mS     |
    | 9  | 0x9  |    250 mS    |     750 mS     |
    | 10 | 0xa  |    500 mS    |     1.5 S      |
    | 11 | 0xb  |    800 mS    |     2.4 S      |
    | 12 | 0xc  |      1 S     |       3 S      |
    | 13 | 0xd  |      3 S     |       9 S      |
    | 14 | 0xe  |      5 S     |      15 S      |
    | 15 | 0xf  |      8 S     |      24 S      |
    \__________________________________________*/
    switch( type ) {
      case RATE_ATTACK:
        switch( rate ) {
          case 0x0:  return    2;
          case 0x1:  return    8;
          case 0x2:  return   16;
          case 0x3:  return   24;
          case 0x4:  return   38;
          case 0x5:  return   56;
          case 0x6:  return   68;
          case 0x7:  return   80;
          case 0x8:  return  100;
          case 0x9:  return  250;
          case 0xa:  return  500;
          case 0xb:  return  800;
          case 0xc:  return 1000;
          case 0xd:  return 3000;
          case 0xe:  return 5000;
          case 0xf:  return 8000;
          default:   return 1;
        }
      break;
      case RATE_DECAY:
      case RATE_RELEASE:
        switch( rate ) {
          case 0x0:  return     6  ;
          case 0x1:  return    24  ;
          case 0x2:  return    48  ;
          case 0x3:  return    72  ;
          case 0x4:  return   114  ;
          case 0x5:  return   168  ;
          case 0x6:  return   204  ;
          case 0x7:  return   240  ;
          case 0x8:  return   300  ;
          case 0x9:  return   750  ;
          case 0xa:  return  1500  ;
          case 0xb:  return  2400  ;
          case 0xc:  return  3000  ;
          case 0xd:  return  9000  ;
          case 0xe:  return 15000  ;
          case 0xf:  return 24000  ;
          default:   return 1;
        }
      break;
      default:
        return 1;
    }
  }

};


#endif
