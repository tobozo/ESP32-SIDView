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

loopmode NVSPrefs::getLoopMode()
{
  prefs.begin("SIDExplorer");
  uint8_t playerloopmode = (loopmode)prefs.getUChar("loopmode", (uint8_t)SID_LOOP_OFF );
  prefs.end();
  log_w("Thawed loopmode from NVS: ");
  switch( playerloopmode ) { // validate
    case SID_LOOP_ON:     log_w("SID_LOOP_ON"); break;
    case SID_LOOP_RANDOM: log_w("SID_LOOP_RANDOM"); break;
    case SID_LOOP_OFF:    log_w("SID_LOOP_OFF"); break;
    default: playerloopmode = SID_LOOP_OFF; break;
  }
  return (loopmode)playerloopmode;
}


void NVSPrefs::setLoopMode( loopmode mode )
{
  prefs.begin("SIDExplorer");
  prefs.putUChar( "loopmode", (uint8_t)mode );
  prefs.end();
  log_w("Saved loopmode to NVS: ");
  switch( mode ) {
    case SID_LOOP_ON:     log_w("SID_LOOP_ON"); break;
    case SID_LOOP_RANDOM: log_w("SID_LOOP_RANDOM"); break;
    case SID_LOOP_OFF:    log_w("SID_LOOP_OFF"); break;
  }
}

uint8_t NVSPrefs::getVolume()
{
  prefs.begin("SIDExplorer");
  uint8_t volume = (loopmode)prefs.getUChar("volume", 10 );
  volume = volume%16; // avoid returning poisonous value
  log_d("Thawed volume from NVS: %d", volume );
  prefs.end();
  return volume;
}

void NVSPrefs::setVolume( uint8_t volume )
{
  prefs.begin("SIDExplorer");
  prefs.putUChar( "volume", volume );
  prefs.end();
  log_d("Saved volume to NVS: %d", volume );
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
    snprintf( path, 255, "%s", SID_FOLDER );
  }
  prefs.end();
}

