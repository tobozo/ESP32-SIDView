#include <Wire.h>
#include "HID_Common.h"


static bool wire_begun = false;

void initXPad()
{
  if( wire_begun ) {
    return;
  }
  Wire.begin( SDA, SCL ); // connect XPadShield
  wire_begun = true;
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


int readXpad()
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
    case 0x01: // down
      return HID_UP;
    break;
    case 0x02: // up
      return HID_DOWN;
    break;
    case 0x04: // right
      return HID_RIGHT;
    break;
    case 0x08: // left
      return HID_LEFT;
    break;
    break;
    case 0x10: // B (play/stop button)
      return HID_PLAYSTOP;
    break;
    case 0x20: // Action button
      return HID_ACTION1;
    break;
    case 0x40: // C
      return HID_ACTION2;
    break;
    case 0x80: // D
      return HID_ACTION3;
    break;
    default: // simultaneous buttons push ?
      return HID_NONE;
    break;
  }
}
