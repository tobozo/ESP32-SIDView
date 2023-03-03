#pragma once

#include "../HID_Common.hpp"

#if defined HID_USB

  #include "HID_USB_Keys.h"

  // reversed from https://github.com/felis/USB_Host_Shield_2.0
  // /!\ this is limited to only one device (keyboard)

  #define UHS_HID_BOOT_KEY_ZERO           0x27
  #define UHS_HID_BOOT_KEY_ENTER          0x28
  #define UHS_HID_BOOT_KEY_SPACE          0x2c
  #define UHS_HID_BOOT_KEY_CAPS_LOCK      0x39
  #define UHS_HID_BOOT_KEY_SCROLL_LOCK    0x47
  #define UHS_HID_BOOT_KEY_NUM_LOCK       0x53
  #define UHS_HID_BOOT_KEY_ZERO2          0x62
  #define UHS_HID_BOOT_KEY_PERIOD         0x63

  #define VALUE_BETWEEN(v,l,h) (((v)>(l)) && ((v)<(h)))
  #define VALUE_WITHIN(v,l,h) (((v)>=(l)) && ((v)<=(h)))

  struct MODIFIERKEYS
  {
    uint8_t bmLeftCtrl : 1;
    uint8_t bmLeftShift : 1;
    uint8_t bmLeftAlt : 1;
    uint8_t bmLeftGUI : 1;
    uint8_t bmRightCtrl : 1;
    uint8_t bmRightShift : 1;
    uint8_t bmRightAlt : 1;
    uint8_t bmRightGUI : 1;
  };

  struct KBDINFO
  {
    struct
    {
      uint8_t bmLeftCtrl : 1;
      uint8_t bmLeftShift : 1;
      uint8_t bmLeftAlt : 1;
      uint8_t bmLeftGUI : 1;
      uint8_t bmRightCtrl : 1;
      uint8_t bmRightShift : 1;
      uint8_t bmRightAlt : 1;
      uint8_t bmRightGUI : 1;
    };
    uint8_t bReserved;
    uint8_t Keys[6];
  };


  struct KBDLEDS
  {
    uint8_t bmNumLock : 1;
    uint8_t bmCapsLock : 1;
    uint8_t bmScrollLock : 1;
    uint8_t bmCompose : 1;
    uint8_t bmKana : 1;
    uint8_t bmReserved : 3;
  };


  class KeyboardReportParser
  {
    static const uint8_t numKeys[10];
    static const uint8_t symKeysUp[12];
    static const uint8_t symKeysLo[12];
    static const uint8_t padKeys[5];

    protected:

      union
      {
        KBDINFO kbdInfo;
        uint8_t bInfo[sizeof (KBDINFO)];
      } prevState;

      union
      {
        KBDLEDS kbdLeds;
        uint8_t bLeds;
      } kbdLockingKeys;

    public:

      KeyboardReportParser()
      {
        //bLeds = 0;
        bmCapsLock   = false;
        bmNumLock    = false;
        bmScrollLock = false;
      };

      bool bmCapsLock;
      bool bmNumLock;
      bool bmScrollLock;

      uint8_t OemToAscii(uint8_t mod, uint8_t key);
      void Parse(uint8_t len, uint8_t *buf);


    protected:

      virtual uint8_t HandleLockingKeys(uint8_t key)
      {
        switch(key) {
          case UHS_HID_BOOT_KEY_NUM_LOCK:
            bmNumLock = !bmNumLock;
          break;
          case UHS_HID_BOOT_KEY_CAPS_LOCK:
            bmCapsLock = !bmCapsLock;
          break;
          case UHS_HID_BOOT_KEY_SCROLL_LOCK:
            bmScrollLock = !bmScrollLock;
          break;
        }
        return 0;
      };

      virtual void OnControlKeysChanged(uint8_t before __attribute__((unused)), uint8_t after __attribute__((unused))) { };
      virtual void OnKeyDown(uint8_t mod __attribute__((unused)), uint8_t key __attribute__((unused))) { };
      virtual void OnKeyUp(uint8_t mod __attribute__((unused)), uint8_t key __attribute__((unused))) { };
      virtual const uint8_t *getNumKeys() { return numKeys; };
      virtual const uint8_t *getSymKeysUp() { return symKeysUp; };
      virtual const uint8_t *getSymKeysLo() { return symKeysLo; };
      virtual const uint8_t *getPadKeys() { return padKeys; };

  };

#endif
