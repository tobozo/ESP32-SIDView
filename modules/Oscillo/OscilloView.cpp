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


#include "OscilloView.hpp"


OscilloView::~OscilloView()
{
  deinit();
}

OscilloView::OscilloView( uint16_t _width, uint16_t _height ) : graphWidth(_width), spriteHeight(_height)
{
  graphHeight = spriteHeight - 2;
};


void OscilloView::init( SID6581 *_sid, uint32_t freqHz, uint8_t oscType, std::uint32_t fgcol, std::uint32_t bgcol )
{
  sid = _sid;
  osc->setFreqHz( freqHz );
  env->reset();
  oscSprite->setPsram(false);
  oscSprite->setColorDepth(lgfx::palette_1bit);
  //oscSprite->createSprite( graphWidth, spriteHeight );
  oscSprite->setPaletteColor( 0, bgcol );
  oscSprite->setPaletteColor( 1, fgcol );
  timeFrameRatio = float(timeFrameWidth_us) / float(graphWidth); // how many micros per pixel
  bgcolor = bgcol;
  fgcolor = fgcol;
  pan = osc->period*2;
  ticks = pan/graphWidth;
  //if( valuesCache == nullptr ) {
  //  valuesCache = (int32_t*)sid_calloc( graphWidth+1, sizeof( int32_t ) );
  //}
  //memset( valuesCache, 0, graphWidth+1*sizeof( int32_t ) );
  log_d("[%d] OscView init [%dx%d]", ESP.getFreeHeap(), graphWidth, graphHeight );
}


void OscilloView::deinit()
{
  //oscSprite->deleteSprite();
  /*
  if( valuesCache != nullptr ) {
    free( valuesCache );
    valuesCache = nullptr;
  }
  */
}


void OscilloView::setADSR( float attack, float decay, float sustain, float release, float ratioA, float ratioDR )
{
  env->setAttackRate( attack ); // attackTime in ticks... // use adsr1.setAttackRate(12 * samplerate)
  env->setDecayRate( decay );
  env->setReleaseRate( release );
  env->setSustainLevel( sustain );
  env->setTargetRatioA( ratioA );
  env->setTargetRatioDR( ratioDR );
}


void OscilloView::setPulseWidth( uint16_t pw )
{
  pulsewidth = pw;
}


void OscilloView::setFreqHz( uint32_t hz )
{
  osc->setFreqHz( hz );
  pan = osc->period*2;
  ticks = pan/(graphWidth-1);
  //tt = 0;
}


void OscilloView::setOscType( uint8_t type )
{
  oscType = type;
}


float OscilloView::getOscFloatValue( float t )
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


uint32_t OscilloView::getOscValue( float t )
{
  return getOscFloatValue( t ) * graphHeight;
}


void OscilloView::gate( uint8_t val )
{
  if( val == 1 ) {
    env->reset();
  }
  env->gate( val );
}


