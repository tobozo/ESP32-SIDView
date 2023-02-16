#pragma once

#include <Arduino.h>

// uncomment at least one of those
#define HID_SERIAL // accept commands from serial console
#define HID_XPAD   // use I2C X-Pad 8 buttons interface
//#define HID_USB    // use HID Keyboard, uses SDA/SCL as D+/D- for USB-LS Soft Host, highly unstable
//#define HID_TOUCH  // use Touch from display, very basic touch to button interface

#define TFT_ROTATION 2 // customize this according to your display, valid values are 0...7

#define LED_VU_PIN LED_BUILTIN // Builtin (or external) LED Pin to use as a vu-meter for sound level (builtin led pin on D32Pro = LED_BUILTIN = GPIO_NUM_5)
//#define SD_UPDATABLE           // enable SD-Updater, disabling saves 44Kb of program storage space and ~312 bytes of heap
//#define SID_DOWNLOAD_ARCHIVES  // HVSC archive can be downloaded to the SD Card if it's empty on load, disabling saves ~3.5Kb of program storage space and ~2.5Kb of heap
//#define ENABLE_HVSC_SEARCH     // enable elastic-search style indexing/sharding, disabling saves ~46Kb storage spage and ~256 bytes of heap

#define SID_FOLDER   "/sid"    // local path on the SD Card where SID files can be found, NO TRAILING SLASH, MUST BE A FOLDER, CANNOT BE ROOT
#define HVSC_VERSION "HVSC-78" // a unique name for a the current HVSC Version

#if defined (CONFIG_IDF_TARGET_ESP32)
  #define SID_CLOCK     25
  #define SID_DATA      15
  #define SID_LATCH      2
  #define SID_CLOCK_PIN 26 // optional if the board already has an oscillator
  #define SID_PLAYER // load the sidPlayer module over the SID library
#elif defined (CONFIG_IDF_TARGET_ESP32S3) //|| defined (CONFIG_IDF_TARGET_ESP32S2)
  #define SID_CLOCK     41 // multiplexer bus clock
  #define SID_DATA      39 // multiplexer bus data
  #define SID_LATCH     40 // multiplexer bus latch
  #define SID_CLOCK_PIN  4  // PHI: optional if the board already has an oscillator
  #define SID_PLAYER // load the sidPlayer module over the SID library
#else
  #error "Unsupported architecture, feel free to unlock and contribute!"
#endif

// DON'T EDIT ANYTHING AFTER HERE

// mandatory defines for this app
#define SID_FS       M5STACK_SD                           // from fs::FS filesystem <SD.h> or ESP32-Chimera-Core.hpp
#define MD5_FOLDER   "/md5"                               // where all the md5 stuff is stored (db file, caches, indexes)
#define MD5_FILE     MD5_FOLDER "/Songlengths.full.md5"   // local path to the HVSC md5 file
#define MD5_FILE_IDX MD5_FILE ".idx"                      // The MD5 data from Songlengths will be indexed into this file
#define MD5_URL      "https://hvsc.c64.org/download/C64Music/DOCUMENTS/Songlengths.md5" // [Optional if SID_DOWNLOAD_ARCHIVES flag is enabled] HVSC Songlengths.md5 file as is

// Use this if your ESP has never connected to your WiFi before
//#define WIFI_SSID "my-wifi-ssid"
//#define WIFI_PASS "my-wifi-password"

// // hail the c64 colors !
// #define C64_DARKBLUE      0x4338ccU
// #define C64_LIGHTBLUE     0xC9BEFFU
// #define C64_MAROON        0xA1998BU
// #define C64_MAROON_DARKER 0x797264U
// #define C64_MAROON_DARK   0x938A7DU
//
// #define C64_DARKBLUE_565      tft.color565(0x43, 0x38, 0xcc )
// #define C64_LIGHTBLUE_565     tft.color565(0xC9, 0xBE, 0xFF )
// #define C64_MAROON_565        tft.color565(0xA1, 0x99, 0x8B )
// #define C64_MAROON_DARKER_565 tft.color565(0x79, 0x72, 0x64 )
// #define C64_MAROON_DARK_565   tft.color565(0x93, 0x8A, 0x7D )


