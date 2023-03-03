#include "HID_Serial.hpp"

#include <Wire.h>

static bool serial_begun = false;


bool SerialReady()
{
  return serial_begun;
}

void initSerial()
{
  if( serial_begun ) {
    return;
  }
  if( !Serial.available() ) {
    //Serial.begin(115200);
  }
  //Wire.begin( SDA, SCL ); // connect SerialShield
  serial_begun = true;
  Serial.flush();

}


void beforeSerialRead()
{
  // don't spam I2C
}

void afterSerialRead()
{
  // Turn I2C on
}


static unsigned long serialMinDelay = 30; // min delay between queryes
static unsigned long serialLastQuery = 0;
char lastSerialLine[255];


HIDControls readSerial()
{
  unsigned long now = millis();
  if( now - serialLastQuery > serialMinDelay ) {
    serialLastQuery = now;
  } else {
    return HID_NONE;
  }
  if( !Serial.available() ) return HID_NONE;
  //Wire.requestFrom(0x10, 1); // scan buttons from SerialShield
  int resp = Serial.read();
  switch(resp) {
    case 's': // down
      return HID_UP;
    break;
    case 'w': // up
      return HID_DOWN;
    break;
    case 'd': // right
      return HID_RIGHT;
    break;
    case 'a': // left
      return HID_LEFT;
    break;
    case ' ': // B (play/stop button)
      return HID_PLAYSTOP;
    break;
    case '1': // Action button
      return HID_ACTION1;
    break;
    case '2': // C
      return HID_ACTION2;
    break;
    case '3': // D
      return HID_ACTION3;
    break;
    case '?':
      return HID_SEARCH;
    break;
    case 'y':
      return HID_PROMPT_Y;
    break;
    case 'n':
      return HID_PROMPT_N;
    break;
    case 'z': // zzzz
      return HID_SLEEP;
    break;
    default: // simultaneous buttons push ?
      return HID_NONE;
    break;
  }
}
