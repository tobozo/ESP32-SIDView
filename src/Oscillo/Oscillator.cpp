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


#include "Oscillator.hpp"


Oscillator::Oscillator( uint32_t freq )
{
  setFreqHz( freq );
};

void Oscillator::setFreqHz( uint32_t hz )
{
  freqHz     = hz;
  freqms     = hz * 1000.0f;
  if( hz == 0 ) return;
  period_us = 1000000 / freqHz;
  log_v("freqHz: %4d, freqms: %8d, period_us: %5d", freqHz, freqms, period_us );
}

float Oscillator::getTriangle( int32_t utime )
{
  // ex: 3951Hz (or 3.951KHz) : 1 period_us = 253 microseconds (or 0.253 milliseconds)
  int32_t xpos       = utime < 0  ? period_us + ((utime+1)%-period_us) : utime%period_us;
  int32_t doublexpos = xpos*2;
  int32_t ypos       = doublexpos%period_us;
  int32_t yprogress  = (ypos*50)/(period_us);
  int32_t doubleyprogress = (doublexpos > period_us ? yprogress : 50-yprogress);
  return 0.5 - (doubleyprogress / 50.0f);
}

float Oscillator::getSawTooth( int32_t utime )
{
  int32_t ypos       = utime < 0  ? period_us + ((utime+1)%-period_us) : utime%period_us;
  float yprogress    = 50.0f - ( (ypos*50.0f)/(period_us) );
  return 0.5f - (yprogress / 50.0f);
}

float Oscillator::getSquare( int32_t utime, int PWn )
{
  // Where PWn is the 12-bit number in the Pulse Width registers.
  // Range from 0 to 4095 ($FFF) in the C64 pulse width registers
  // A value of 2048 ($800) will produce a square wave
  // A value of 4095 will not produce a constant DC output
  int32_t xpos       = utime < 0  ? period_us + ((utime+1)%-period_us) : utime%period_us;
  float pwn          = (PWn/4095.0f) - 0.5f; // PWn to per one range -0.5 <-> 0.5
  float xlow         = period_us/2 + period_us*pwn;
  bool state         = xpos > xlow;
  return (state ? 0.5f : -0.5f);
}

float Oscillator::getNoise( int32_t utime, int PWn  )
{
  int32_t xpos       = utime < 0  ? period_us + ((utime+1)%-period_us) : utime%period_us;
  float pwn          = (PWn/4095.0f) - 0.5f; // PWn to per one range -0.5 <-> 0.5
  float xlow         = period_us/2 +  period_us*pwn;
  int32_t ylevel     = rand()%50;
  bool state         = xpos > xlow;
  return (state ? ylevel/100.0f : -ylevel/100.0f);
}
