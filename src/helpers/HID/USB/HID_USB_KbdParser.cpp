#include "HID_USB_KbdParser.hpp"

#if defined HID_USB

  const uint8_t KeyboardReportParser::numKeys[10] PROGMEM = {'!', '@', '#', '$', '%', '^', '&', '*', '(', ')'};
  const uint8_t KeyboardReportParser::symKeysUp[12] PROGMEM = {'_', '+', '{', '}', '|', '~', ':', '"', '~', '<', '>', '?'};
  const uint8_t KeyboardReportParser::symKeysLo[12] PROGMEM = {'-', '=', '[', ']', '\\', ' ', ';', '\'', '`', ',', '.', '/'};
  const uint8_t KeyboardReportParser::padKeys[5] PROGMEM = {'/', '*', '-', '+', '\r'};

  void KeyboardReportParser::Parse(uint8_t len, uint8_t *buf)
  {
    if (buf[2] == 1)
      return;
    // provide event for changed control key state
    if (prevState.bInfo[0x00] != buf[0x00]) {
      OnControlKeysChanged(prevState.bInfo[0x00], buf[0x00]);
    }

    for (uint8_t i = 2; i < 8; i++) {
      bool down = false;
      bool up = false;

      for (uint8_t j = 2; j < 8; j++) {
        if (buf[i] == prevState.bInfo[j] && buf[i] != 1)
          down = true;
        if (buf[j] == prevState.bInfo[i] && prevState.bInfo[i] != 1)
          up = true;
      }
      if (!down) {
        HandleLockingKeys(buf[i]);
        OnKeyDown(*buf, buf[i]);
      }
      if (!up)
        OnKeyUp(prevState.bInfo[0], prevState.bInfo[i]);
    }
    for (uint8_t i = 0; i < 8; i++)
      prevState.bInfo[i] = buf[i];
  }


  uint8_t KeyboardReportParser::OemToAscii(uint8_t mod, uint8_t key)
  {
    uint8_t shift = (mod & 0x22);
    // [a-z]
    if (VALUE_WITHIN(key, 0x04, 0x1d)) {
      // Upper case letters
      if ((bmCapsLock == 0 && shift) ||
        (bmCapsLock == 1 && shift == 0))
        return (key - 4 + 'A');

        // Lower case letters
      else
        return (key - 4 + 'a');
    }// Numbers
    else if (VALUE_WITHIN(key, 0x1e, 0x27)) {
      if (shift)
        return ((uint8_t)pgm_read_byte(&getNumKeys()[key - 0x1e]));
      else
        return ((key == UHS_HID_BOOT_KEY_ZERO) ? '0' : key - 0x1e + '1');
    }// Keypad Numbers
    else if(VALUE_WITHIN(key, 0x59, 0x61)) {
      if(bmNumLock == 1)
        return (key - 0x59 + '1');
    } else if(VALUE_WITHIN(key, 0x2d, 0x38))
      return ((shift) ? (uint8_t)pgm_read_byte(&getSymKeysUp()[key - 0x2d]) : (uint8_t)pgm_read_byte(&getSymKeysLo()[key - 0x2d]));
    else if(VALUE_WITHIN(key, 0x54, 0x58))
      return (uint8_t)pgm_read_byte(&getPadKeys()[key - 0x54]);
    else {
      switch(key) {
        case UHS_HID_BOOT_KEY_SPACE: return (0x20);
        case UHS_HID_BOOT_KEY_ENTER: return ('\r'); // Carriage return (0x0D)
        case UHS_HID_BOOT_KEY_ZERO2: return ((bmNumLock == 1) ? '0': 0);
        case UHS_HID_BOOT_KEY_PERIOD: return ((bmNumLock == 1) ? '.': 0);
      }
    }
    return ( 0);
  }

#endif
