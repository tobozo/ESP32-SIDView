#pragma once

#include "../HID_Common.hpp"

#if defined HID_XPAD

  #include <Wire.h>

  #if defined (CONFIG_IDF_TARGET_ESP32)

    #define XPAD_SDA SDA
    #define XPAD_SCL SCL

  #elif defined (CONFIG_IDF_TARGET_ESP32S3) //|| defined (CONFIG_IDF_TARGET_ESP32S2)

    #define XPAD_SDA 9
    #define XPAD_SCL 10

  #else
    #error "Unsupported architecture, feel free to unlock and contribute!"
  #endif

  bool XPadReady();
  void initXPad();
  HIDControls readXpad();
  void beforeXpadRead();
  void afterXpadRead();

#endif
