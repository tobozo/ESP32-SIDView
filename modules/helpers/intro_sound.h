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
#ifndef _SID_INTRO_SOUND_H_
#define _SID_INTRO_SOUND_H_

#define SID_PLAYER
#define SID_INSTRUMENTS
#include "SID6581.h" // https://github.com/hpwit/SID6581
//#include "modules/Instruments/SidInstruments.h" // https://github.com/hpwit/SID6581
//extern SIDTunesPlayer *sidPlayer;


class gameboystartup1 : public sid_instrument
{
  public:
    int i;
    virtual void start_sample(int voice,int note)
    {
        sid->setAttack(    voice, 2 ); // 0 (0-15)
        sid->setDecay(     voice, 0 ); // 2 (0-15)
        sid->setSustain(   voice, 15 ); // 15 (0-15)
        sid->setRelease(   voice, 1 ); // 10 (0-15)
        sid->setPulse(     voice, 2048 ); // 1000 ( 0 - 4096 )
        sid->setWaveForm(  voice, SID_WAVEFORM_PULSE ); // SID_WAVEFORM_PULSE (TRIANGLE/SAWTOOTH/PULSE)
        sid->setGate(      voice, 1 ); // 1 (0-1)
        sid->setFrequency( voice, note); // note 0-95
        //i=0;
    }
};


class gameboystartup2 : public sid_instrument
{
  public:
    int i;
    virtual void start_sample(int voice,int note)
    {
        sid->setAttack(    voice, 2 ); // 0 (0-15)
        sid->setDecay(     voice, 0 ); // 2 (0-15)
        sid->setSustain(   voice, 7 ); // 15 (0-15)
        sid->setRelease(   voice, 1 ); // 10 (0-15)
        sid->setPulse(     voice, 2048 ); // 1000 ( 0 - 4096 )
        sid->setWaveForm(  voice, SID_WAVEFORM_PULSE ); // SID_WAVEFORM_PULSE (TRIANGLE/SAWTOOTH/PULSE)
        sid->setGate(      voice, 1 ); // 1 (0-1)
        sid->setFrequency( voice, note); // note 0-95
        //i=0;
    }
};



void soundIntro( SID6581* sid )
{
  SIDKeyBoardPlayer::KeyBoardPlayer( sid, 1);
  sid->sidSetVolume( 0, 15 );
  delay(500);
  SIDKeyBoardPlayer::changeAllInstruments<gameboystartup1>( sid );
  SIDKeyBoardPlayer::playNote( 0, SID_PlayNotes[60], 120 ); // 1 octave = 12
  delay(120);
  SIDKeyBoardPlayer::changeAllInstruments<gameboystartup2>( sid );
  SIDKeyBoardPlayer::playNote( 0, SID_PlayNotes[84], 1000 ); // 1 octave = 12
  // fade out
  for( int t=0;t<16;t++ ) {
    delay( 1000/16 );
    sid->sidSetVolume( 0, 15-t );
  }
  SIDKeyBoardPlayer::stopNote( 0 );
  // back to full silence
  sid->setAttack(  0, 0 ); // 0 (0-15)
  sid->setDecay(   0, 0 ); // 2 (0-15)
  sid->setSustain( 0, 0 ); // 15 (0-15)
  sid->setRelease( 0, 0 ); // 10 (0-15)
  sid->setGate(    0, 0 ); // 1 (0-1)
  vTaskDelete( SID_xTaskHandles[0] );
  sid->resetsid();
}


#endif
