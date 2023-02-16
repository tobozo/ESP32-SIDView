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
#include "../UI/UI_Utils.hpp"

using namespace UI_Colors;


OscilloView::~OscilloView()
{
  if( sptr != nullptr ) {
    oscSprite->deleteSprite();
  }
  if( valuesCache ) free( valuesCache );
}

OscilloView::OscilloView( LGFX* display, uint16_t _width, uint16_t _height ) : graphWidth(_width), spriteHeight(_height)
{
  graphHeight = spriteHeight - 2;

  osc           = new Oscillator(1);
  env           = new ADSR;
  oscSprite     = new LGFX_Sprite( display );
  lowPassFilter = new LowPass();
};


void OscilloView::init( SID6581 *_sid, std::uint32_t fgcol, std::uint32_t bgcol, uint32_t freqHz, uint8_t oscType )
{
  sid = _sid;
  osc->setFreqHz( freqHz );
  env->reset();
  //oscSprite->createSprite( graphWidth, spriteHeight );
  //static void *sptr = nullptr;
  bool sprite_cached = false;

  if( sptr == nullptr ) {
    oscSprite->setPsram(false);
    oscSprite->setColorDepth(lgfx::palette_1bit);
    sptr = oscSprite->createSprite( graphWidth, spriteHeight );
    if( !sptr ) {
      log_e("Not enough ram to create enveloppe sprite");
      return;
    }
    oscSprite->setPaletteColor( 0, bgcol );
    oscSprite->setPaletteColor( 1, fgcol );
    oscSprite->setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    oscSprite->setTextDatum( TL_DATUM );
    sprite_cached = true;
  }

  if( !valuesCache ){
    valuesCache = (float*)calloc( graphWidth+1, sizeof( float ) );
    if( valuesCache == NULL ) {
      log_e("Failed to create values cache");
      return;
    }
  }

  timeFrameRatio = float(sampleRate_us) / float(graphWidth); // how many micros per pixel
  bgcolor = bgcol;
  fgcolor = fgcol;
  pan = osc->period_us*2;
  ticks = pan/graphWidth;

  log_d("[%d][%s] OscView init [%dx%d]", ESP.getFreeHeap(), sprite_cached?"H":"C", graphWidth, graphHeight );
}


