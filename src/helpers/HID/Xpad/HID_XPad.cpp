#include "HID_XPad.hpp"

#if defined HID_XPAD

  static bool wire_begun = false;

  bool XPadReady()
  {
    return wire_begun;
  }

  void initXPad()
  {
    if( wire_begun ) {
      return;
    }
    Wire.begin( SDA, SCL ); // connect XPadShield
    wire_begun = true;
    readXpad(); // clear xpad input, sometimes returns garbage after init
  }


  void beforeXpadRead()
  {
    // don't spam I2C
  }

  void afterXpadRead()
  {
    // Turn I2C on
  }


  static unsigned long xPadMinDelay = 30; // min delay between queryes
  static unsigned long xPadLastQuery = 0;


  struct xpad_button_map_t
  {
    struct bits_t {
      bool up: 1;
      bool down: 1;
      bool right: 1;
      bool left: 1;
      bool a: 1;
      bool b: 1;
      bool c: 1;
      bool d: 1;
    };
    union {
      bits_t bits;
      uint8_t flags;
    };
  };


  Button BtnUp;
  Button BtnDown;
  Button BtnLeft;
  Button BtnRight;
  Button BtnPlay;
  Button BtnAction1;
  Button BtnAction2;
  Button BtnAction3;

  Button *Btns[8] = {
    &BtnUp,
    &BtnDown,
    &BtnLeft,
    &BtnRight,
    &BtnPlay,
    &BtnAction1,
    &BtnAction2,
    &BtnAction3
  };



  HIDControls readXpad()
  {
    unsigned long now = millis();
    if( now - xPadLastQuery > xPadMinDelay ) {
      xPadLastQuery = now;
    } else {
      return HID_NONE;
    }

    Wire.requestFrom(0x10, 1); // scan buttons from XPadShield
    int resp = Wire.read();
    switch(resp) {
      case 0x01: // up
        BtnUp.setState(1);
        return HID_UP;
      break;
      case 0x02: // down
        BtnDown.setState(1);
        return HID_DOWN;
      break;
      case 0x04: // right
        BtnRight.setState(1);
        return HID_RIGHT;
      break;
      case 0x08: // left
        BtnRight.setState(1);
        return HID_LEFT;
      break;
      break;
      case 0x10: // B (play/stop button)
        BtnPlay.setState(1);
        return HID_PLAYSTOP;
      break;
      case 0x20: // Action button (toggle loop options)
        BtnAction1.setState(1);
        return HID_ACTION1;
      break;
      case 0x40: // C (minus '-' decrease/previous)
        BtnAction2.setState(1);
        return HID_ACTION2;
      break;
      case 0x80: // D (plus '+' increase/next)
        BtnAction3.setState(1);
        return HID_ACTION3;
      break;
      default: // simultaneous buttons push ?
        return HID_NONE;
      break;
    }
  }

#endif
