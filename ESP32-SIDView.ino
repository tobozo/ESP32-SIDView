#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"

#include <ESP32-Chimera-Core.h>
#include <M5StackUpdater.h> // https://github.com/tobozo/M5Stack-SD-Updater
#define tft M5.Lcd
#include "SDUpdaterHooks.h"

//#ifdef BOARD_HAS_PSRAM
  // OTA, archive download & unpacker
  #include "web.h"
//#endif

#include "assets.h"
#include "ADSR.h"

// hail the c64 colors !
#define C64_DARKBLUE      0x4338ccU
#define C64_LIGHTBLUE     0xC9BEFFU
#define C64_MAROON        0xA1998Bu
#define C64_MAROON_DARKER 0x797264U
#define C64_MAROON_DARK   0x938A7DU

#include <SidPlayer.h> // https://github.com/hpwit/SID6581
#define SID_CLOCK 25
#define SID_DATA 15
#define SID_LATCH 2
#define SID_CLOCK_PIN 26
SIDTunesPlayer *sidPlayer = nullptr;

#include "SIDArchiveManager.h"
#include "SIDExplorer.h"


SID_Archive C64MusicDemos = { "C64MusicDemos", "https://phpsecu.re/C64MusicDemos.tar.gz", "/DEMOS",     &MD5Config };
SID_Archive C64Musicians  = { "C64Musicians",  "https://phpsecu.re/C64Musicians.tar.gz",  "/MUSICIANS", &MD5Config };
SID_Archive C64MusicGames = { "C64MusicGames", "https://phpsecu.re/C64MusicGames.tar.gz", "/GAMES",     &MD5Config };
SID_Archive HVSC[3] = { C64MusicGames, C64MusicDemos, C64Musicians };

size_t totalsidpackages = sizeof( HVSC ) / sizeof( HVSC[0] );


//#pragma warning pop -Wwarning=unused-variable

// TODO: wifi => sid hosting site => targz => sid file
// TODO: better ADSR



static ADSR *env = new ADSR[3];

#define SONG_TIMEOUT 180 // 180s = 3mn
#define SONG_TIMEOUT_MS SONG_TIMEOUT*1000



bool playing = false;
int HEADER_HEIGHT = 41, VOICE_HEIGHT = 29, VOICE_WIDTH=64;


TFT_eSprite spriteText      = TFT_eSprite( &tft );
TFT_eSprite spriteVoice1    = TFT_eSprite( &tft );
TFT_eSprite spriteVoice2    = TFT_eSprite( &tft );
TFT_eSprite spriteVoice3    = TFT_eSprite( &tft );
TFT_eSprite spriteWaveform1 = TFT_eSprite( &tft );
TFT_eSprite spriteWaveform2 = TFT_eSprite( &tft );
TFT_eSprite spriteWaveform3 = TFT_eSprite( &tft );

TFT_eSprite spriteIconNoise    = TFT_eSprite( &tft );
TFT_eSprite spriteIconTriangle = TFT_eSprite( &tft );
TFT_eSprite spriteIconSawtooth = TFT_eSprite( &tft );
TFT_eSprite spriteIconPulse    = TFT_eSprite( &tft );

// array stored sprites
TFT_eSprite spriteVoices[3]    = { spriteVoice1, spriteVoice2, spriteVoice3 };
TFT_eSprite spriteWaveforms[3] = { spriteWaveform1, spriteWaveform2, spriteWaveform3 };


enum RateType {
  RATE_ATTACK,
  RATE_DECAY,
  RATE_RELEASE
};


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


vec2 pointCache[5];
vec2 startPoint;
vec2 attackPoint;
vec2 decayPoint;
vec2 sustainPoint;
vec2 releasePoint;


struct Oscillator {
  uint32_t freqHz; // cycles per second ( Hertz )
  uint32_t freqms; // cycles per microsecond
  uint32_t period; // in microseconds

  float invfreqms;
  bool state = false;

  Oscillator() {};

  Oscillator( uint32_t freq ) {
    setFreqHz( freq );
  };

  void setFreqHz( uint32_t hz ) {
    freqHz     = hz;
    freqms     = hz * 1000.0;
    period = 1000000 / freqHz;
    log_d("freqHz: %4d, freqms: %8d, periodms: %5d\n", freqHz, freqms, period );
  }

  float getTriangle( int32_t utime ) {
    // ex: 3951Hz (or 3.951KHz) : 1 period = 253 microseconds (or 0.253 milliseconds)
    int32_t xpos = utime%period;
    int32_t doublexpos = xpos*2;
    int32_t ypos = doublexpos%period;
    int32_t yprogress = (ypos*100)/(period);
    int32_t doubleyprogress = (doublexpos > period ? yprogress : 100-yprogress);
    return doubleyprogress / 100.0;
  }

  float getSawTooth( int32_t utime ) {
    int32_t ypos = utime%period;
    int32_t yprogress = 100 - ( (ypos*100)/(period) );
    return yprogress / 100.0;
  }

  float getSquare( int32_t utime, int PWn ) {
    // Where PWn is the 12-bit number in the Pulse Width registers.
    // A value of 0 or 4095 ($FFF) in the pulse width registers will poroduce a constant DC output,
    // while a value of 2048 ($800) will produce a square wave. [A value of 4095 will not produce a constant DC output].
    int32_t xpos = utime%period;
    int32_t ylevel = PWn/40.95; // PWn to percent
    bool state = xpos*2 > period;
    return state ? ylevel / 100.0 : 1.0 - ylevel / 100.0;
  }

  float getNoise( int32_t utime, int PWn  ) {
    int32_t xpos = utime%period;
    int32_t ylevel = random(100);
    bool state = xpos*2 > period;
    return state ? ylevel / 100.0 : 1.0 - ylevel / 100.0;
  }
};


