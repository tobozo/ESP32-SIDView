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

#include "../config.h"

#include "../ADSR/ADSR.hpp"


class Oscillator
{

  public:

    Oscillator() {};
    Oscillator( uint32_t freq );

    uint32_t freqHz; // cycles per second ( Hertz )
    uint32_t freqms; // cycles per microsecond
    int32_t  period_us; // in microseconds

    void  setFreqHz( uint32_t hz );
    float getTriangle( int32_t utime );
    float getSawTooth( int32_t utime );
    float getSquare( int32_t utime, int PWn );
    float getNoise( int32_t utime, int PWn  );

  private:

    float invfreqms;
    bool state = false;

};
