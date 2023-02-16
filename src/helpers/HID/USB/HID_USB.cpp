#include "HID_USB.hpp"

#if defined HID_USB

  static keyEvent kEvt;


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


  void SidKeyboardParser::OnKeyUp(uint8_t mod, uint8_t key)
  {
    kEvt.mod = mod;
    kEvt.key = key;
    kEvt.evt = onKeyUp;
    kEvt.keyStr = OemToAscii(mod, key);
    kEvt.processed = false;
    if( sendEvent ) sendEvent( mod, key, onKeyUp );
  }


  void SidKeyboardParser::OnKeyDown(uint8_t mod, uint8_t key)
  {
    if( !kEvt.processed ) {
      log_w("Losing unprocessed keyEvent (0x%02x)", kEvt.key );
    }
    kEvt.mod = mod;
    kEvt.key = key;
    kEvt.evt = onKeyDown;
    kEvt.keyStr = OemToAscii(mod, key);
    kEvt.processed = false;
    if( sendEvent ) sendEvent( mod, key, onKeyDown );
  }


  void SidKeyboardParser::OnControlKeysChanged(uint8_t before, uint8_t after)
  {
    if( sendEvent ) sendEvent( before, after, onControlKeysChanged );

    MODIFIERKEYS beforeMod;
    *((uint8_t*)&beforeMod) = before;

    MODIFIERKEYS afterMod;
    *((uint8_t*)&afterMod) = after;

    if (beforeMod.bmLeftCtrl != afterMod.bmLeftCtrl) {
      log_d("LeftCtrl changed");
    }
    if (beforeMod.bmLeftShift != afterMod.bmLeftShift) {
      log_d("LeftShift changed");
    }
    if (beforeMod.bmLeftAlt != afterMod.bmLeftAlt) {
      log_d("LeftAlt changed");
    }
    if (beforeMod.bmLeftGUI != afterMod.bmLeftGUI) {
      log_d("LeftGUI changed");
    }

    if (beforeMod.bmRightCtrl != afterMod.bmRightCtrl) {
      log_d("RightCtrl changed");
    }
    if (beforeMod.bmRightShift != afterMod.bmRightShift) {
      log_d("RightShift changed");
    }
    if (beforeMod.bmRightAlt != afterMod.bmRightAlt) {
      log_d("RightAlt changed");
    }
    if (beforeMod.bmRightGUI != afterMod.bmRightGUI) {
      log_d("RightGUI changed");
    }
  }

  /*
  void SidKeyboardParser::PrintKey(uint8_t m, uint8_t key)
  {
    MODIFIERKEYS mod;
    *((uint8_t*)&mod) = m;
    Serial.print((mod.bmLeftCtrl   == 1) ? "C" : " ");
    Serial.print((mod.bmLeftShift  == 1) ? "S" : " ");
    Serial.print((mod.bmLeftAlt    == 1) ? "A" : " ");
    Serial.print((mod.bmLeftGUI    == 1) ? "G" : " ");

    Serial.print(" >");
    Serial.printf("0x%02x", key );
    Serial.print("< ");

    Serial.print((mod.bmRightCtrl   == 1) ? "C" : " ");
    Serial.print((mod.bmRightShift  == 1) ? "S" : " ");
    Serial.print((mod.bmRightAlt    == 1) ? "A" : " ");
    Serial.println((mod.bmRightGUI    == 1) ? "G" : " ");
  };
  */


  // USB detection callback
  static void onUSBDetect( uint8_t usbNum, void * dev )
  {
    sDevDesc *device = (sDevDesc*)dev;
    log_w("New device detected on USB#%d", usbNum);
    log_w("desc.idVendor           = 0x%04x", device->idVendor);
    log_w("desc.idProduct          = 0x%04x", device->idProduct);
  }



  SidKeyboardParser Prs;
  //static int lastKeyReleased = -1;
  //static int lastKeyPressed  = -1;
  static int lastKeyDown     = -1;
  static int lastKeyUp       = -1;
  static bool usb_started = false;

  void usbTicker() {
    vTaskDelay(1);
  }


  void HIDEventHandler( uint8_t mod, uint8_t key, keyToEvents evt )
  {
    switch( evt ) {
      case onKeyDown:
        log_v("[Event=onKeyDown] Key='%s' (0x%02x), mod=0x%02x", String(kEvt.keyStr).c_str(), key, mod );
        if( kEvt.keyStr > 0 ) {
          lastKeyDown = kEvt.keyStr;
        } else {
          //TODO switch( kEvt.key )
        }
      break;
      case onKeyUp:
        log_v("[Event=onKeyUp] Key='%s' (0x%02x), mod=0x%02x", String(kEvt.keyStr).c_str(), key, mod );
        lastKeyUp = key;
        if( kEvt.keyStr > 0 ) {
          lastKeyUp = kEvt.keyStr;
        } else {
          //TODO switch( kEvt.key )
        }

      break;
      default:
      break;
    }
  }


  bool USBReady()
  {
    return usb_started;
  }

  // keyboard data parser to pass to the USB driver as a callback
  void onUSBReceice(uint8_t usbNum, uint8_t byte_depth, uint8_t* data, uint8_t data_len)
  {
    // parse USB data, may contain several keys
    Prs.Parse( data_len, data );
  }


  void HIDBeforeRead()
  {
    //USH.TimerResume();
  }

  void HIDAfterRead()
  {
    //USH.TimerPause();
  }


  void HIDUSBInit()
  {
    if( usb_started ) {
      log_e("USB already started!");
      return;
    }
    // attach to USB event handler
    Prs.setEventHandler( HIDEventHandler );
    // init the USB Host
    USH.setTaskPriority( SID_HID_TASK_PRIORITY );
    USH.setTaskCore( SID_HID_CORE ); // inherit from config.h
    USH.init(
      (usb_pins_config_t){
        DP_P0, DM_P0,
        DP_P1, DM_P1,
        DP_P2, DM_P2,
        DP_P3, DM_P3
      },
      onUSBDetect,
      onUSBReceice,
      usbTicker
    );
    log_w("USB Soft Host got start signal from core #%d", xPortGetCoreID() );
    usb_started = true;
    vTaskDelay(10);
  }


  // last control key slot, memoizer and getter/setter
  HIDControlKey lastControlKey;
  HIDControlKey* setControlKey( HIDControls c, int k )
  {
    lastControlKey.set(c,k);
    return &lastControlKey;
  }
  HIDControlKey* getControlKey( bool ctrlmode = false )
  {
    if( kEvt.processed ) return setControlKey(HID_NONE, 0);
    if( kEvt.evt == onKeyDown ) return setControlKey(HID_NONE, 0);
    kEvt.processed = true;
    switch(kEvt.key) {
      // handled non printable keys
      case KEY_PAGEUP:    return setControlKey(HID_UP, 0);
      case KEY_UP:        return setControlKey(HID_UP, 0);
      case KEY_PAGEDOWN:  return setControlKey(HID_DOWN, 0);
      case KEY_DOWN:      return setControlKey(HID_DOWN, 0);
      case KEY_LEFT:      return setControlKey(HID_LEFT, 0);
      case KEY_RIGHT:     return setControlKey(HID_RIGHT, 0);
      case KEY_ESC:       return setControlKey(HID_ACTION5, 0);
      case KEY_BACKSPACE: return setControlKey(HID_ACTION4, 0);
      default:
        // unhandled non printable keys with ctrlmod = false
        if( !ctrlmode ) {
          return setControlKey(HID_NONE, kEvt.keyStr == 0 ? -1 : kEvt.keyStr);
        }
        // unhandled non printable keys with ctrlmode = true
        if( kEvt.keyStr == 0 ) {
          return setControlKey(HID_NONE, -1);
        }
        // handled printable keys with ctrlmode = true
        char c = tolower( kEvt.keyStr );
        switch(c) {
          case 's':
            return setControlKey(HID_DOWN, 0);
          break;
          case 'w':
            return setControlKey(HID_UP, 0);
          break;
          case 'd':
            return setControlKey(HID_RIGHT, 0);
          break;
          case 'a':
            return setControlKey(HID_LEFT, 0);
          break;
          case ' ': // space key
            return setControlKey(HID_PLAYSTOP, 0);
          break;
          case '\r': // 0x0d
          //case '/':
            return setControlKey(HID_ACTION1, 0);
          break;
          case '+':
            return setControlKey(HID_ACTION2, 0);
          break;
          case '-': // D
            return setControlKey(HID_ACTION3, 0);
          break;
          case '?':
            return setControlKey(HID_SEARCH, 0);
          break;
          case 'y':
            return setControlKey(HID_PROMPT_Y, 0);
          break;
          case 'n':
            return setControlKey(HID_PROMPT_N, 0);
          break;
          default:
            return setControlKey(HID_NONE, kEvt.keyStr == 0 ? -1 : kEvt.keyStr);
          break;
        }
      break;
    }
  }




  HIDControlKey* getLastPressedKey()
  {
    if( kEvt.processed ) return NULL;
    return getControlKey( false );
  }


  HIDControls readUSBHIDKeyboarD()
  {
    if( !usb_started ) {
      HIDUSBInit();
      return HID_NONE;
    }
    if( kEvt.processed ) return HID_NONE;
    HIDControlKey *c = getControlKey( true );
    return c->control;
  }

#endif
