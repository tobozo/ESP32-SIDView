#pragma once

#include <Arduino.h>

// ---- HID Configuration ----
// uncomment at least one of those
#define HID_SERIAL // accept commands from serial console, better leave this uncommented
#define HID_XPAD   // use I2C X-Pad 8 buttons interface
//#define HID_USB    // use HID Keyboard, uses SDA/SCL as D+/D- for USB-LS Soft Host, highly unstable
//#define HID_TOUCH  // use Touch from display, very basic touch to button interface, /!\ may induce bus conflicts

#define TFT_ROTATION 2 // customize this according to your display, valid values are 0...7

// ---- Plugins Actication ----
#define LED_VU_PIN LED_BUILTIN // Builtin (or external) LED Pin to use as a vu-meter for sound level (builtin led pin on D32Pro = LED_BUILTIN = GPIO_NUM_5)
//#define SD_UPDATABLE           // enable SD-Updater, disabling saves 44Kb of program storage space and ~312 bytes of heap
//#define SID_DOWNLOAD_ARCHIVES  // HVSC archive can be downloaded to the SD Card if it's empty on load, disabling saves ~3.5Kb of program storage space and ~2.5Kb of heap
//#define ENABLE_HVSC_SEARCH     // Experimental, requires HID_USB: enable elastic-search style indexing/sharding, disabling saves ~46Kb storage spage and ~256 bytes of heap

#define SID_FOLDER   "/sid"    // local path on the SD Card where SID files can be found, NO TRAILING SLASH, MUST BE A FOLDER, CANNOT BE ROOT
#define HVSC_VERSION "HVSC-78" // a unique name for a the current HVSC Version


// ---- SID6581 Pins Configuration ---
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

#if defined ENABLE_HVSC_SEARCH && !defined HID_USB
  #pragma message "ENABLE_HVSC_SEARCH requires HID_USB, but HID_USB is disabled!"
  #pragma message "Disabling ENABLE_HVSC_SEARCH"
  //#undef ENABLE_HVSC_SEARCH
#endif


// mandatory defines for this app


// ---- Filesystem Configuration ----
#define SID_FS       M5STACK_SD                           // from fs::FS filesystem <SD.h> or ESP32-Chimera-Core.hpp
#define MD5_FOLDER   "/md5"                               // where all the md5 stuff is stored (db file, caches, indexes)
#define MD5_FILE     MD5_FOLDER "/Songlengths.full.md5"   // local path to the HVSC md5 file
#define MD5_FILE_IDX MD5_FILE ".idx"                      // The MD5 data from Songlengths will be indexed into this file
#define MD5_URL      "https://hvsc.c64.org/download/C64Music/DOCUMENTS/Songlengths.md5" // [Optional if SID_DOWNLOAD_ARCHIVES flag is enabled] HVSC Songlengths.md5 file as is

// ---- WiFi Configuration ----
// optional SSID/PASS to your router can be hardcoded, will use WiFi.begin() and/or Autoconnect otherwise
//#define WIFI_SSID "my-wifi-ssid"
//#define WIFI_PASS "my-wifi-password"
// Default SSID/Pass for WiFiManager
#define WIFIMANAGER_AP_SSID "AutoConnectAP"
#define WIFIMANAGER_AP_PASS "12345678"


#if defined SID_DOWNLOAD_ARCHIVES
  #define HVSC_TGZ_URL "https://phpsecu.re/" HVSC_VERSION ".tar.gz"  // [Optional if SID_DOWNLOAD_ARCHIVES flag is enabled] tgz archive containing "DEMO", "GAMES" and "MUSICIANS" folders from HVSC
  #define DEST_FS_USES_SD // set SD as default filesystem for ESP32-Targz
  #include <ESP32-targz.h>  // https://github.com/tobozo/ESP32-Targz
#else
  #include "./helpers/Path_Utils.hpp"
#endif

// ---- Optional/Experimental USB-HID Configuration ----
// Some core settings inherit from <SID6581.h> SID_PLAYER_CORE / SID_CPU_CORE / SID_QUEUE_CORE
#if defined HID_USB
  // USB-Soft-Host isn't multithread-safe at the time of writing this app
  #define SID_MAINTASK_CORE    0
  #define SID_HID_CORE         1 // soft-host USB requires a full core
  #define SID_HIDTASK_PRIORITY 5

  // WARN: when using SCL/SDA, pins must not be shared
  // with another I2C device !
  #define DP_P0  SCL  // always enabled
  #define DM_P0  SDA  // always enabled
  #define DP_P1  -1
  #define DM_P1  -1
  #define DP_P2  -1
  #define DM_P2  -1
  #define DP_P3  -1
  #define DM_P3  -1
  #define DEBUG_ALL
  #include <ESP32-USB-Soft-Host.h> // https://github.com/tobozo/ESP32-USB-Soft-Host

#elif defined HID_TOUCH || defined HID_SERIAL || defined HID_XPAD
  // multithread safety isn't threatened by a hacky USB driver, let
  // the SID6581 library assign the CPU/QUEUE core and spread
  // maintask and drawtask on different cores.
  #define SID_MAINTASK_CORE    1
  #define SID_HID_CORE         0
  #define SID_HIDTASK_PRIORITY 0
#else
  #error "At least one HID method must be defined! (HID_SERIAL, HID_XPAD, HID_USB or HID_TOUCH)"
#endif

//#define SID_MAINTASK_PRIORITY SID_CPU_TASK_PRIORITY-1
//#define SID_DRAWTASK_PRIORITY 4


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
  #include <M5StackUpdater.h>      // https://github.com/tobozo/M5Stack-SD-Updater
#endif


#include "./helpers/types.hpp"
