#pragma once

#include "../HID_Common.hpp"

#if defined HAS_TOUCH

  bool TouchReady();
  void initTouch();
  void beforeTouchRead();
  void afterTouchRead();

  static void shapeTriangleUpCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label );
  static void shapeTriangleDownCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label );
  static void shapeTrianglePrevCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label );
  static void shapeTriangleNextCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label );
  static void shapeTriangleRightCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label );
  static void shapeLoopToggleCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label );
  static void shapePlusCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label );
  static void shapeMinusCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label );

  typedef struct
  {
    TouchButton *button;
    const char* label;
    uint8_t font_size;
    HIDControls action;
    int16_t x,y,w,h;
    TouchButton::drawCb cb;
  } TouchButtonAction_t;


  void registerButtonAction( LGFX_Sprite *sprite, TouchButtonAction_t *btna );
  void setupUIBtns( LGFX_Sprite *sprite, int x, int y, int w, int h, int xoffset, int yoffset );
  void drawTouchButtons();
  void releaseTouchButtons();
  HIDControls readTouch();

#endif
