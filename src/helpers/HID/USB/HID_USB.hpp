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

  // USB detection callback
  static void onUSBDetect( uint8_t usbNum, void * dev );
  void usbTicker();
  void HIDEventHandler( uint8_t mod, uint8_t key, keyToEvents evt );
  // keyboard data parser to pass to the USB driver as a callback;
  void onUSBReceice(uint8_t usbNum, uint8_t byte_depth, uint8_t* data, uint8_t data_len);
  void HIDBeforeRead();
  void HIDAfterRead();
  void HIDUSBInit();
  bool USBReady();
  HIDControls readUSBHIDKeyboarD();
  // last control key slot, memoizer and getter/setter
  HIDControlKey* setControlKey( HIDControls c, int k );
  HIDControlKey* getControlKey( bool ctrlmode = false );
  HIDControlKey* getLastPressedKey();

#endif
