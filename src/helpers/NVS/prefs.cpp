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

#include "prefs.hpp"

Preferences prefs;

loopmode NVSPrefs::getLoopMode()
{
  prefs.begin("SIDExplorer");
  uint8_t playerloopmode = (loopmode)prefs.getUChar("loopmode", (uint8_t)SID_LOOP_OFF );
  prefs.end();
  switch( playerloopmode ) { // validate
    case SID_LOOP_ON:     log_d("Thawed loopmode from NVS: SID_LOOP_ON"); break;
    case SID_LOOP_RANDOM: log_d("Thawed loopmode from NVS: SID_LOOP_RANDOM"); break;
    case SID_LOOP_OFF:    log_d("Thawed loopmode from NVS: SID_LOOP_OFF"); break;
    default: playerloopmode = SID_LOOP_OFF; break;
  }
  return (loopmode)playerloopmode;
}


void NVSPrefs::setLoopMode( loopmode mode )
{
  prefs.begin("SIDExplorer");
  prefs.putUChar( "loopmode", (uint8_t)mode );
  prefs.end();
  switch( mode ) {
    case SID_LOOP_ON:     log_d("Saved loopmode to NVS: SID_LOOP_ON"); break;
    case SID_LOOP_RANDOM: log_d("Saved loopmode to NVS: SID_LOOP_RANDOM"); break;
    case SID_LOOP_OFF:    log_d("Saved loopmode to NVS: SID_LOOP_OFF"); break;
  }
}


uint8_t NVSPrefs::getNetMode()
{
  prefs.begin("SIDExplorer");
  uint8_t mode = (uint8_t)prefs.getUChar("netmode", 0 );
  mode = mode%2; // avoid returning poisonous value
  log_v("Thawed mode from NVS: %d", mode );
  prefs.end();
  return mode;
}


void NVSPrefs::setNetMode( uint8_t mode )
{
  prefs.begin("SIDExplorer");
  prefs.putUChar( "netmode", mode%2 );
  prefs.end();
  log_v("Saved netmode to NVS: %d", mode );
}

uint8_t NVSPrefs::getVolume()
{
  prefs.begin("SIDExplorer");
  uint8_t volume = (loopmode)prefs.getUChar("volume", 10 );
  volume = volume%16; // avoid returning poisonous value
  log_v("Thawed volume from NVS: %d", volume );
  prefs.end();
  return volume;
}

void NVSPrefs::setVolume( uint8_t volume )
{
  prefs.begin("SIDExplorer");
  prefs.putUChar( "volume", volume );
  prefs.end();
  log_v("Saved volume to NVS: %d", volume );
}


void NVSPrefs::setLastPath( const char* path )
{
  prefs.begin("SIDExplorer");
  prefs.putString("path", path);
  prefs.end();
}

void NVSPrefs::getLastPath( char* path )
{
  prefs.begin("SIDExplorer");
  size_t pathlen = prefs.getString("path", path, 255);
  if( pathlen <= 0 || path[0] != '/' ) {
    log_d("Defaulting to %s", SID_FOLDER );
    snprintf( path, 255, "%s", SID_FOLDER );
  }
  prefs.end();
}

