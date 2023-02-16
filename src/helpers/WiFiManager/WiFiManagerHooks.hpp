#pragma once

#include "../../config.h"

namespace WiFiManagerNS
{
  static bool configSaved = false;
  // Optional callback function, fired when NTP gets updated.
  // Used to print the updated time or adjust an external RTC module.
  void on_time_available(struct timeval *t);
  void setup();
  void loop();
}