#if defined SID_DOWNLOAD_ARCHIVES
  #define HVSC_TGZ_URL "https://phpsecu.re/" HVSC_VERSION ".tar.gz"  // [Optional if SID_DOWNLOAD_ARCHIVES flag is enabled] tgz archive containing "DEMO", "GAMES" and "MUSICIANS" folders from HVSC
  #define DEST_FS_USES_SD // set SD as default filesystem for ESP32-Targz
  #include <ESP32-targz.h>  // https://github.com/tobozo/ESP32-Targz
#else
  #include "./helpers/Path_Utils.hpp"
#endif

// Some core settings inherit from <SID6581.h> SID_PLAYER_CORE / SID_CPU_CORE / SID_QUEUE_CORE
#if defined HID_USB
  // USB-Soft-Host isn't multithread-safe at the time of writing this app
  // so there's only the spi core, and the app core
  #define SPI_CORE 1
  #define APP_CORE 0
  #define SID_QUEUE_CORE    APP_CORE  // default = 0
  #define SID_CPU_CORE      APP_CORE  // default = SID_QUEUE_CORE
  #define SID_PLAYER_CORE   APP_CORE  // default = 1 - SID_QUEUE_CORE
  #define SID_HID_CORE      SPI_CORE  // soft-host USB requires a full core
  #define SID_MAINTASK_CORE APP_CORE  // maintask and drawtask will compete
  //#define SID_DRAWTASK_CORE APP_CORE  // maintask and drawtask will compete
  #define SID_HID_TASK_PRIORITY 5
#elif defined HID_TOUCH || defined HID_SERIAL || defined HID_XPAD
  // multithread safety isn't threatened by a hacky USB driver, let
  // the SID6581 library assign the CPU/QUEUE core and spread
  // maintask and drawtask on different cores.
  #define SID_MAINTASK_CORE 1
  //#define SID_DRAWTASK_CORE 0
  #define SID_HID_CORE      0
#else
  #error "At least one HID method must be defined! (HID_SERIAL, HID_XPAD, HID_USB or HID_TOUCH)"
#endif

//#define SID_MAINTASK_PRIORITY SID_CPU_TASK_PRIORITY-1
//#define SID_DRAWTASK_PRIORITY 4
#define SID_HIDTASK_PRIORITY  0

// some default settings for the player
#define SONG_TIMEOUT 180 // 180s = 3mn
#define SONG_TIMEOUT_MS SONG_TIMEOUT*1000

#define SID_PLAYER // enable <SID6581.hpp> plugin
#define SID_INSTRUMENTS // enable <SID6581.hpp> plugin
#include <SID6581.hpp> // https://github.com/hpwit/SID6581
// lazily borrowing gnu_basename() from <SID6581>/<SID_HVSC_Indexer.h>
#define gnu_basename BufferedIndex::gnu_basename

#if defined HID_TOUCH
  #define HAS_TOUCH // enable optional touch handling in ESP32-Chimera-Core
#endif
#include <ESP32-Chimera-Core.h>  // https://github.com/tobozo/ESP32-Chimera-Core

#if defined SD_UPDATABLE
  // SD Updater lobby config
  #define SDU_APP_NAME   "SID Player"
  #define SDU_APP_PATH   "/SIDPlayer.bin"
  #define SDU_APP_AUTHOR "@tobozo"
  #define SDU_APP_KEY    "217A25432A462D4A404E635266556A586E3272357538782F413F4428472B4B62"
  #include "./SDUpdater/SDUpdaterHooks.hpp" // SD-loadability
#endif

namespace SIDView
{
  extern SIDTunesPlayer *sidPlayer; // SID6581 tunes player
  extern LGFX *display;
  extern xSemaphoreHandle mux;
};
