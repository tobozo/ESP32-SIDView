
#include <ESP32-Chimera-Core.h>
//#include <M5Stack.h>
#define tft M5.Lcd

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "web.h"
#include "assets.h"
#include "ADSR.h"

#include "SidPlayer.h"
#define SID_CLOCK 25
#define SID_DATA 15
#define SID_LATCH 2
#define SID_CLOCK_PIN 26

SIDTunesPlayer * sidPlayer;
ADSR *env = new ADSR[3];

#define SONG_TIMEOUT 180 // 180s = 3mn
#define SONG_TIMEOUT_MS SONG_TIMEOUT*1000
unsigned long lastEvent = millis();

char fileName[80];
int positionInPlaylist = -1;
int lastresp = -1;
bool playing = false;
unsigned long lastpush = millis();
int debounce = 300;
int HEADER_HEIGHT = 41, VOICE_HEIGHT = 29, VOICE_WIDTH=64;

TFT_eSprite spriteText      = TFT_eSprite( &tft );
TFT_eSprite spriteVoice1    = TFT_eSprite( &tft );
TFT_eSprite spriteVoice2    = TFT_eSprite( &tft );
TFT_eSprite spriteVoice3    = TFT_eSprite( &tft );
TFT_eSprite spriteWaveform1 = TFT_eSprite( &tft );
TFT_eSprite spriteWaveform2 = TFT_eSprite( &tft );
TFT_eSprite spriteWaveform3 = TFT_eSprite( &tft );
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
  uint16_t bgcolor = TFT_BLACK;
  uint16_t fgcolor = TFT_WHITE;
  uint16_t gridcolor = tft.color565( 128,128,128 );
  uint16_t pulsewidth = 2048; // 0-4095
  uint8_t oscType = 0;
  uint32_t tt = 0, ttprev=0;
  uint32_t value;
  uint32_t lastValue = 0;
  int32_t* valuesCache = nullptr;
  float* floatValuesCache = nullptr;
  int32_t lasty;
  ~OscilloView() { if( valuesCache != nullptr ) valuesCache=NULL; }
  OscilloView( uint16_t width, uint16_t height, uint32_t freqHz=1, uint8_t oscType=0, uint16_t fgcol=TFT_WHITE, uint16_t bgcol=TFT_BLACK  )  {
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
      case SID_WAVEFORM_SQUARE /* SID_WAVEFORM_SQUARE */: // square
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
      case SID_WAVEFORM_SQUARE /* SID_WAVEFORM_SQUARE */: // square
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
      log_v( lasty );
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


void drawBezierCurve( TFT_eSprite &sprite, vec2* points, int numPoints, uint16_t color ) {
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
bool showFPS = true;

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
    tft.setTextColor( TFT_BLACK, TFT_WHITE );
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
      sprite.drawFastHLine( xStart, yStart, pulseWidth, TFT_WHITE );
      // vline
      sprite.drawFastVLine( xEnd, yEnd, pulseHeight, TFT_WHITE );

    }
    // trailing space
    if( xEnd < renderWidth ) {
      xEnd--;
      yEnd = up ? sprite.height() - pulseYPos : pulseYPos;
      sprite.drawFastHLine( xEnd+1, yEnd, renderWidth-xEnd, TFT_WHITE );
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

      sprite.drawLine( xStart, yStart, xEnd, yEnd, TFT_WHITE );

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
      sprite.drawLine( lastx, lasty, x, y1, TFT_WHITE );
      sprite.drawLine( x, y1, x, y2, TFT_WHITE );
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
        sprite.drawPixel( x , y , TFT_WHITE );
      } else {
        sprite.drawLine( lastx, lasty, x, y, TFT_WHITE );
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
  spriteVoices[voice].drawFastHLine( 0, vMiddle, renderWidth, TFT_WHITE );
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

  drawBezierCurve( sprite, adsrPoints, 5, TFT_WHITE);

}


void drawVoice( int voice ) {
  spriteVoices[voice].fillSprite( TFT_BLACK );

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

  spriteWaveforms[voice].fillSprite( TFT_BLACK );

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
      Serial.printf("Scrolling #%d\n", counter);
      spriteWaveforms[voice].drawPixel( renderWidth-1, graphValue, TFT_WHITE );
      spriteWaveforms[voice].pushSprite( VOICE_WIDTH, HEADER_HEIGHT + ( voice*VOICE_HEIGHT ) );
    }
  }

}




