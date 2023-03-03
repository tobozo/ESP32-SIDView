#pragma once

#include "../HID_Common.hpp"

#if defined HID_USB

  #include "HID_USB_KbdParser.hpp"

  enum keyToEvents
  {
    onNone,
    onKeyDown,
    onKeyUp,
    onControlKeysChanged,
    onKeyPressed,
    onKeyReleased
  };


  struct keyEvent
  {
    uint8_t mod = 0;
    uint8_t key = 0;
    keyToEvents evt = onNone;
    char keyStr = 0; // 0 = non printable
    bool processed = true;
  };


  typedef void (*eventEmitter)(uint8_t mod, uint8_t key, keyToEvents evt );


  class SidKeyboardParser : public KeyboardReportParser
  {
    public:
      void setEventHandler( eventEmitter cb ) { sendEvent = cb; };
      //void PrintKey(uint8_t mod, uint8_t key);
      char lastkey[2] = {0,0};

    protected:
      void OnControlKeysChanged(uint8_t before, uint8_t after);
      void OnKeyDown	(uint8_t mod, uint8_t key);
      void OnKeyUp	(uint8_t mod, uint8_t key);
      eventEmitter sendEvent = nullptr;
  };

  void HIDBeforeRead();
  void HIDAfterRead();
  void HIDUSBInit();
  bool USBReady();
  HIDControls readUSBHIDKeyboarD();
  // last control key slot
  HIDControlKey* getUSBLastPressedKey();

#endif
