#pragma once

#include "../HID_Common.hpp"

#if defined HAS_TOUCH

  bool TouchReady();
  void initTouch();
  void beforeTouchRead();
  void afterTouchRead();
  HIDControls readTouch();

#endif