void renderVoice( int voice ) {

  spriteVoices[voice].fillSprite( TFT_BLACK );
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
  spriteText.setTextColor( TFT_WHITE, TFT_BLACK );

  switch( waveform ) {
    case 0://SILENCE ?
      drawSilence( spriteVoices[voice], voice );
      spriteVoices[voice].printf("#%d Silence", voice);
    break;
    case SID_WAVEFORM_TRIANGLE:
      drawTriangle( spriteVoices[voice], voice );
      spriteVoices[voice].printf("#%d Triangle", voice);
    break;
    case SID_WAVEFORM_SAWTOOTH:
      drawSawtooth( spriteVoices[voice], voice );
      spriteVoices[voice].printf("#%d Sawtooth", voice);
    break;
    case SID_WAVEFORM_SQUARE /* SID_WAVEFORM_SQUARE */: // square
      drawSquare( spriteVoices[voice], voice );
      spriteVoices[voice].printf("#%d Square", voice);
    break;
    case SID_WAVEFORM_NOISE:
      drawNoise( spriteVoices[voice], voice );
      spriteVoices[voice].printf("#%d Noise", voice);
    break;
    default:
      Serial.printf("Unhandled waveform value %d for voice #%d\n", waveform, voice );
  }

  // drawEnveloppe( spriteVoices[voice], voice );
  // drawADSR( spriteVoices[voice], voice );
  //spriteVoices[voice].setCursor( 0, 1 );
  //spriteVoices[voice].printf("Voice #%d", voice);
  spriteVoices[voice].pushSprite( 0, HEADER_HEIGHT + ( voice*VOICE_HEIGHT ) );
  //spriteWaveforms[voice].pushSprite( VOICE_WIDTH, HEADER_HEIGHT + ( voice*VOICE_HEIGHT ) );
}



static void renderVoices( void* param ) {
  while(1) {
    if( sidPlayer->getPositionInPlaylist() != positionInPlaylist ) {
      positionInPlaylist = sidPlayer->getPositionInPlaylist();
      spriteText.fillSprite(TFT_BLACK);
      spriteText.setCursor(100, 0);
      spriteText.setTextColor( TFT_BLACK, TFT_WHITE );
      spriteText.printf("#%d", positionInPlaylist );

      sprintf( fileName, "%s", sidPlayer->getName() );
      spriteText.setCursor(0, 0);
      spriteText.print( fileName );
      spriteText.pushSprite(0,32);
      Serial.printf("New track : %s\n", fileName );
    }

    for( int voice=0; voice<3; voice++ ) {
      renderVoice(voice);
    }
    if( showFPS ) renderFPS();
    vTaskDelay(10);
  }
}



void sidAddFolder( fs::FS &fs, const char* foldername, const char* filetype=".sid", bool recursive=false ) {
  File root = fs.open( foldername );
  if(!root){
    Serial.printf("[ERROR] Failed to open %s directory\n", foldername);
    return;
  }
  if(!root.isDirectory()){
    Serial.printf("[ERROR] %s is not a directory\b", foldername);
    return;
  }
  File file = root.openNextFile();
  while(file){
    if( file.isDirectory() || !String( file.name() ).endsWith( filetype ) ) {
      if( recursive ) {
        sidAddFolder( fs, file.name(), filetype, true );
      } else {
        Serial.printf("[INFO] Ignored [%s]\n", file.name() );
      }
    } else {
      sidPlayer->addSong(fs, file.name() );
      Serial.printf("[INFO] Added [%s] ( %d bytes )\n", file.name(), file.size() );
    }
    file = root.openNextFile();
  }
}



