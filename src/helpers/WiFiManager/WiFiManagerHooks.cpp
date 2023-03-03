#include "WiFiManagerHooks.hpp"

#include <WiFiManager.h>   // https://github.com/tzapu/WiFiManager
#include <WiFiManagerTz.h> // https://github.com/tobozo/WiFiManagerTz


// Note: WiFiManagerNS namespace belongs to <WiFiManagerTz.h> library
namespace WiFiManagerNS
{

  WiFiManager *wifiManager = nullptr;

  // Optional callback function, fired when NTP gets updated.
  // Used to print the updated time or adjust an external RTC module.
  void on_time_available(struct timeval *t)
  {
    Serial.print("[NTP] Adjusted datetime: ");
    struct tm timeInfo;
    getLocalTime(&timeInfo, 1000);
    Serial.println(&timeInfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
    net::wifiOff();
    // RTC.adjust( &timeInfo );
    net::timeSynced = true; // tell deepsid the ESP32 is up to datetime
  }

  void setup()
  {
    if( !wifiManager ) wifiManager = new WiFiManager();
    // optionally attach external RTC update callback
    NTP::onTimeAvailable( &on_time_available );
    // attach NTP/TZ/Clock-setup page to the WiFi Manager
    WiFiManagerNS::init( wifiManager );

    if( !wifiManager ) {
      log_e("Unable to launch WiFiManager, aborting");
      return;
    }

    // /!\ make sure "custom" is listed there as it's required to pull the "Setup Clock" button
    std::vector<const char *> menu = {"wifi", "info", "custom", "param", "sep", "restart", "exit"};
    wifiManager->setMenu(menu);

    wifiManager->setConnectRetries(10);          // necessary with sluggish routers and/or hidden AP
    wifiManager->setCleanConnect(true);          // ESP32 wants this
    wifiManager->setConfigPortalBlocking(false); // /!\ false=use "wifiManager->process();" in the loop()
    wifiManager->setConfigPortalTimeout(120);    // will wait 2mn before closing portal
    wifiManager->setSaveConfigCallback([](){ configSaved = true;}); // restart on credentials save, ESP32 doesn't like to switch between AP/STA

    if( wifiManager->autoConnect( WIFIMANAGER_AP_SSID, WIFIMANAGER_AP_PASS ) ) {
      Serial.print("Connected to Access Point, visit http://");
      Serial.print(WiFi.localIP());
      Serial.println(" to setup the clock or change WiFi settings");
      wifiManager->startConfigPortal();
    } else {
      Serial.println("Configportal is running, no WiFi has been set yet");
    }

    if(WiFi.status() == WL_CONNECTED){
      configTime();
    }
  }

  void loop()
  {
    if( wifiManager ) wifiManager->process();
  }

};




