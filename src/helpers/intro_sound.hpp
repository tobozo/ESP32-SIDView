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

namespace SIDView
{


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


  __attribute__((unused))
  static void soundIntro( SID6581* sid )
  {
    //SIDKeyBoardPlayer::KeyBoardPlayer( sid, 3);
    sid->sidSetVolume( 0, 15 );
    // delay(500);
    // SIDKeyBoardPlayer::changeAllInstruments<gameboystartup1>( sid );
    // SIDKeyBoardPlayer::playNote( 0, SID_PlayNotes[60], 120 ); // 1 octave = 12
    // delay(120);
    // SIDKeyBoardPlayer::changeAllInstruments<gameboystartup2>( sid );
    // SIDKeyBoardPlayer::playNote( 0, SID_PlayNotes[84], 1000 ); // 1 octave = 12
    // delay(100);
    // SIDKeyBoardPlayer::playNote( 1, SID_PlayNotes[72], 1000 ); // 1 octave = 12
    // delay(100);
    // SIDKeyBoardPlayer::playNote( 2, SID_PlayNotes[60], 1000 ); // 1 octave = 12


    int voiceNum = 0;

    for( int count=0;count<15;count++ ) {

      String error = "";

      for( voiceNum=0; voiceNum<3; voiceNum++ ) {

        uint16_t randPulseValue   = random()%4096;
        uint16_t randFreqValue    = random()%65536;
        uint8_t randAttackValue   = random()%16;
        uint8_t randDecayValue    = random()%16;
        uint8_t randSustainkValue = random()%16;
        uint8_t randReleaseValue  = random()%16;
        //uint8_t randWaveform      = random()%5;
        // switch((count+voiceNum)%5) {
        //   case 0: waveform = SID_WAVEFORM_TRIANGLE; break;
        //   case 1: waveform = SID_WAVEFORM_SAWTOOTH; break;
        //   case 2: waveform = SID_WAVEFORM_PULSE; break;
        //   case 3: waveform = SID_WAVEFORM_NOISE; break;
        //   case 4: default: waveform = SID_WAVEFORM_SILENCE;
        // }

        sid->setAttack ( voiceNum, randAttackValue );
        sid->setDecay  ( voiceNum, randDecayValue );
        sid->setSustain( voiceNum, randSustainkValue );
        sid->setRelease( voiceNum, randReleaseValue );
        sid->setPulse  ( voiceNum, randPulseValue );
        //sid->setWaveForm( voiceNum, randWaveform );
        sid->setGate( voiceNum, 1 ); // 1 (0-1)
        sid->setFrequency( voiceNum, randFreqValue );

        Serial.printf("[V#%d][F:%04x][P:%03x][A:%x][D:%x][S:%x][R:%x]\n", voiceNum, randFreqValue, randPulseValue, randAttackValue, randDecayValue, randSustainkValue, randReleaseValue );
      }

      SID_Registers_t registers = SID_Registers_t(0);
      sid->copyRegisters( &registers );

      //auto registers = sid->copyRegisters();

      for( voiceNum=0; voiceNum<3; voiceNum++ ) {

        auto voice = registers.voice(voiceNum);

        auto pulse   = sid->getPulse(voiceNum);
        auto freq    = sid->getFrequency(voiceNum);
        auto attack  = sid->getAttack(voiceNum);
        auto decay   = sid->getDecay(voiceNum);
        auto sustain = sid->getSustain(voiceNum);
        auto release = sid->getRelease(voiceNum);
        auto gate    = sid->getGate(voiceNum);

        String err = "";
        if( voice.freq()     != freq )      err += "\nFREQ     \texpected:0x"+String(freq,16)     +"\tgot:0x"+String(voice.freq(),16);
        if( voice.pulse()    != pulse )     err += "\nPULSE    \texpected:0x"+String(pulse,16)    +"\tgot:0x"+String(voice.pulse(),16);
        if( voice.gate()     != gate )      err += "\nGATE     \texpected:0x"+String(gate,16)     +"\tgot:0x"+String(voice.gate(),16);
        if( voice.attack()   != attack )    err += "\nATTACK   \texpected:0x"+String(attack,16)   +"\tgot:0x"+String(voice.attack(),16);
        if( voice.decay()    != decay )     err += "\nDECAY    \texpected:0x"+String(decay,16)    +"\tgot:0x"+String(voice.decay(),16);
        if( voice.sustain()  != sustain )   err += "\nSUSTAIN  \texpected:0x"+String(sustain,16)  +"\tgot:0x"+String(voice.sustain(),16);
        if( voice.release()  != release )   err += "\nRELEASE  \texpected:0x"+String(release,16)  +"\tgot:0x"+String(voice.release(),16);
        if( err != "" ) error += "\n\nVoice #"+String(voiceNum)+":"+err;
      }

      if( error != "" ) {
        Serial.printf("**************** Error test #%d: **************\n%s\n\n", count+1, error.c_str() );
        registers.print();
        for( int i=0; i<25; i++ ) {
          registers.data[i].print(SID_Reg_Address_t(i));
          Serial.println();
        }
        break; // stop looping
      }
    }

    //auto registers = sid->copyRegisters();
    //registers.print();


    auto registers = (SID_Registers_t*)sid->getRegisters();;
    registers->print();


    // fade out
    // for( int t=0;t<16;t++ ) {
    //   delay( 1000/16 );
    //   sid->sidSetVolume( 0, 15-t );
    //   sid->sidSetVolume( 1, 15-t );
    //   sid->sidSetVolume( 2, 15-t );
    // }
    //
    // SIDKeyBoardPlayer::stopNote( 0 );
    // SIDKeyBoardPlayer::stopNote( 1 );
    // SIDKeyBoardPlayer::stopNote( 2 );
    //

    Serial.println("WAT");

    // back to full silence
    for( voiceNum=0; voiceNum<3; voiceNum++ ) {
      sid->setAttack ( voiceNum, 0 ); // 0 (0-15)
      sid->setDecay  ( voiceNum, 0 ); // 2 (0-15)
      sid->setSustain( voiceNum, 0 ); // 15 (0-15)
      sid->setRelease( voiceNum, 0 ); // 10 (0-15)
      sid->setGate   ( voiceNum, 0 ); // 1 (0-1)
    }
    sid->soundOff();
    Serial.println("YAY");
    if( SID_xTaskHandles[0] ) vTaskDelete( SID_xTaskHandles[0] );
  }


};