static void myLoop( void * param ) {
  while(1) {
    if( webServerRunning ) {
      //server.handleClient();
      ArduinoOTA.handle();
    }

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
          sidPlayer->playNext();
        break;
        case 0x08: // leftheader_jpg
          Serial.println("sidPlayer->stopPlayer() from button");
          sidPlayer->stopPlayer();
        break;
        case 0x10: // B
          Serial.println("sidPlayer->play() from button");
          sidPlayer->play();
        case 0x20: // A
          Serial.println("sidPlayer->playNextSongInSid() from button");
          sidPlayer->playNextSongInSid();
        case 0x40: // C
        case 0x80: // D
        default: // simultaneous buttons push
        break;

      }
      Serial.println(resp, HEX);
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


void sidCallback(  sidEvent event ) {
  lastEvent = millis();

  switch( event ) {
    case SID_NEW_TRACK:
      Serial.printf( "[EVENT] New track: %s\n", sidPlayer->getFilename() );
    break;
    case SID_START_PLAY:
      Serial.printf( "[EVENT] Start play: %s\n", sidPlayer->getFilename() );
    break;
    case SID_END_PLAY:
      Serial.printf( "[EVENT] Stopping play: %s\n", sidPlayer->getFilename() );
    break;
    case SID_PAUSE_PLAY:
      Serial.printf( "[EVENT] Pausing play: %s\n", sidPlayer->getFilename() );
    break;
    case SID_RESUME_PLAY:
      Serial.printf( "[EVENT] Resume play: %s\n", sidPlayer->getFilename() );
    break;
    case SID_END_SONG:
      Serial.println("[EVENT] End of track");
    break;
  }
}



void setup() {
    // put your setup code here, to run once:
    randomSeed(analogRead(0));
    M5.begin();
    tft.setRotation( 2 );
    Wire.begin( SDA, SCL );
    M5.I2C.scan();
    SD.begin();

    Serial.printf("Init display %d*%d\n", tft.width(), tft.height() );

    VOICE_HEIGHT = (128/*tft.height()*/ - HEADER_HEIGHT) /3; // 29 // ( tft.height() - HEADER_HEIGHT ) / 3
    VOICE_WIDTH = 64; // tft.width() / 2

    tft.setBrightness( 2 ); // avoid brownout

    /*
    stubbornConnect();
    tft.setCursor(0, 0);
    tft.print( WiFi.localIP() );
    tft.setCursor(0, 16);
    tft.setTextSize(1);
    tft.print("http://esp32-sid6581.local");
    */

    Serial.printf("Supported waveforms: %d, %d, %d, %d\n", SID_WAVEFORM_TRIANGLE, SID_WAVEFORM_SAWTOOTH, SID_WAVEFORM_SQUARE, SID_WAVEFORM_NOISE );

    tft.setBrightness( 255 );

    tft.fillScreen( TFT_BLUE );
    tft.drawJpg( header_jpg, header_jpg_len, 0, 0 );

    //tft.setBrightness( 64 );

    sidPlayer = new SIDTunesPlayer();
    sidPlayer->setEventCallback( sidCallback );
    sidPlayer->begin(SID_CLOCK,SID_DATA,SID_LATCH); //if you have an external clock circuit
    //sidPlayer->begin( SID_CLOCK, SID_DATA, SID_LATCH, SID_CLOCK_PIN ); //(if you do not have a external clock cicuit)

    sidAddFolder( SD, "/sid", ".sid", true );

    sidPlayer->SetMaxVolume(7); //value between 0 and 15

    sidPlayer->play(); //it will play all songs in loop

    spriteText.setPsram(false);
    spriteVoices[0].setPsram(false);
    spriteVoices[1].setPsram(false);
    spriteVoices[2].setPsram(false);
    spriteWaveforms[0].setPsram(false);
    spriteWaveforms[1].setPsram(false);
    spriteWaveforms[2].setPsram(false);
    /*
    spriteWaveforms[0].setColorDepth(1);
    spriteWaveforms[1].setColorDepth(1);
    spriteWaveforms[2].setColorDepth(1);
    */
    spriteText.createSprite( 128, 8 );
    spriteVoices[0].createSprite( VOICE_WIDTH, VOICE_HEIGHT );
    spriteVoices[1].createSprite( VOICE_WIDTH, VOICE_HEIGHT );
    spriteVoices[2].createSprite( VOICE_WIDTH, VOICE_HEIGHT );

    spriteWaveforms[0].createSprite( (VOICE_WIDTH)-2, VOICE_HEIGHT-10 );
    spriteWaveforms[1].createSprite( (VOICE_WIDTH)-2, VOICE_HEIGHT-10 );
    spriteWaveforms[2].createSprite( (VOICE_WIDTH)-2, VOICE_HEIGHT-10 );

    xTaskCreatePinnedToCore( myLoop, "myLoop", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore( renderVoices, "renderVoices", 4096, NULL, 1, NULL, 1);

}

void loop() {
  vTaskDelete( NULL );
}
