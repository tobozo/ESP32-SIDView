#include "HID_Common.hpp"
#include "../../config.h"

// extern void initXPad();
// extern void HIDUSBInit();
// extern void initTouch();
// extern void initSerial();
// extern int readXpad();
// extern int readUSBHIDKeyboarD();
// extern int readTouch();
// extern int readSerial();
// extern void beforeXpadRead();
// extern void HIDBeforeRead();
// extern void beforeTouchRead();
// extern void beforeSerialRead();
// extern void afterXpadRead();
// extern void HIDAfterRead();
// extern void afterTouchRead();
// extern void afterSerialRead();


uint32_t inactive_since = 0; // UI debouncer

#if defined HID_XPAD
  #include "./Xpad/HID_XPad.hpp"
#endif
#if defined HID_USB
  #include "./USB/HID_USB.hpp"
#endif
#if defined HID_TOUCH
  #include "./Touch/HID_Touch.hpp"
#endif
#if defined HID_SERIAL
  #include "./Serial/HID_Serial.hpp"
#endif

bool HIDReady()
{
  #if defined HID_XPAD
    return XPadReady();
  #endif
  #if defined HID_USB
    return USBReady();
  #endif
  #if defined HID_TOUCH
    return TouchReady();
  #endif
  #if defined HID_SERIAL
    return SerialReady();
  #endif
}


void initHID()
{
  #if defined HID_XPAD
    initXPad();
  #endif
  #if defined HID_USB
    HIDUSBInit();
  #endif
  #if defined HID_TOUCH
    initTouch();
  #endif
  #if defined HID_SERIAL
    initSerial();
  #endif
}

HIDControls readHID()
{
  HIDControls ret = HID_NONE;
  #if defined HID_XPAD
    ret = readXpad();
    if( ret !=HID_NONE ) return ret;
  #endif
  #if defined HID_USB
    ret = readUSBHIDKeyboarD();
    if( ret !=HID_NONE ) return ret;
  #endif
  #if defined HID_TOUCH
    ret = readTouch();
    if( ret !=HID_NONE ) return ret;
  #endif
  #if defined HID_SERIAL
    ret = readSerial();
  #endif
  return ret;
}

void beforeHIDRead()
{
  #if defined HID_XPAD
    beforeXpadRead();
  #endif
  #if defined HID_USB
    HIDBeforeRead();
  #endif
  #if defined HID_TOUCH
    beforeTouchRead();
  #endif
  #if defined HID_SERIAL
    beforeSerialRead();
  #endif
}

void afterHIDRead()
{
  #if defined HID_XPAD
    afterXpadRead();
  #endif
  #if defined HID_USB
    HIDAfterRead();
  #endif
  #if defined HID_TOUCH
    afterTouchRead();
  #endif
  #if defined HID_SERIAL
    afterSerialRead();
  #endif
}



HIDControlKey* getLastPressedKey()
{
  #if defined HID_USB
    return getUSBLastPressedKey();
  #endif
  return nullptr;
}