struct OscilloView {
  Oscillator *osc;
  ADSR *env;
  TFT_eSprite *sprite;
  float ticks;
  float pan;
  uint16_t graphWidth;
  uint16_t graphHeight;
  std::uint32_t bgcolor = C64_DARKBLUE;
  std::uint32_t fgcolor = C64_LIGHTBLUE;
  std::uint32_t gridcolor = tft.color565( 128,128,128 );
  uint16_t pulsewidth = 2048; // 0-4095
  uint8_t oscType = 0;
  uint32_t tt = 0, ttprev=0;
  uint32_t value;
  uint32_t lastValue = 0;
  int32_t* valuesCache = nullptr;
  float* floatValuesCache = nullptr;
  int32_t lasty;
  ~OscilloView() { if( valuesCache != nullptr ) valuesCache=NULL; }
  OscilloView( uint16_t width, uint16_t height, uint32_t freqHz=1, uint8_t oscType=0, std::uint32_t fgcol=C64_LIGHTBLUE, std::uint32_t bgcol=C64_DARKBLUE  )  {
    osc = new Oscillator( freqHz );
    env = new ADSR;
    sprite = new TFT_eSprite( &tft );
    //sprite->setPsram(false);
    sprite->createSprite( width, height );
    graphWidth  = width; // TODO: uint8_t *buffer = new uint8_t( width+1 );
    graphHeight = height;
    bgcolor = bgcol;
    fgcolor = fgcol;
    pan = osc->period*2;
    ticks = pan/graphWidth;
    valuesCache = (int32_t*)calloc( width+1, sizeof( int32_t ) );
    floatValuesCache = (float*)calloc( width+1, sizeof( float ) );
  };

  void setADSR( uint8_t attack, uint8_t decay, float sustain, uint8_t release, float ratioA=0.05, float ratioDR=0.02 ) {

    env->reset();
    env->setAttackRate( attack ); // attackTime in ticks... // use adsr1.setAttackRate(12 * samplerate)
    env->setDecayRate( decay );
    env->setReleaseRate( release );
    env->setSustainLevel( sustain );
    env->setTargetRatioA( ratioA );
    env->setTargetRatioDR( ratioDR );

  }

  void setPulseWidth( uint16_t pw ) {
    pulsewidth = pw;
  }

  void setFreqHz( uint32_t hz ) {
    osc->setFreqHz( hz );
    pan = osc->period*2;
    ticks = pan/(graphWidth-1);
    //tt = 0;
  }

  void setOscType( uint8_t type ) {
    switch( type ) {
      case SID_WAVEFORM_TRIANGLE:
      case SID_WAVEFORM_SAWTOOTH:
      case SID_WAVEFORM_PULSE /* SID_WAVEFORM_SQUARE */: // square
      case SID_WAVEFORM_NOISE:
        oscType = type;
      break;
      case 0://SILENCE ?
      default:
        oscType = 0;
      break;
    }
  }


  float getOscFloatValue( float t ) {
    switch( oscType ) {
      case SID_WAVEFORM_TRIANGLE:
        log_v("Triangle ");
        return osc->getTriangle( t );
      break;
      case SID_WAVEFORM_SAWTOOTH:
        log_v("Sawtooth ");
        return osc->getSawTooth( t );
      break;
      case SID_WAVEFORM_PULSE /* SID_WAVEFORM_SQUARE */: // square
        log_v("Square (pulse %d) ", pulsewidth);
        return osc->getSquare( t, pulsewidth );
      break;
      case SID_WAVEFORM_NOISE:
        log_v("Noise (pulse %d) ", pulsewidth);
        return osc->getNoise( t, pulsewidth );
      break;
      case 0://SILENCE ?
      default:
        return 0;
      break;
    }
  }

  uint32_t getOscValue( float t ) {
    return getOscFloatValue( t ) * graphHeight;
  }


  void gate( uint8_t val ) {
    if( val == 1 ) {
      env->reset();
    }
    env->gate( val );
  }

  void pushPoint() {

    env->process(); // process and get enveloppe value
    float envout = env->getOutput();

    floatValuesCache[tt] = getOscFloatValue( tt*ticks )/* * envout*/;
    log_v( "%d\n", valuesCache[tt] );

    if( floatValuesCache[tt] < 0 || floatValuesCache[tt] > 1.0 ) {
      return;
    }
    valuesCache[tt]      = floatValuesCache[tt]*graphHeight;

    sprite->scroll( -1, 0 );
    //sprite->printf("%3d:%2d", int( 100*oscFloatValues[voice] ), oscValues[voice] );
    sprite->drawFastVLine( graphWidth-1, 0, graphHeight, bgcolor );
    sprite->drawPixel( graphWidth-1, graphHeight/2, gridcolor );

    if( valuesCache[tt] != lasty) {
      log_v( "%d\n", lasty );
      sprite->drawLine( graphWidth-2, lasty, graphWidth-1, valuesCache[tt], fgcolor );
    } else {
      sprite->drawPixel( graphWidth-1, valuesCache[tt], fgcolor );
    }
    lasty = valuesCache[tt];

  }

  void pushView( int32_t x, int32_t y ) {
    if( ticks == 0 ) return; // not set or division by zero
    if( tt == 0 ) {
      floatValuesCache[0] = 0.5;//getOscFloatValue( tt*ticks );
      valuesCache[0] = floatValuesCache[0]*graphHeight;
      env->gate(1);
    }
    tt++;
    tt %= graphWidth;
    pushPoint();
    sprite->pushSprite( x, y );
  }

};


OscilloView oscView1( VOICE_WIDTH, VOICE_HEIGHT );
OscilloView oscView2( VOICE_WIDTH, VOICE_HEIGHT );
OscilloView oscView3( VOICE_WIDTH, VOICE_HEIGHT );

OscilloView oscViews[3] = { oscView1, oscView2, oscView3 };


