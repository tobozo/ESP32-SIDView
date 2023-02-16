#pragma once

#include "../../config.h"
//#include <Wire.h>

enum HIDControls
{
  HID_NONE     =  0,
  HID_DOWN     =  1,  // move down selection
  HID_UP       =  2,  // move up selection
  HID_LEFT     =  3,  // prev song/track
  HID_RIGHT    =  4,  // next song/track
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

static constexpr const char* HIDControlStr[] =
{
  "NONE"     ,
  "DOWN"     , // move down selection
  "UP"       , // move up selection
  "LEFT"     , // prev song/track
  "RIGHT"    , // next song/track
  "PLAYSTOP" , // toggle selected
  "SEARCH"   , // search mode requires a real keyboard (TODO: SerialHIDControls)
  "ACTION1"  , // toggle loop mode
  "ACTION2"  , // volume +
  "ACTION3"  , // volume -
  "ACTION4"  , // backspace
  "ACTION5"  , // escape
  "PROMPT_Y" , // yes/no action
  "PROMPT_N" , // yes/no action
  "SLEEP"    , // turn ESP off
};

struct HIDControlKey
{
  void set( HIDControls c, int k ) { control = c; key = k; };
  HIDControls control = HID_NONE;
  int key = -1;
};

bool HIDReady();
void initHID();
HIDControls readHID();
void beforeHIDRead();
void afterHIDRead();



