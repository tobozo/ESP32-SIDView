#ifndef _SID_CONFIG_H_
#define _SID_CONFIG_H_

// this build's pin setup
#define SID_CLOCK 25
#define SID_DATA 15
#define SID_LATCH 2
#define SID_CLOCK_PIN 26

#define SID_FS       SD // from fs::FS filesystem <SD.h>
#define SID_FOLDER   "/sid" // local path on the SD Card, NO TRAILING SLASH, MUST BE A FOLDER
#define MD5_FOLDER   "/md5" // where all the md5 stuff is stored (db file, cache, index)
#define MD5_FILE     MD5_FOLDER "/Songlengths.full.md5" // local path to the HVSC md5 file
#define MD5_FILE_IDX MD5_FILE ".idx"
#define MD5_URL      "https://phpsecu.re/Songlengths.md5" // remote url

// Use this if your ESP has never connected to your WiFi before
//#define WIFI_SSID "my-wifi-ssid"
//#define WIFI_PASS "my-wifi-password"

// hail the c64 colors !
#define C64_DARKBLUE      0x4338ccU
#define C64_LIGHTBLUE     0xC9BEFFU
#define C64_MAROON        0xA1998Bu
#define C64_MAROON_DARKER 0x797264U
#define C64_MAROON_DARK   0x938A7DU

// some default settings for the player
#define SONG_TIMEOUT 180 // 180s = 3mn
#define SONG_TIMEOUT_MS SONG_TIMEOUT*1000

// uncomment this to enable SD-Updater
// #define SD_UPDATABLE

// some UI dimensions
#define HEADER_HEIGHT 41                               // arbitrarily chosen height
#define VOICE_HEIGHT  ((tft.height()-HEADER_HEIGHT)/3) // = 29 on a 128x128
#define VOICE_WIDTH   tft.width()

#include <ESP32-Chimera-Core.h>  // https://github.com/tobozo/ESP32-Chimera-Core
#define tft M5.Lcd // syntax sugar

#ifdef SD_UPDATABLE
  // SD-loadability
  #include "modules/SDUpdater/SDUpdaterHooks.h"
#endif

#define DEST_FS_USES_SD
#include <ESP32-targz.h>         // https://github.com/tobozo/ESP32-Targz

#include <SidPlayer.h>           // https://github.com/hpwit/SID6581

// MD5FileParser config for SidPlayer
MD5FileConfig MD5Config =
{
  &SID_FS,          // SD
  SID_FOLDER,       // /sid
  MD5_FOLDER,       // /md5
  MD5_FILE,         // /md5/Songlengths.full.md5
  MD5_FILE_IDX,     // /md5/Songlengths.full.md5.idx
  MD5_URL,          // https://www.prg.dtu.dk/HVSC/C64Music/DOCUMENTS/Songlengths.md5
  MD5_INDEX_LOOKUP, // MD5_RAW_READ, MD5_INDEX_LOOKUP, MD5_RAINBOW_LOOKUP
  nullptr           // callback function for progress when the init happens
};

// High Voltage SID Collection downloader
#include "modules/Downloader/SIDArchiveManager.h"

/*
SID_Archive C64MusicDemos("C64MusicDemos", "https://phpsecu.re/C64MusicDemos.tar.gz", "/DEMOS",     &MD5Config );
SID_Archive C64Musicians ("C64Musicians",  "https://phpsecu.re/C64Musicians.tar.gz",  "/MUSICIANS", &MD5Config );
SID_Archive C64MusicGames("C64MusicGames", "https://phpsecu.re/C64MusicGames.tar.gz", "/GAMES",     &MD5Config );
SID_Archive HVSC[3] = { C64MusicGames, C64MusicDemos, C64Musicians };
*/

SID_Archive HVSC74("HVSC 74", "https://phpsecu.re/HVSC-74.tar.gz", "HVSC-74", &MD5Config );
SID_Archive HVSC[1] = { HVSC74 };

// load the Player
#include "modules/UI/SIDViewer.h"

#endif
