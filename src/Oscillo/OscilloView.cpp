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
    oscSprite->setPsram( false );
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
  //showUnits = 0;
  float wfunits = 0.0;

  if( oscType & SID_WAVEFORM_TRIANGLE ) {
    float triangle = osc->getTriangle( t );
    //if( triangle <-0.5f || triangle > 0.5f ) log_e("triangle overflow: %.2f", triangle );
    ret += triangle;
    wfunits++;
  }
  if( oscType & SID_WAVEFORM_SAWTOOTH ) {
    float sawtooth = osc->getSawTooth( t );
    ret += sawtooth;
    //if( sawtooth <-0.5f || sawtooth > 0.5f ) log_e("sawtooth overflow: %.2f", sawtooth );
    wfunits++;
  }
  if( oscType & SID_WAVEFORM_PULSE ) {
    float pulse = osc->getSquare( t, pulsewidth );
    ret += pulse;
    //if( pulse <-0.5f || pulse > 0.5f ) log_e("pulse overflow: %.2f", pulse );
    wfunits++;
  }
  if( oscType & SID_WAVEFORM_NOISE ) {
    float noise = osc->getNoise( t, pulsewidth );
    ret += noise;
    //if( noise <-0.5f || noise > 0.5f ) log_e("noise overflow: %.2f", noise );
    wfunits++;
  }
  if( wfunits > 1 ) {
    ret /= wfunits;
  }
  //showUnits = wfunits;

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
  info.load( registers, voice ); // process all SID registers

  if( info.has_lowpass_filter ) {
    lowPassFilter->setCutoffFreqAndResonance( info.Cutoff, info.Resonance );
  }

  setADSR( info.attack * sampleRate_ms, info.decay * sampleRate_ms, info.sustain, info.release * sampleRate_ms, 1.0f, 1.0f );
  setFreqHz( info.fout ); // * 0.00596; // Fout = (Fn * 0.0596) Hz
  setOscType( info.waveform );
  setPulseWidth( info.pulsewidth );

  if( gated != info.gate ) {
    if( info.gate ) env->reset();
    env->gate( info.gate );
    gated = info.gate;
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
    if( info.has_lowpass_filter ) {
      valuesCache[i] = lowPassFilter->Process( valuesCache[i] );
    }
  }

  if( showFPS ) renderFPS();
  if( showWaveform ) renderWaveform();

  // scale values to viewport height and position to vmiddle
  valuesCache[0] = (valuesCache[0]*(graphHeight-2.0f))+(graphHeight/2)+1.0f;

  for( int i=1; i<graphWidth; i++ ) {
    valuesCache[i] = (valuesCache[i]*(graphHeight-2.0f))+(graphHeight/2.0f)+1.0f;
    if( !showWaveform || (showWaveform && i>16) )
      oscSprite->drawLine( i-1, valuesCache[i-1], i, valuesCache[i], fgcolor );
  }

  valuesCacheAvg /= graphWidth;
}



lgfx::pixelcopy_t triangle_pixels(triangle_bits, lgfx::grayscale_1bit, lgfx::grayscale_1bit);
lgfx::pixelcopy_t sawtooth_pixels(sawtooth_bits, lgfx::grayscale_1bit, lgfx::grayscale_1bit);
lgfx::pixelcopy_t pulse_pixels(pulse_bits, lgfx::grayscale_1bit, lgfx::grayscale_1bit);
lgfx::pixelcopy_t noise_pixels(noise_bits, lgfx::grayscale_1bit, lgfx::grayscale_1bit);
lgfx::pixelcopy_t filter_pixels(filter_bits, lgfx::grayscale_1bit, lgfx::grayscale_1bit);

lgfx::pixelcopy_t filter_freq_pixels(filter_freq_bits, lgfx::grayscale_1bit, lgfx::grayscale_1bit);
lgfx::pixelcopy_t filter_lp_pixels(filter_lp_bits, lgfx::grayscale_1bit, lgfx::grayscale_1bit);
lgfx::pixelcopy_t filter_hp_pixels(filter_hp_bits, lgfx::grayscale_1bit, lgfx::grayscale_1bit);
lgfx::pixelcopy_t filter_bp_pixels(filter_bp_bits, lgfx::grayscale_1bit, lgfx::grayscale_1bit);



void OscilloView::drawIcon( lgfx::pixelcopy_t *icon_data, uint32_t xoffset, uint32_t yoffset, uint32_t width, uint32_t height )
{
  oscSprite->pushImage( xoffset, yoffset, width, height, icon_data );
}


void OscilloView::renderWaveform()
{
  showUnits = 0;

  if( oscType & SID_WAVEFORM_TRIANGLE ) {
    drawIcon( &triangle_pixels, 16*showUnits, 8 );
    showUnits++;
  }
  if( oscType & SID_WAVEFORM_SAWTOOTH ) {
    drawIcon( &sawtooth_pixels, 16*showUnits, 8 );
    showUnits++;
  }
  if( oscType & SID_WAVEFORM_PULSE ) {
    drawIcon( &pulse_pixels, 16*showUnits, 8 );
    showUnits++;
  }
  if( oscType & SID_WAVEFORM_NOISE ) {
    drawIcon( &noise_pixels, 16*showUnits, 8 );
    showUnits++;
  }

  if( showUnits == 0 ) {
    if( oscType > 0 ) { // unsupported OSC type, render value as text
      char text[5] = {0};
      snprintf( text, 12, "0x%02d", oscType );
      oscSprite->drawString( text, 0, 8 );
    }
  }

  if( info.has_lowpass_filter ) drawIcon( &filter_lp_pixels,  0, 16, 8, 8 );
  if( info.has_highpass ) drawIcon( &filter_hp_pixels,  8, 16, 8, 8 );
  if( info.has_bandpass ) drawIcon( &filter_bp_pixels, 16, 16, 8, 8 );
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
  snprintf( text, 12, "%3dfps", fps );
  oscSprite->drawString( text, 0,0 );

}


int OscilloInfo::getEnveloppeRateMs( rateType_t type, uint8_t rate )
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