void OscilloView::deinit()
{

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
  pan = osc->period_us*2;
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


void OscilloView::process( int voice )
{
  //memset( valuesCache, 0, graphWidth+1 );
  valuesCacheAvg = 0;
  if( valuesCache == NULL ) {
    log_d("Not enough ram to store enveloppe data");
    return;
  }

  oscSprite->fillSprite( bgcolor ); // clear sprite

  //auto registers = SID6581::copyRegisters();
  //auto voiceregs = registers.voice(voice);

  auto registers = (SID_Registers_t*)sid->getRegisters();
  auto voiceregs = registers->voice(voice);


  bool        gate = voiceregs.gate();
  bool    ringmode = voiceregs.ringmode();
  bool        sync = voiceregs.sync();
  float pulsewidth = voiceregs.pulse();
  float     attack = voiceregs.attack();
  float      decay = voiceregs.decay();
  float    sustain = voiceregs.sustain();
  float    release = voiceregs.release();
  uint8_t waveform = voiceregs.waveform();
  uint32_t    fout = (double)voiceregs.freq()/16.777216;

  // bool        gate = sid->getGate( voice )==1;
  // uint8_t waveform = sid->getWaveForm(voice);
  // uint32_t    fout = sid->getFrequencyHz(voice);// * 0.00596; // Fout = (Fn * 0.0596) Hz
  // int     ringmode = sid->getRingMode( voice );
  // int         sync = sid->getSync( voice );
  // float pulsewidth = sid->getPulse(voice);
  // float     attack = sid->getAttack(voice);  // 0-15
  // float      decay = sid->getDecay(voice);   // 0-15
  // float    sustain = sid->getSustain(voice); // 0-15
  // float    release = sid->getRelease(voice); // 0-15

  bool has_filter = false;

  switch( voice ) {
    // case 0: has_filter = sid->getFilter1()==1; break;
    // case 1: has_filter = sid->getFilter2()==1; break;
    // case 2: has_filter = sid->getFilter3()==1 && sid->get3OFF()==0; break;
    case 0: has_filter = registers->filt1(); break;
    case 1: has_filter = registers->filt2(); break;
    case 2: has_filter = registers->filt3() && !registers->mode3off(); break;
    default: break;
  }

  // When set to a one, the low Pass output of the Filter is selected and sent to the audio output.
  // For a given Filter input Signal, all frequency components below the Filter Cutoff Frequency
  // are passed unaltered, while all frequency components above the Cutoff are attenuated at a rate
  // of 12 dB/Octave. The low Pass mode produces full-bodied sounds.
  bool has_lowpass = registers->lp(); // low pass

  // Same as bit 4 for the High Pass output. All frequency components above the Cutoff are passed
  // unaltered, while all frequency components below the Cutoff are attenuated at a rate of 12
  // dB/Octave. The High Pass mode produces tinny, buzzy sounds.
  bool has_highpass = registers->hp(); // high pass

  // Same as bit 4 for the Band Pass output. All frequency components above and below the Cutoff
  // are attenuated at a rate of 6 dB/Octave. The Band Pass mode produces thin, open sounds.
  bool has_bandpass = registers->bp(); // band pass

  // The approximate Cutoff Frequency ranges between 30Hz and 12KHz (11bit value)
  uint16_t LowHiFilterFreq = registers->cutoff(); // sid->getFilterFrequency();

  // Bits 4-7 of this register (RES0-RES3) control the Resonance of the Filler, resonance of a
  // peaking effect which emphasizes frequency components at the Cutoff Frequency of the Filter,
  // causing a sharper sound. There are 16 Resonance settings ranging linearly from no resonance
  // (0) to maximum resonance (15 or #F).
  //int ResonanceHex = sid->getResonance();
  uint8_t ResonanceHex = registers->resonance();

  // check if this voice has lowpass filter enabled
  bool has_lowpass_filter = has_filter && has_lowpass;

  if( has_lowpass_filter ) {
    // map those values to [0...1] float range
    float Cutoff        = 1.0 - float( map( LowHiFilterFreq, 30, 10000, 0, 1000 ) ) / 1000.0f;
    float Resonance     = float( map( ResonanceHex, 0, 15, 0, 1000 ) ) / 1000.0f;
    lowPassFilter->setCutoffFreqAndResonance( Cutoff, Resonance );
  }

  attack  = getEnveloppeRateMs( RATE_ATTACK, attack );
  decay   = getEnveloppeRateMs( RATE_DECAY, decay );
  release = getEnveloppeRateMs( RATE_RELEASE, release );

  setADSR( attack * sampleRate_ms, decay * sampleRate_ms, sustain/15.0f, release * sampleRate_ms, 1.0f, 1.0f );
  setFreqHz( fout );
  setOscType( waveform );
  setPulseWidth( pulsewidth );

  if( gated != gate ) {
    if( gate ) env->reset();
    env->gate( gate );
    gated = gate;
    samplepos = 0;
  }

  samplepos = -timeFrameRatio*(graphWidth/2);

  for( int i=0; i<graphWidth; i++ ) {
    float adsramplitude = env->process();  // ADSR amplitude value ranges 0 <-> 1
    samplepos += timeFrameRatio;
    float oscVal = getOscFloatValue( samplepos ); // Oscillator value ranges -0.5 <-> 0.5
    float output = adsramplitude*oscVal; // Oscillator * ADSR value ranges -0.5 <-> 0.5
    valuesCacheAvg += output;
    valuesCache[i] = output;
    if( has_lowpass_filter ) {
      valuesCache[i] = lowPassFilter->Process( valuesCache[i] );
    }
  }

  if( showFPS ) renderFPS();

  // scale values to viewport height and position to vmiddle
  valuesCache[0] = (valuesCache[0]*graphHeight)+graphHeight/2;

  for( int i=1; i<graphWidth; i++ ) {
    valuesCache[i] = (valuesCache[i]*graphHeight)+graphHeight/2;
    oscSprite->drawLine( i-1, valuesCache[i-1], i, valuesCache[i], fgcolor );
  }

  valuesCacheAvg /= graphWidth;
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
