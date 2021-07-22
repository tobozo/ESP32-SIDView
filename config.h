#ifndef _SID_CONFIG_H_
#define _SID_CONFIG_H_

// this build's pin setup
#define SID_CLOCK 25
#define SID_DATA 15
#define SID_LATCH 2
#define SID_CLOCK_PIN 26 // optional

// mandatory defines for this app
#define SID_FS       M5STACK_SD                           // from fs::FS filesystem <SD.h> or ESP32-Chimera-Core.hpp
#define SID_FOLDER   "/sid"                               // local path on the SD Card, NO TRAILING SLASH, MUST BE A FOLDER
#define MD5_FOLDER   "/md5"                               // where all the md5 stuff is stored (db file, caches, indexes)
#define MD5_FILE     MD5_FOLDER "/Songlengths.full.md5"   // local path to the HVSC md5 file
#define MD5_FILE_IDX MD5_FILE ".idx"                      // The MD5 data from Songlengths will be indexed into this file
#define HVSC_VERSION "HVSC-74"                            // a unique name for a dotfile to hold archive decompression status
#define MD5_URL      "https://hvsc.c64.org/download/C64Music/DOCUMENTS/Songlengths.md5" // [Optional if SID_DOWNLOAD_ARCHIVES flag is enabled] HVSC Songlengths.md5 file as is
#define HVSC_TGZ_URL "https://phpsecu.re/HVSC-74.tar.gz"  // [Optional if SID_DOWNLOAD_ARCHIVES flag is enabled] tgz archive containing "DEMO", "GAMES" and "MUSICIANS" folders from HVSC

// uncomment one of those
//#define HID_XPAD
//#define HID_USB
#define HID_TOUCH
#define HID_SERIAL


// Some core settings inherit from <SID6581.h> SID_PLAYER_CORE / SID_CPU_CORE / SID_QUEUE_CORE

#if defined HID_USB

  #define SPI_CORE 1
  #define APP_CORE 0

  #define SID_QUEUE_CORE  APP_CORE  // default = 0
  #define SID_CPU_CORE    APP_CORE  // default = SID_QUEUE_CORE
  #define SID_PLAYER_CORE APP_CORE  // default = 1 - SID_QUEUE_CORE

  #define SID_HID_CORE      SPI_CORE
  #define SID_MAINTASK_CORE APP_CORE
  #define SID_DRAWTASK_CORE APP_CORE

  #define SID_HID_TASK_PRIORITY 5

#elif defined HID_TOUCH || defined HID_SERIAL || defined HID_XPAD

  //#define SID_QUEUE_CORE 1
  //#define SID_PLAYER_CORE 1

  #define SID_MAINTASK_CORE 1
  #define SID_DRAWTASK_CORE 0

#else
  // TODO: implement Serial
  #error "Please selectd a HID method (xpad or usb or touch)"
#endif


#define SID_MAINTASK_PRIORITY 5
#define SID_DRAWTASK_PRIORITY 6



//#define SD_UPDATABLE          // comment this out to disable SD-Updater ( disabling saves ~3% of program storage space)
//#define SID_DOWNLOAD_ARCHIVES // comment this out if the archives are already on the SD Card ( disabling saves ~42% of program storage space and 40Kb ram)
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

#include "modules/UI/SIDExplorer.hpp"

#endif