void OscilloView::drawEnveloppe( int voice, int32_t x, int32_t y )
{
  const char* sptr = (const char*)oscSprite->createSprite( graphWidth, spriteHeight );
  if( !sptr ) {
    log_d("Not enough ram to create enveloppe sprite");
    return;
  }
  valuesCache = (int32_t*)sid_calloc( graphWidth+1, sizeof( int32_t ) );
  if( valuesCache == NULL ) {
    log_d("Not enough ram to store enveloppe data");
    return;
  }
  oscSprite->setPaletteColor( 0, bgcolor );
  oscSprite->setPaletteColor( 1, fgcolor );
  //oscSprite->fillSprite( bgcolor );

  float    pulsewidth = sid->getPulse(voice);
  uint8_t  waveform   = sid->getWaveForm(voice);
  float    attack     = sid->getAttack(voice);  // 0-15
  float    decay      = sid->getDecay(voice);   // 0-15
  float    sustain    = sid->getSustain(voice); // 0-15
  float    release    = sid->getRelease(voice); // 0-15
  uint32_t fout       = sid->getFrequencyHz(voice);// * 0.00596; // Fout = (Fn * 0.0596) Hz
  bool     gate       = sid->getGate( voice );
  __attribute__((unused))
  int  ringmode       = sid->getRingMode( voice );
  __attribute__((unused))
  int      sync       = sid->getSync( voice );

  oscSprite->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
  oscSprite->setTextDatum( TL_DATUM );
  char text[24] = {0};

  /*
  int Resonance = sid->getResonance(0);
  int LowHiFilterFreq = sid->getFilterFrequency(0); // The approximate Cutoff Frequency ranges between 30Hz and 10KHz
  int FilterEx = sid->getFilterEX(0);

  snprintf( text, 24, "F: %02x - H/L: %04x", filtervalue, LowHiFilterFreq );
  oscSprite->drawString( text, 0,0 );

  snprintf( text, 24, "R: %02x - X: %02x", Resonance, FilterEx );
  oscSprite->drawString( text, 0,8 );
  */


  float sampleRate = timeFrameWidth_ms; // float( fps );

  attack  = getEnveloppeRateMs( RATE_ATTACK, attack );// / 1000.0;
  decay   = getEnveloppeRateMs( RATE_DECAY, decay );// / 1000.0;
  release = getEnveloppeRateMs( RATE_RELEASE, release );// / 1000.0;
  sustain /= 15.0;

  setADSR( attack * sampleRate, decay * sampleRate, sustain, release * sampleRate, 1.0, 1.0 );
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



  int filtervalue = 0;
  switch( voice ) {
    case 0: filtervalue = sid->getFilter1(0); break;
    case 1: filtervalue = sid->getFilter2(0); break;
    case 2: filtervalue = sid->getFilter3(0); break;
    default: filtervalue = 0;
  }

  if( filtervalue == 1 ) {
    int HP = sid->getHP(0); // high pass
    int BP = sid->getBP(0); // band pass
    int LP = sid->getLP(0); // low pass


    if( LP == 1 ) {

      //snprintf( text, 24, "H:%02x B:%02x L:%02x", HP, BP, LP );
      //oscSprite->drawString( text, 0,16 );
      lowPassFilter->SetParameters( 0.75, 0.02 );
      for( int i=0; i<graphWidth; i++ ) {
        valuesCache[i] = lowPassFilter->Process( valuesCache[i] );
      }
    }

  }


  if( showFPS )
    renderFPS();

  for( int i=1; i<graphWidth; i++ ) {
    oscSprite->drawLine( i-1, valuesCache[i-1], i, valuesCache[i], fgcolor );
  }

  oscSprite->pushSprite( x, y );
  oscSprite->deleteSprite();
  free( valuesCache );

}


void OscilloView::renderFPS() {
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
    lastfps = fps;
  }
  oscSprite->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
  oscSprite->setTextDatum( TL_DATUM );
  char text[12] = {0};
  snprintf( text, 12, "fps: %3d", fps );
  oscSprite->drawString( text, 0,0 );

}


int OscilloView::getEnveloppeRateMs( rateType_t type, uint8_t rate )
{


  /*------------ENVELOPE RATES---------------*\
  |   VALUE   | ATTACK RATE   | DECAY/RELEASE |
  |-------------------------------------------|
  | DEC (HEX) | (Time/Cycle)  | (Time/Cycle)  |
  |-------------------------------------------|
  | 0  | 0x0  |      2 mS     |      6 mS     |
  | 1  | 0x1  |      8 mS     |     24 mS     |
  | 2  | 0x2  |     16 mS     |     48 mS     |
  | 3  | 0x3  |     24 mS     |     72 mS     |
  | 4  | 0x4  |     38 mS     |    114 mS     |
  | 5  | 0x5  |     56 mS     |    168 mS     |
  | 6  | 0x6  |     68 mS     |    204 mS     |
  | 7  | 0x7  |     80 mS     |    240 mS     |
  | 8  | 0x8  |    100 mS     |    300 mS     |
  | 9  | 0x9  |    250 mS     |    750 mS     |
  | 10 | 0xa  |    500 mS     |    1.5 S      |
  | 11 | 0xb  |    800 mS     |    2.4 S      |
  | 12 | 0xc  |      1 S      |      3 S      |
  | 13 | 0xd  |      3 S      |      9 S      |
  | 14 | 0xe  |      5 S      |     15 S      |
  | 15 | 0xf  |      8 S      |     24 S      |
  \*_________________________________________*/



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