vec2 getBezierPoint( vec2* points, int numPoints, float t ) {
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


void drawSquare( TFT_eSprite &sprite, int voice ) {
  int renderWidth  = sprite.width();
  int renderHeight = sprite.height();
  int pulseWidth  = map( sid.getPulse(voice), 0, 8192, 1, renderWidth );
  int pulseHeight = map( sid.getFrequency(voice), 0, 65536, 1, renderHeight );
  int pulseYPos = renderHeight/2 - pulseHeight/2;
  int pulses = renderWidth / pulseWidth;

  int xStart, yStart, xEnd, yEnd, yHstart;

  if( pulses > 0 ) {
    bool up = false;
    for( int i=0; i<pulses; i++ ) {
      up = ! up;
      xStart  = i*pulseWidth;
      xEnd    = xStart+pulseWidth;
      yStart  = up ? pulseYPos : sprite.height() - pulseYPos;
      yEnd    = up ? yStart : (yStart-pulseHeight)+1;

      // hline
      sprite.drawFastHLine( xStart, yStart, pulseWidth, C64_LIGHTBLUE );
      // vline
      sprite.drawFastVLine( xEnd, yEnd, pulseHeight, C64_LIGHTBLUE );

    }
    // trailing space
    if( xEnd < renderWidth ) {
      xEnd--;
      yEnd = up ? sprite.height() - pulseYPos : pulseYPos;
      sprite.drawFastHLine( xEnd+1, yEnd, renderWidth-xEnd, C64_LIGHTBLUE );
    }

  }
}


void drawTriangle( TFT_eSprite &sprite, int voice ) {
  int renderWidth  = sprite.width();
  int renderHeight = sprite.height();
  int pulseWidth  = map( sid.getPulse(voice), 0, 8192, 1, renderWidth );
  int pulseHeight = map( sid.getFrequency(voice), 0, 65536, 1, renderHeight );
  int pulseYPos = renderHeight/2 - pulseHeight/2;
  int pulses = renderWidth / pulseWidth;

  int xStart=-pulseWidth/2, yStart=renderHeight-pulseYPos, xEnd, yEnd, yHstart;
  //int x1=0, y1=pulseYPos;

  if( pulses > 0 ) {
    bool up = false;
    for( int i=0; i<pulses; i++ ) {
      up = ! up;

      xEnd    = -pulseWidth/2 + pulseWidth*i;
      yEnd    = up ? renderHeight-pulseYPos : (renderHeight-pulseYPos-pulseHeight)+1;

      sprite.drawLine( xStart, yStart, xEnd, yEnd, C64_LIGHTBLUE );

      xStart = xEnd;
      yStart = yEnd;

    }

  }
}


void drawSawtooth( TFT_eSprite &sprite, int voice ) {
  int renderWidth  = sprite.width();
  int renderHeight = sprite.height();
  int vMiddle      = renderHeight / 2;
  int pulseWidth   = map( sid.getPulse(voice), 0, 8192, 1, renderWidth );
  int pulseHeight  = map( sid.getFrequency(voice), 0, 65536, 1, renderHeight );
  int pulses       = renderWidth / pulseWidth;
  int lastx        = 0;
  int lasty        = renderHeight;

  if( pulses > 0 ) {
    for( int i=0; i<pulses; i++ ) {
      int x = pulseWidth*i;
      int y1 = vMiddle - pulseHeight;
      int y2 = vMiddle + pulseHeight;
      sprite.drawLine( lastx, lasty, x, y1, C64_LIGHTBLUE );
      sprite.drawLine( x, y1, x, y2, C64_LIGHTBLUE );
      lastx = x;
      lasty = y2;
    }
  }
}


void drawNoise( TFT_eSprite &sprite, int voice ) {
  int renderWidth  = sprite.width();
  int renderHeight = sprite.height();
  int vMiddle      = renderHeight / 2;
  int pulseWidth   = map( sid.getPulse(voice), 0, 8192, 1, renderWidth );
  int pulseHeight  = map( sid.getFrequency(voice), 0, 65536, 1, renderHeight );
  int lastx = -1, lasty = -1;
  float i   = 0;
  float x   = 0, y = 0, vy;
  bool up   = false;

  if( pulseWidth > 0 ) {
    while( x < renderWidth ) {
      up = ! up;
      vy = random( pulseHeight );
      y = up ? vMiddle+vy : vMiddle-vy;
      if( lastx==-1 || lasty==-1 ) {
        sprite.drawPixel( x , y , C64_LIGHTBLUE );
      } else {
        sprite.drawLine( lastx, lasty, x, y, C64_LIGHTBLUE );
      }
      lastx = x;
      lasty = y;
      x +=random(pulseWidth) + 4.0;
    }
  }
}


void drawSilence( TFT_eSprite &sprite, int voice ) {
  int renderWidth  = sprite.width();
  int renderHeight = sprite.height();
  int vMiddle      = renderHeight / 2;
  spriteVoices[voice].drawFastHLine( 0, vMiddle, renderWidth, C64_LIGHTBLUE );
}


void drawADSR( TFT_eSprite &sprite, int voice ) {

  int attack  = sid.getAttack(voice);
  int decay   = sid.getDecay(voice);
  int sustain = sid.getSustain(voice);
  int release = sid.getRelease(voice);

  int total = attack + decay + sustain + release;
/*
  if( total == 0 ) {
    drawSilence( sprite, voice );
    return;
  }
*/
  int renderWidth   = (sprite.width()*.5) - 2;
  int renderHeight  = (sprite.height()) - 10;
  int renderLeft    = sprite.width()*.5 + 2;
  int renderBottom  = sprite.height() - 1;
  int renderHmiddle = renderBottom - renderHeight/2;
  int renderTop     = 10;

  int a = map( attack,  0, 15, 0, renderWidth/4);
  int d = map( decay,   0, 15, 0, renderHeight/2);
  int s = map( sustain, 0, 15, 0, renderWidth/4);
  int r = map( release, 0, 15, 0, renderWidth/4);

  startPoint   = { float(renderLeft),             float(renderBottom) };
  attackPoint  = { float(renderLeft + a),         float(renderTop) };
  decayPoint   = { float(renderLeft + a + d),     float(renderTop + d) };
  sustainPoint = { float(renderLeft + a + d + s), float(renderTop + d) };
  releasePoint = { float(renderLeft+renderWidth), float(renderBottom) };
  vec2 adsrPoints[5] = { startPoint, attackPoint, decayPoint, sustainPoint, releasePoint };

  drawBezierCurve( sprite, adsrPoints, 5, C64_LIGHTBLUE);

}


void drawVoice( int voice ) {
  spriteVoices[voice].fillSprite( C64_DARKBLUE );

  int attack   = sid.getAttack(voice);
  int decay    = sid.getDecay(voice);
  int sustain  = sid.getSustain(voice);
  int release  = sid.getRelease(voice);
  int pulse    = sid.getPulse(voice);
  //int coarse  = sig.getCoarse(voice) ???
  int gate     = sid.getGate(voice);
  int waveform = sid.getWaveForm(voice);
  int test     = sid.getTest(voice);
  int sync     = sid.getSync(voice);
  int ringmode = sid.getRingMode(voice);
}


int getEnveloppeRateMs( RateType type, uint8_t rate ) {

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


char lastHexEnveloppe[5] = "0000";
char hexEnveloppe[5] = "0000";

void feedEnveloppe( int voice ) {
  float attack  = sid.getAttack(voice);  // 0-15
  float decay   = sid.getDecay(voice);   // 0-15
  float sustain = sid.getSustain(voice); // 0-15
  float release = sid.getRelease(voice); // 0-15
  float fout    = sid.getFrequency(voice) * 0.00596; // Fout = (Fn * 0.0596) Hz

  float sampleRate = 1.0;//float( fps ); // based on the fps

  sprintf( hexEnveloppe, "%x%x%x%x", int(attack), int(decay), int(sustain), int(release) );

  attack  = getEnveloppeRateMs( RATE_ATTACK, attack );
  decay   = getEnveloppeRateMs( RATE_DECAY, decay );
  release = getEnveloppeRateMs( RATE_RELEASE, release );
  sustain /= 15.0;

  env[voice].gate( sid.getGate( voice ) );

  if( strcmp( lastHexEnveloppe, hexEnveloppe ) == 0 ) {
    // no enveloppe change
  } else {

    env[voice].reset();
    env[voice].setAttackRate(   attack * sampleRate );  // .1 * sampleRate (= .1 second)
    env[voice].setDecayRate(    decay * sampleRate );   // .3 * sampleRate
    env[voice].setSustainLevel( sustain );              // .8
    env[voice].setReleaseRate(  release * sampleRate ); // 5 * sampleRate
    env[voice].setTargetRatioA(0.02);
    env[voice].setTargetRatioDR(0.01);
    memcpy( lastHexEnveloppe, hexEnveloppe, 5 * sizeof( char ) );
  }
}


void drawEnveloppe( TFT_eSprite &sprite, int voice ) {
  int renderWidth   = sprite.width();
  int renderHeight  = sprite.height()-2;
  int vMiddle       = renderHeight / 2;

  float attack  = sid.getAttack(voice);  // 0-15
  float decay   = sid.getDecay(voice);   // 0-15
  float sustain = sid.getSustain(voice); // 0-15
  float release = sid.getRelease(voice); // 0-15
  float fout    = sid.getFrequencyHz(voice) * 0.00596; // Fout = (Fn * 0.0596) Hz
  sprintf( hexEnveloppe, "%x%x%x%x", int(attack), int(decay), int(sustain), int(release) );
  //Serial.printf("[%s] -> ", hexEnveloppe );

  float sampleRate = /*1000.0 /*/ float( fps );

  attack  = getEnveloppeRateMs( RATE_ATTACK, attack );
  decay   = getEnveloppeRateMs( RATE_DECAY, decay );
  release = getEnveloppeRateMs( RATE_RELEASE, release );
  sustain /= 15.0;
  env[voice].reset();
  env[voice].setAttackRate(   attack  * sampleRate );  // .1 * sampleRate (= .1 second)
  env[voice].setDecayRate(    decay   * sampleRate );   // .3 * sampleRate
  env[voice].setSustainLevel( sustain * sampleRate );              // .8
  env[voice].setReleaseRate(  release * sampleRate ); // 5 * sampleRate
  env[voice].setTargetRatioA(0.02);
  env[voice].setTargetRatioDR(0.01);

  spriteWaveforms[voice].fillSprite( C64_DARKBLUE );

  unsigned long counter = 0;
  int maxframes = renderWidth;

  if( strcmp( hexEnveloppe, "0000" ) != 0 ) {
    while( maxframes-- >= 0 ) {
      counter++;
      if( counter%fps==0 ) {
        env[voice].gate( 0 );
        Serial.println("gate(0)");
      }
      if( counter%fps==1 ) {
        env[voice].gate( 1 );
        Serial.println("gate(1)");
      }
      env[voice].process();
      float graphValue = renderHeight - env[voice].getOutput()*renderHeight;
      spriteWaveforms[voice].scroll( -1, 0 );
      Serial.printf("Scrolling #%d\n", (int)counter);
      spriteWaveforms[voice].drawPixel( renderWidth-1, graphValue, C64_LIGHTBLUE );
      spriteWaveforms[voice].pushSprite( VOICE_WIDTH, HEADER_HEIGHT + ( voice*VOICE_HEIGHT ) );
    }
  }

}




void renderVoice( int voice ) {

  spriteVoices[voice].fillSprite( C64_DARKBLUE );
  uint8_t waveform = sid.getWaveForm(voice);

  uint32_t freq    = sid.getFrequencyHz(voice);
  float attack     = sid.getAttack(voice);  // 0-15
  float decay      = sid.getDecay(voice);   // 0-15
  float sustain    = sid.getSustain(voice); // 0-15
  float release    = sid.getRelease(voice); // 0-15
  float pulsewidth = sid.getPulse(voice);
  attack  = getEnveloppeRateMs( RATE_ATTACK, attack );
  decay   = getEnveloppeRateMs( RATE_DECAY, decay );
  release = getEnveloppeRateMs( RATE_RELEASE, release );
  sustain /= 15.0;

  if( freq != oscViews[voice].osc->freqHz && freq > 0 ) {
    oscViews[voice].setFreqHz( freq );
  }
  oscViews[voice].setOscType( waveform );
  oscViews[voice].setPulseWidth( sid.getPulse(voice) );

/*
  oscViews[voice].setADSR( attack, decay, sustain, release );
  oscViews[voice].gate( sid.getGate( voice ) );
*/

  oscViews[voice].pushView( VOICE_WIDTH, (HEADER_HEIGHT-1) + ( voice*VOICE_HEIGHT ) );

  spriteVoices[voice].setCursor( 0, 1 );
  spriteVoices[voice].setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
  //spriteText.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
  //spriteText.setTextDatum( TL_DATUM );

  switch( waveform ) {
    case 0://SILENCE ?
      drawSilence( spriteVoices[voice], voice );
      spriteVoices[voice].printf("#%d", voice);
      spriteVoices[voice].drawFastHLine( 16, 4, 16, C64_LIGHTBLUE );
    break;
    case SID_WAVEFORM_TRIANGLE:
      drawTriangle( spriteVoices[voice], voice );
      spriteVoices[voice].printf("#%d", voice);
      spriteIconTriangle.pushSprite( &spriteVoices[voice], 16, 0, TFT_BLACK );

    break;
    case SID_WAVEFORM_SAWTOOTH:
      drawSawtooth( spriteVoices[voice], voice );
      spriteVoices[voice].printf("#%d", voice);
      spriteIconSawtooth.pushSprite( &spriteVoices[voice], 16, 0, TFT_BLACK );
    break;
    case SID_WAVEFORM_PULSE /* SID_WAVEFORM_SQUARE */: // square
      drawSquare( spriteVoices[voice], voice );
      spriteVoices[voice].printf("#%d", voice);
      spriteIconPulse.pushSprite( &spriteVoices[voice], 16, 0, TFT_BLACK );
    break;
    case SID_WAVEFORM_NOISE:
      drawNoise( spriteVoices[voice], voice );
      spriteVoices[voice].printf("#%d", voice);
      spriteIconNoise.pushSprite( &spriteVoices[voice], 16, 0, TFT_BLACK );
    break;
    default:
      //Serial.printf("Unhandled waveform value %d for voice #%d\n", waveform, voice );
      drawSquare( spriteVoices[voice], voice );
      spriteVoices[voice].printf("#%d ????", voice);
  }

  // drawEnveloppe( spriteVoices[voice], voice );
  // drawADSR( spriteVoices[voice], voice );
  //spriteVoices[voice].setCursor( 0, 1 );
  //spriteVoices[voice].printf("Voice #%d", voice);
  spriteVoices[voice].pushSprite( 0, HEADER_HEIGHT + ( voice*VOICE_HEIGHT ) );
  //spriteWaveforms[voice].pushSprite( VOICE_WIDTH, HEADER_HEIGHT + ( voice*VOICE_HEIGHT ) );
}



void drawVolumeIcon( int16_t x, int16_t y, uint8_t volume )
{
  tft.startWrite();
  tft.fillRect(x, y, 8, 8, C64_DARKBLUE );
  for( uint8_t i=0;i<volume;i+=2 ) {
    tft.drawLine( x+i/2, y+7, x+i/2, y+8-i/2, C64_LIGHTBLUE );
  }
  tft.endWrite();
}



static void renderVoices( void* param = NULL ) {

  SIDExplorer *mySIDExplorer = nullptr;

  char filenumStr[32]  = {0};
  char songNameStr[255] = {0};
  stoprender = false;
  int16_t textPosX = 0, textPosY = 0;
  loopmode playerloopmode = MODE_SINGLE_TRACK;
  uint8_t playerVolume = maxVolume;
  bool force_redraw = false;
  bool scrolltext = false;

  if( param != NULL ) {
    mySIDExplorer = (SIDExplorer *) param;
    playerloopmode = mySIDExplorer->playerloopmode;
  }

  spriteText.setFont( &Font8x8C64 );
  spriteText.setTextSize(1);


  //auto elapsed = sidPlayer->song_duration - sidPlayer->delta_song_duration;
  //auto progress = 100*elapsed/sidPlayer->song_duration;
  int _lasprogress = 0;
  uint8_t _lastsubsong = 0;

  initSprites();

  while(1) {
    if( stoprender ) break;
    if( render == false ) {
      vTaskDelay( 10 );
      continue;
    }

    // track progress
    auto _elapsed = /*sidPlayer->song_duration -*/ sidPlayer->delta_song_duration;
    auto _progress = (100*_elapsed) / sidPlayer->song_duration;


    if( _lastsubsong != sidPlayer->currentsong ) {
      _lastsubsong = sidPlayer->currentsong;
      // TODO: render subsong
    }

    if( _lasprogress != _progress ) {
      _lasprogress = _progress;
      if( _progress > 0 ) {
        char timeStr[12] = {0};

        tft.setTextColor( C64_DARKBLUE, C64_MAROON );

        tft.setTextDatum( TL_DATUM );
        sprintf(timeStr, "%02d:%02d",
          (_elapsed/60000)%60,
          (_elapsed/1000)%60
        );
        tft.drawString( timeStr, 0, 4 );

        tft.setTextDatum( TR_DATUM );
        sprintf(timeStr, "%02d:%02d",
          (sidPlayer->song_duration/60000)%60,
          (sidPlayer->song_duration/1000)%60
        );
        tft.drawString( timeStr, tft.width(), 4 );

        uint32_t len_light = (_progress*tft.width())/100;
        uint32_t len_dark  = tft.width()-len_light;
        tft.drawFastHLine( 0,         27, len_light, C64_MAROON );
        tft.drawFastHLine( len_light, 27, len_dark,  C64_DARKBLUE );
        tft.drawFastHLine( 0,         28, len_light, C64_MAROON_DARK );
        tft.drawFastHLine( len_light, 28, len_dark,  C64_DARKBLUE );

      } else {
        tft.drawFastHLine( 0, 27, tft.width(), C64_DARKBLUE );
      }
    }
    if( playerloopmode != mySIDExplorer->playerloopmode ) {
      force_redraw = true;
      playerloopmode = mySIDExplorer->playerloopmode;
    }
    if( playerVolume != maxVolume ) {
      force_redraw = true;
      playerVolume = maxVolume;
    }
    if( sidPlayer->getPositionInPlaylist() != positionInPlaylist ) {
      force_redraw = true;
      scrolltext = false;
    }

    if( scrolltext ) {
      scrollableText.doscroll();
    }

    if( force_redraw ) {
      force_redraw = false;

      positionInPlaylist = sidPlayer->getPositionInPlaylist();
      spriteText.fillSprite(C64_DARKBLUE);
      spriteText.setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );

      snprintf( songNameStr, 255, " %s by %s (c) %s ", sidPlayer->getName(), sidPlayer->getAuthor(), sidPlayer->getPublished() );
      sprintf( filenumStr, "#%d", positionInPlaylist );

      if( mySIDExplorer != nullptr ) {
        textPosX = 16;
        textPosY = 0;
        tft.drawJpg( (const uint8_t*)mySIDExplorer->loopmodeicon, mySIDExplorer->loopmodeicon_len, 0, 16 );
        drawVolumeIcon( 8, 16, maxVolume );
        uint16_t scrollwidth = tft.width();
        scrollwidth/=8;
        scrollwidth*=8;
        if( scrollwidth < tft.textWidth( songNameStr ) ) {
          //Serial.println("Reset scroll");
          if( !scrolltext ) {
            scrollableText.setup( songNameStr, 0, 32, scrollwidth, tft.fontHeight(), 300, true );
            scrolltext = true;
          }
        } else {
          scrolltext = false;
        }
        tft.setTextColor( C64_DARKBLUE, C64_LIGHTBLUE );
        tft.setTextDatum( TR_DATUM );
        tft.drawString( filenumStr, tft.width()-1, 16 );

      } else {
        textPosX = -3;
        textPosY = 0;
        spriteText.setTextDatum( TR_DATUM );
        spriteText.drawString( filenumStr, spriteText.width()-1, textPosY );
      }

      if( !scrolltext ) {
        spriteText.setTextDatum( TL_DATUM );
        spriteText.drawString( songNameStr, textPosX, textPosY );
      }



      spriteText.pushSprite(0,32);

      //Serial.printf("New track : %s\n", songNameStr );
    }

    for( int voice=0; voice<3; voice++ ) {
      renderVoice(voice);
    }
    if( showFPS ) renderFPS();
    vTaskDelay(10);
  }
  stoprender = false;
  log_w("Task will self-delete");
  deInitSprites();
  vTaskDelete( NULL );
}


static void PlayerButtonsTask( void * param ) {

  Wire.begin( SDA, SCL ); // connect XPadShield

  while(1) {
    #ifdef BOARD_HAS_PSRAM
      if( webServerRunning ) {
        //server.handleClient();
        ArduinoOTA.handle();
      }
    #endif

    // scan buttons from XPadShield
    Wire.requestFrom(0x10, 1);
    int resp = Wire.read();

    if( resp!=0 && ( lastresp != resp || lastpush + debounce < millis() ) ) {

      switch(resp) {
        case 0x01: // down
          Serial.println("sidPlayer->pausePlay() from button");
          sidPlayer->pausePlay();
        break;
        case 0x02: // up
          Serial.println("sidPlayer->playPrev() from button");
          sidPlayer->playPrev();
        break;
        case 0x04: // right
          Serial.println("sidPlayer->playNext() from button");
          sidPlayer->playNextSong();
        break;
        case 0x08: // left
          Serial.println("sidPlayer->stopPlayer() from button");
          sidPlayer->stopPlayer();
        break;
        case 0x10: // B
          Serial.println("sidPlayer->play() from button");
          sidPlayer->play();
        break;
        case 0x20: // A
          Serial.println("sidPlayer->playNextSongInSid() from button");
          sidPlayer->playNextSongInSid();
        break;
        case 0x40: // C
          if( maxVolume > 0 ) {
            maxVolume--;
            sidPlayer->SetMaxVolume( maxVolume ); //value between 0 and 15
            Serial.printf("New volume level: %d\n", maxVolume );
          }
        break;
        case 0x80: // D
          if( maxVolume < 15 ) {
            maxVolume++;
            sidPlayer->SetMaxVolume( maxVolume ); //value between 0 and 15
            Serial.printf("New volume level: %d\n", maxVolume );
          }
        break;
        default: // simultaneous buttons push ?
          Serial.printf("Unhandled button combination: %X\n", resp );
        break;

      }
      log_v("XPadButton response: %X", resp);
      lastresp = resp;
      lastpush = millis();
    } else if( lastEvent + SONG_TIMEOUT_MS < millis() ) {
      lastEvent = millis();
      Serial.println("sidPlayer->playNext() from timeout");
      sidPlayer->playNext();
    }

    vTaskDelay(100);
  }
}


uint8_t PlaylistChooser() {

  if( totalsidpackages == 0 ) {
    Serial.println("[ERROR] no SID packages are defined in SIDArchiveManager, halting system");
    while(1);
  }

  // no need to show a picker if there's only one element
  if(totalsidpackages == 1) return 0;

  Wire.begin( SDA, SCL ); // connect XPadShield

  PlaylistRenderer playlists[totalsidpackages];

  int enabledId = 0;

  for( uint8_t i=0; i<totalsidpackages; i++ ) {
    playlists[i].name = HVSC[i].name;
    playlists[i].index = i;
    if( i==enabledId ) {
      playlists[i].enabled = true;
    } else {
      playlists[i].enabled = false;
    }
    playlists[i].draw();
  }

  while(1) {

    // scan buttons from XPadShield
    Wire.requestFrom(0x10, 1);
    int resp = Wire.read();

    if( resp!=0 && ( lastresp != resp || lastpush + debounce < millis() ) ) {

      playlists[enabledId].enabled = false;
      playlists[enabledId].draw();

      switch(resp) {
        case 0x01: // down
        case 0x04: // right
          enabledId--;
          if( enabledId < 0 ) enabledId = totalsidpackages-1;
          playlists[enabledId].enabled = true;
          playlists[enabledId].draw();
        break;
        case 0x08: // left
        case 0x02: // up
          enabledId++;
          if( enabledId >= totalsidpackages ) enabledId = 0;
          playlists[enabledId].enabled = true;
          playlists[enabledId].draw();
        break;
        case 0x10: // B = select
          return enabledId;
        break;
        default: // simultaneous buttons push ?
          Serial.printf("Unhandled button combination: %X\n", resp );
        break;
      }

      log_v("XPadButton response: %X", resp);
      lastresp = resp;
      lastpush = millis();
    } else if( lastEvent + SONG_TIMEOUT_MS < millis() ) {
      Serial.println("picked a playlist after timeout");
      return enabledId;
    }

    vTaskDelay(100);
  }
}






void initSprites()
{
  spriteText.setPsram(false);

  spriteVoices[0].setPsram(false);
  spriteVoices[1].setPsram(false);
  spriteVoices[2].setPsram(false);
  spriteWaveforms[0].setPsram(false);
  spriteWaveforms[1].setPsram(false);
  spriteWaveforms[2].setPsram(false);

  spriteIconNoise.setPsram(false);
  spriteIconTriangle.setPsram(false);
  spriteIconSawtooth.setPsram(false);
  spriteIconPulse.setPsram(false);

  /*
  spriteWaveforms[0].setColorDepth(1);
  spriteWaveforms[1].setColorDepth(1);
  spriteWaveforms[2].setColorDepth(1);
  */

  spriteIconNoise.setColorDepth(8);
  spriteIconTriangle.setColorDepth(8);
  spriteIconSawtooth.setColorDepth(8);
  spriteIconPulse.setColorDepth(8);

  spriteIconNoise.createSprite( 16, 8 );
  spriteIconTriangle.createSprite( 16, 8 );
  spriteIconSawtooth.createSprite( 16, 8 );
  spriteIconPulse.createSprite( 16, 8 );

  spriteIconNoise.drawJpg( noise_jpg, noise_jpg_len, 0, 0 );
  spriteIconTriangle.drawJpg( triangle_jpg, triangle_jpg_len, 0, 0 );
  spriteIconSawtooth.drawJpg( sawtooth_jpg, sawtooth_jpg_len, 0, 0 );
  spriteIconPulse.drawJpg( pulse_jpg, pulse_jpg_len, 0, 0 );

  spriteText.createSprite( tft.width(), 8 );
  spriteVoices[0].createSprite( VOICE_WIDTH, VOICE_HEIGHT );
  spriteVoices[1].createSprite( VOICE_WIDTH, VOICE_HEIGHT );
  spriteVoices[2].createSprite( VOICE_WIDTH, VOICE_HEIGHT );

  spriteWaveforms[0].createSprite( (VOICE_WIDTH)-2, VOICE_HEIGHT-10 );
  spriteWaveforms[1].createSprite( (VOICE_WIDTH)-2, VOICE_HEIGHT-10 );
  spriteWaveforms[2].createSprite( (VOICE_WIDTH)-2, VOICE_HEIGHT-10 );
}

void deInitSprites()
{
  spriteText.deleteSprite();
  spriteVoices[0].deleteSprite();
  spriteVoices[1].deleteSprite();
  spriteVoices[2].deleteSprite();
  spriteWaveforms[0].deleteSprite();
  spriteWaveforms[1].deleteSprite();
  spriteWaveforms[2].deleteSprite();
  spriteIconNoise.deleteSprite();
  spriteIconTriangle.deleteSprite();
  spriteIconSawtooth.deleteSprite();
  spriteIconPulse.deleteSprite();
}



/*
class sid_myinstrument : public sid_instrument {
  public:
    int i;
    virtual void start_sample( int voice,int note ) {

        sid.setAttack(   voice, 2 ); // 0
        sid.setDecay(    voice, 0 ); // 2
        sid.setSustain(  voice, 8 ); // 15
        sid.setRelease(  voice, 2 ); // 10
        sid.setPulse(    voice, 512 ); // 1000
        sid.setWaveForm( voice, SID_WAVEFORM_PULSE ); // SID_WAVEFORM_PULSE
        sid.setGate(     voice, 1 ); // 1
        i=0;
    }
    virtual void next_instruction( int voice, int note ) {
        //log_v(note+10*(cos(2*3.14*i/100)));
        sid.setFrequency( voice, note+20*( 1+i/50 )*( cos( 2*3.14*i/10 ) ) );
        //x
        i++;
        vTaskDelay(20);
    }
    virtual void after_off( int voice, int note ) {
        sid.setFrequency( voice, note+20*( cos( 2*3.14*i/10 ) ) );
        //x
        i++;
        vTaskDelay(20);
    }
};

*/






void setup() {

    randomSeed(analogRead(0));
    M5.begin(true, true); // begin both TFT and SD
    tft.setRotation( 2 ); // portrait mode !

/*
    checkMenuStickyPartition(); // copy self to SD Card as "/menu.bin" and OTA1 partition
    setupButtonsStyle(); // set styles first (default it 320x240 landscape)
    setAssertTrigger( &myAssertStartUpdateFromButton ); // use my own buttons combination at boot
    checkSDUpdater( SID_FS, MENU_BIN, 1000 ); // suggest rollback
*/
    //M5.I2C.scan();
    Serial.printf("SID Player UI: %d*%d\n", tft.width(), tft.height() );
    //initSprites();

    delay( 1000 ); // SD Fails sometimes without this

    tft.fillScreen( C64_DARKBLUE ); // Commodore64 blueish
    tft.drawJpg( header128x32_jpg, header128x32_jpg_len, 0, 0 );

    VOICE_HEIGHT = ( tft.height() - HEADER_HEIGHT) / 3; // 29 // ( tft.height() - HEADER_HEIGHT ) / 3
    VOICE_WIDTH  = tft.width() / 2;

    SIDExplorer *mySIDExplorer = new SIDExplorer( SID_FS, HVSC, totalsidpackages );
    mySIDExplorer->explore();

    //int itemId = SIDExplorer( SID_FS, HVSC, totalsidpackages );
/*
    if( itemId >= -1 ) {
      if( myFolder[itemId].type == F_SID_FILE ) {
        // just play one song TODO: enable subsong toggle
        sidPlayer = new SIDTunesPlayer( 1 );
        sidPlayer->setEventCallback( sidCallback );
        sidPlayer->setLoopMode( MODE_LOOP_PLAYLIST_RANDOM );
        //sidPlayer->begin(SID_CLOCK,SID_DATA,SID_LATCH); //if you have an external clock circuit
        sidPlayer->begin( SID_CLOCK, SID_DATA, SID_LATCH, SID_CLOCK_PIN ); //(if you do not have a external clock cicuit)
        sidPlayer->addSong( SID_FS, myFolder[itemId].path.c_str() );
        sidPlayer->SetMaxVolume( maxVolume ); //value between 0 and 15
        sidPlayer->play(); //it will play all songs in loop

      } else if( myFolder[itemId].type == F_FOLDER ) {
        myFolder[itemId].path.c_str();
      } else {
        // duh ?
      }
    }
    */

    Serial.printf("Supported waveforms: %d, %d, %d, %d\n", SID_WAVEFORM_TRIANGLE, SID_WAVEFORM_SAWTOOTH, SID_WAVEFORM_PULSE, SID_WAVEFORM_NOISE );

    while(1);
    /*
    const char* songpath = "/sid/DEMOS/UNKNOWN/Music_Shop/Absch_L_F_A_Leut.sid";
    songstruct *songinfo = (songstruct*)ps_calloc(1, sizeof(songstruct));

    if( !getInfoFromSIDFile(SID_FS, songpath, songinfo) ) {
      Serial.println("FAILED");
    } else {
      Serial.println("OK");
      songinfo->durations = (uint32_t*)sid_calloc( 1, songinfo->subsongs*sizeof(uint32_t) );
      songdebug( songinfo );
    }

    while(1);
    */

/*
    //const char* songpath = "/sid/GAMES/A-F/Caveman_Ugh-Lympics.sid";
    const char* songpath = "/DEMOS/UNKNOWN/Music_Baby_tune_2.sid";
    songstruct *songinfo = (songstruct*)ps_calloc(1, sizeof(songstruct));

    if( !getInfoFromSIDFile(SID_FS, songpath, songinfo) ) {
      Serial.println("FAILED");
    } else {
      uint8_t randomwidth = songinfo->subsongs;
      Serial.println("OK");
      songinfo->durations = (uint32_t*)sid_calloc( 1, songinfo->subsongs*sizeof(uint32_t) );
      songdebug( songinfo );

      SongCache->init( songpath );
      SID_FS.remove( SongCache->cachepath );

      sprintf( (char*)songinfo->name, "%s", "BLAH" );
      songinfo->subsongs = rand()%randomwidth;
      SongCache->add( SID_FS, songpath, songinfo ); // create

      sprintf( (char*)songinfo->name, "%s", "BLEB" );
      songinfo->subsongs = rand()%randomwidth;
      SongCache->add( SID_FS, songpath, songinfo ); // insert

      sprintf( (char*)songinfo->name, "%s", "Caveman Ugh-Lympics" );
      songinfo->subsongs = randomwidth;
      SongCache->add( SID_FS, songpath, songinfo ); // insert

      sprintf( (char*)songinfo->name, "%s", "PLOP" );
      songinfo->subsongs = rand()%randomwidth;
      SongCache->add( SID_FS, songpath, songinfo ); // insert


      sprintf( (char*)songinfo->name, "%s", "Caveman Ugh-Lympics" );
      SongCache->add( SID_FS, songpath, songinfo ); // cache hit

      sprintf( (char*)songinfo->name, "%s", "BLEB" );
      SongCache->add( SID_FS, songpath, songinfo ); // cache hit

      sprintf( (char*)songinfo->name, "%s", "BLAH" );
      SongCache->add( SID_FS, songpath, songinfo ); // cache hit

      sprintf( (char*)songinfo->name, "%s", "PLOP" );
      SongCache->add( SID_FS, songpath, songinfo ); // cache hit


      SID_FS.remove( SongCache->cachepath ); // cleanup
    }

    while(1);
*/

    //createCacheFromMd5( SID_FS, SID_FOLDER, MD5_FILE ); // with durations (slower)
    //createCacheFromFolder( SID_FS, SID_FOLDER "/GAMES", ".sid", true ); // without durations

    while(1);

    //SongCache->init("/sid/MUSICIANS/X/Xonic_the_Fox/Ohne_Dich_Rammstein.sid");

/*
    #ifdef BOARD_HAS_PSRAM
*/
      /*
      // ENABLE OTA
      OTASetup();
      Serial.println("OTA READY");
      webServerRunning = true; // TODO: add means to start/stop WiFi+WebServer

      tft.setCursor(0, 16);
      tft.setTextSize(1);
      tft.print("http://esp32-sid6581.local");
      */

      // first pass, download all things
      for( int i=0; i<totalsidpackages; i++ ) {
        if( !HVSC[i].exists() ) {
          if( !HVSC[i].download() ) {
            Serial.printf("Downloading of %s failed, deleting damaged or empty file\n", HVSC[i].tgzFileName );
            SID_FS.remove( HVSC[i].tgzFileName );
          }
        }
      }

      // also download the MD5 checksums file containing song lengths
      if( ! SID_FS.exists( MD5_FILE ) ) {
        if( !wifiConnected ) stubbornConnect();
        if( !wget( MD5_URL, SID_FS, MD5_FILE ) ) {
          Serial.printf("Failed to download Songlengths.full.md5 :-(");
        } else {
          Serial.printf("Downloaded song lengths successfully");
        }
      }

      // now turn off wifi as it ate more than 80Kb ram
      if( wifiConnected ) wifiOff();

      // expand archives
      for( int i=0; i<totalsidpackages; i++ ) {
        if( HVSC[i].exists() ) {
          if( !HVSC[i].isExpanded() ) {
            if( !HVSC[i].expand() ) {
              Serial.printf("Expansion of %s failed, please delete the %s folder and %s archive manually\n", HVSC[i].name, HVSC[i].sidRealPath, HVSC[i].tgzFileName );
            }
          } else {
            Serial.printf("Archive %s looks healthy\n", HVSC[i].name );
          }
        } else {
          Serial.printf("Archive %s is missing\n", HVSC[i].name );
        }
      }

      // create cache files
      for( int i=0; i<totalsidpackages; i++ ) {
        if( HVSC[i].exists() && HVSC[i].isExpanded() && !HVSC[i].hasCache() ) {
          Serial.printf("Cache file %s is missing, will rebuild\n", HVSC[i].cachePath );
          if( ! HVSC[i].createAllCacheFiles( 4096 ) ) {
            SID_FS.remove( HVSC[i].cachePath );
            Serial.println("Halted");
            while(1);
          }
        }
      }

      tft.setBrightness( 32 );


      uint8_t chosenPlaylist = PlaylistChooser();


      tft.fillScreen( C64_DARKBLUE ); // Commodore64 blueish
      tft.drawJpg( header128x32_jpg, header128x32_jpg_len, 0, 0 );

      //tft.setBrightness( 64 );
      sidPlayer = new SIDTunesPlayer( 4096 );
      sidPlayer->setEventCallback( sidCallback );
      sidPlayer->setLoopMode( MODE_LOOP_PLAYLIST_RANDOM );
      //sidPlayer->begin(SID_CLOCK,SID_DATA,SID_LATCH); //if you have an external clock circuit
      sidPlayer->begin( SID_CLOCK, SID_DATA, SID_LATCH, SID_CLOCK_PIN ); //(if you do not have a external clock cicuit)
      if( !SongCache->loadCache( sidPlayer, HVSC[chosenPlaylist].cachePath ) ) {
        sidPlayer->addSongsFromFolder( SID_FS, "/sid", ".sid", true );
      } else {
        // custom player
      }

      /*
    #else

      tft.setBrightness( 32 );

      tft.fillScreen( TFT_BLUE );
      tft.drawJpg( header_jpg, header_jpg_len, 0, 0 );

      //tft.setBrightness( 64 );
      sidPlayer = new SIDTunesPlayer();
      sidPlayer->setEventCallback( sidCallback );
      sidPlayer->setLoopMode( MODE_LOOP_PLAYLIST_RANDOM );
      //sidPlayer->begin(SID_CLOCK,SID_DATA,SID_LATCH); //if you have an external clock circuit
      sidPlayer->begin( SID_CLOCK, SID_DATA, SID_LATCH, SID_CLOCK_PIN ); //(if you do not have a external clock cicuit)
      sidPlayer->addSongsFromFolder( SID_FS, "/sid", ".sid", true );
      //sidPlayer->getSongslengthfromMd5( SD,"/md5/soundlength.md5" );

    #endif
*/



    sidPlayer->SetMaxVolume( maxVolume ); //value between 0 and 15

    Serial.printf( "[Songs in playlist:%d (ignored:%d)][Free heap: %d]\n", sidPlayer->numberOfSongs, sidPlayer->ignoredSongs, ESP.getFreeHeap() );

    sidPlayer->play(); //it will play all songs in loop

    xTaskCreatePinnedToCore( PlayerButtonsTask, "PlayerButtonsTask", 4096, NULL, 1, NULL, SID_PLAYER_CORE ); // will trigger SD reads
    xTaskCreatePinnedToCore( renderVoices, "renderVoices", 8192, NULL, 1, NULL, SID_CPU_CORE ); // will trigger TFT writes

}

void loop() {
  vTaskDelete( NULL );
}

#pragma GCC diagnostic pop
