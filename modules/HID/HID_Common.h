#ifndef _HID_COMMON_H
#define _HID_COMMON_H

enum HIDControls
{
  HID_NONE     =  0,
  HID_DOWN     =  1,  // move down selection
  HID_UP       =  2,  // move up selection
  HID_RIGHT    =  3,  // next song/track
  HID_LEFT     =  4,  // prev song/track
  HID_PLAYSTOP =  5,  // toggle selected
  HID_SEARCH   =  6,  // search mode requires a real keyboard (TODO: SerialHIDControls)
  HID_ACTION1  =  7,  // toggle loop mode
  HID_ACTION2  =  8,  // volume +
  HID_ACTION3  =  9,  // volume -
  HID_ACTION4  =  10, // backspace
  HID_ACTION5  =  11, // escape
  HID_PROMPT_Y =  12, // yes/no action
  HID_PROMPT_N =  13, // yes/no action
  HID_SLEEP    =  14, // turn ESP off

};


struct HIDControlKey
{
  void set( HIDControls c, int k ) { control = c; key = k; };
  HIDControls control = HID_NONE;
  int key = -1;
};


static xSemaphoreHandle mux = NULL; // this is needed to mitigate SPI collisions
#define takeSidMuxSemaphore() if( mux ) { xSemaphoreTake(mux, portMAX_DELAY); /*log_v("Took Semaphore");*/ }
#define giveSidMuxSemaphore() if( mux ) { xSemaphoreGive(mux); /*log_v("Gave Semaphore");*/ }


extern void initXPad();
extern void HIDUSBInit();
extern void initTouch();
extern void initSerial();
extern int readXpad();
extern int readUSBHIDKeyboarD();
extern int readTouch();
extern int readSerial();
extern void beforeXpadRead();
extern void HIDBeforeRead();
extern void beforeTouchRead();
extern void beforeSerialRead();
extern void afterXpadRead();
extern void HIDAfterRead();
extern void afterTouchRead();
extern void afterSerialRead();



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

int readHID()
{
  int ret = HID_NONE;
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

#endif
