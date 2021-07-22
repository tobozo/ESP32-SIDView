#include <Wire.h>
#include "HID_Common.h"


static bool touch_begun = false;

void initTouch()
{
  if( touch_begun ) {
    return;
  }
  if (tft.touch()) {
    log_i("Touch interface detected");
    /*
    tft.drawString("touch the arrow marker.", 0, tft.height()>>1);
    tft.calibrateTouch(nullptr, 0xFFFFFFU, 0x000000U, 30);
    std::int32_t x=-1, y=-1, number = 0;
    while (tft.getTouch(&x, &y, number))  // getTouch関数でタッチ中の座標を取得できます。
    {
      tft.fillCircle(x, y, 5, (std::uint32_t)(number * 0x333333u));
      tft.display();
      ++number;
    }
    tft.clear();
    */
  }
  touch_begun = true;
}


void beforeTouchRead()
{
  // don't spam I2C
}

void afterTouchRead()
{
  // Turn I2C on
}


static unsigned long touchMinDelay = 30; // min delay between queryes
static unsigned long touchLastQuery = 0;

std::int32_t lastx, lasty, lastnumber = 0;


#define BUTTON_WIDTH 60
#define BUTTON_HWIDTH BUTTON_WIDTH/2 // 30
#define BUTTON_HEIGHT 28
//#define rgb888to565 lgfx::convert_rgb888_to_rgb565

struct TouchButtonWrapper
{
  bool iconRendered = false;
  TFT_eSprite *buttonsSprite;
  int16_t spritePosX, spritePosY;

  void handlePressed( TouchButton *btn, bool pressed, int16_t x, int16_t y)
  {
    if (pressed && btn->contains(x, y)) {
      log_v("Press at [%d:%d]", x, y );
      btn->press(true); // tell the button it is pressed
    } else {
      if( pressed ) {
        log_v("Outside Press at [%d:%d]", x, y );
      }
      btn->press(false); // tell the button it is NOT pressed
    }
  }

  void handleJustPressed( TouchButton *btn, const char* label )
  {
    if ( btn->justPressed() ) {
      btn->drawButton(true, label);
      draw();
      //pushIcon( label );
    }
  }

  bool justReleased( TouchButton *btn, bool pressed, const char* label )
  {
    bool ret = false;
    if ( btn->justReleased() && (!pressed)) {
      // callable
      ret = true;
    } else if ( btn->justReleased() && (pressed)) {
      // state change but not callable
      ret = false;
    } else {
      // no change, no need to draw
      return false;
    }
    btn->drawButton(false, label);
    draw();
    //pushIcon( label );
    return ret;
  }

  void draw()
  {
    buttonsSprite->pushSprite( spritePosX, spritePosY );
  }

}; // end struct TouchButtonWrapper



typedef struct
{
  TouchButton *button;
  const char* label;
  HIDControls action;
  int16_t x,y,w,h;
  TouchButton::drawCb cb;
} TouchButtonAction_t;


static void shapeTriangleUpCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label )
{
  uint_fast8_t r = (w < h ? w : h) >> 2;
  _gfx->fillTriangle( x+w/2, r+y, r+x, y+h-r, x+w-r, y+h-r, invert ? C64_DARKBLUE : C64_LIGHTBLUE );
  if( invert ) {
    _gfx->drawTriangle( x+w/2, r+y, r+x, y+h-r, x+w-r, y+h-r, C64_LIGHTBLUE );
  }
  _gfx->drawRoundRect( x, y, w, h, r, C64_LIGHTBLUE );
}

static void shapeTriangleDownCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label )
{
  uint_fast8_t r = (w < h ? w : h) >> 2;
  _gfx->fillTriangle( r+x, r+y, x+w-r, r+y, x+w/2, y+h-r, invert ? C64_DARKBLUE : C64_LIGHTBLUE );
  if( invert ) {
    _gfx->drawTriangle( r+x, r+y, x+w-r, r+y, x+w/2, y+h-r, C64_LIGHTBLUE);
  }
  _gfx->drawRoundRect( x, y, w, h, r, C64_LIGHTBLUE );
}

static void shapeTrianglePrevCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label )
{
  uint_fast8_t r = (w < h ? w : h) >> 2;
  _gfx->fillTriangle( x+w-r-1, y+h-r, x+w-r-1, y+r, r*2+x-2, y+h/2, invert ? C64_DARKBLUE : C64_LIGHTBLUE );
  _gfx->fillRect( r+x+1, y+r+1, r-1, h-r*2, invert ? C64_DARKBLUE : C64_LIGHTBLUE );
  if( invert ) {
    _gfx->drawTriangle( x+w-r-1, y+h-r, x+w-r-1, y+r, r*2+x-2, y+h/2, C64_LIGHTBLUE );
    _gfx->drawRect( r+x+1, y+r+1, r-1, h-r*2, C64_LIGHTBLUE );
  }
  _gfx->drawRoundRect( x, y, w, h, r, C64_LIGHTBLUE );
}

static void shapeTriangleNextCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label )
{
  uint_fast8_t r = (w < h ? w : h) >> 2;
  _gfx->fillTriangle( x+r, y+r, x+w-r*2, y+h/2, x+r, y+h-r, invert ? C64_DARKBLUE : C64_LIGHTBLUE );

  _gfx->fillRect( x+w-r*2, y+r+1, r-1, h-r*2, invert ? C64_DARKBLUE : C64_LIGHTBLUE );
  if( invert ) {
    _gfx->drawRect( x+w-r*2, y+r+1, r-1, h-r*2, C64_LIGHTBLUE );
    _gfx->drawTriangle( x+r, y+r, x+w-r*2, y+h/2, x+r, y+h-r, C64_LIGHTBLUE );
  }
  _gfx->drawRoundRect( x, y, w, h, r, C64_LIGHTBLUE );
}

static void shapeTriangleRightCb( LovyanGFX *_gfx, int32_t x, int32_t y, int32_t w, int32_t h, bool invert, const char* label )
{
  uint_fast8_t r = (w < h ? w : h) >> 2;
  _gfx->fillRoundRect( x, y, w, h, r, C64_DARKBLUE );
  if( sidPlayer->isPlaying() ) {
    _gfx->fillRoundRect( x+r, y+r, w-r*2, h-r*2, r/2, invert ? C64_DARKBLUE : C64_LIGHTBLUE );
    if( invert )
      _gfx->drawRoundRect( x+r, y+r, w-r*2, h-r*2, r/2, C64_LIGHTBLUE );
  } else {
    _gfx->fillTriangle( x+r, y+r, x+w-r, y+h/2, x+r, y+h-r, invert ? C64_DARKBLUE : C64_LIGHTBLUE );
    if( invert )
      _gfx->drawTriangle( x+r, y+r, x+w-r, y+h/2, x+r, y+h-r, C64_LIGHTBLUE);
  }
  _gfx->drawRoundRect( x, y, w, h, r, C64_LIGHTBLUE );
}



static TouchButtonAction_t UIBtns[8] = {
  { nullptr, "play",  HID_PLAYSTOP, 17,  241, 30, 24, &shapeTriangleRightCb },
  { nullptr, "up",    HID_UP,       49,  241, 30, 24, &shapeTriangleUpCb },
  { nullptr, "down",  HID_DOWN,     81,  241, 30, 24, &shapeTriangleDownCb },
  { nullptr, "left",  HID_LEFT,     113, 241, 30, 24, &shapeTrianglePrevCb },
  { nullptr, "right", HID_RIGHT,    145, 241, 30, 24, &shapeTriangleNextCb },
  { nullptr, "!",     HID_ACTION1,  91,  273, 58, 30, nullptr },
  { nullptr, "+",     HID_ACTION3,  151, 273, 58, 30, nullptr },
  { nullptr, "-",     HID_ACTION2,  211, 273, 58, 30, nullptr },
};


//static TouchButton *UIButtons[8] = {nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
//const char* UILabels[8] = { "up", "down", "left", "right", "play", "+", "-", "!" };
//HIDControls HIDActions[8] = { HID_UP, HID_DOWN, HID_LEFT, HID_RIGHT, HID_PLAYSTOP, HID_ACTION1, HID_ACTION2, HID_ACTION3 };
static TouchButtonWrapper tbWrapper;
static int spriteOffset = 0;

void setupTouchButtons( TFT_eSprite *sprite, int x, int y, int w, int h, int xoffset, int yoffset )
{
  tbWrapper.buttonsSprite = sprite;
  tbWrapper.spritePosX = xoffset;
  tbWrapper.spritePosY = yoffset;

  spriteOffset = yoffset;

  int fontSize = 1,
      marginx  = 1,
      marginy  = 1,
      padx     = 2, // buttons padding
      pady     = 2,
      rw       = (w/4),
      rh       = (h/2),
      bw       = (w/4)-marginx*2,
      bh       = (h/2)-marginy*2;

  for( int i=0;i<8;i++ ) {
    UIBtns[i].button = new TouchButton();
    UIBtns[i].button->initButton(
        sprite,
        //rw/2+( (i%4)*(rw) )+marginx, rh/2+(i<4?y:y+rh)+marginy, bw, bh, // x,y,w,h
        UIBtns[i].x, UIBtns[i].y, UIBtns[i].w, UIBtns[i].h,
        C64_DARKBLUE, C64_LIGHTBLUE, C64_DARKBLUE, // outline, fill, text
        (char*)UIBtns[i].label, fontSize
    );
    if( UIBtns[i].cb != nullptr ) {
      UIBtns[i].button->setDrawCb( UIBtns[i].cb );
    }
    UIBtns[i].button->setLabelDatum(0, 0, MC_DATUM);
    UIBtns[i].button->press(false);
  }
}


void drawTouchButtons()
{
  for( int i=0;i<8;i++ ) {
    UIBtns[i].button->drawButton();
  }
}

void releaseTouchButtons()
{
  for( int i=0;i<8;i++ ) {
    UIBtns[i].button->press(false);
  }
}


static std::int32_t lasttouchx=-1, lasttouchy=-1;

int readTouch()
{

  unsigned long now = millis();
  if( now - touchLastQuery > touchMinDelay ) {
    touchLastQuery = now;
  } else {
    return HID_NONE;
  }

  //std::int32_t x=-1, y=-1;
  uint8_t number = 0;
  lgfx::touch_point_t tp;

  takeSidMuxSemaphore();
  number = tft.getTouch(&tp, 1);
  bool pressed = number > 0; //tft.getTouch(&x, &y, number);
  giveSidMuxSemaphore();

  if(!pressed) return HID_NONE;

  unsigned long lasttouch = pressed ? millis() : 0;
  unsigned long waitmax = 100; // ms
  int released_button = -1;
  int return_action = HID_NONE;

  while (pressed || lasttouch+waitmax > millis() ) {
    //tft.fillCircle( x, y, 4, TFT_ORANGE );
    tp.y-= spriteOffset; // translate to sprite position

    if( tp.x!=-1) lasttouchx = tp.x;
    if( tp.y!= -(spriteOffset+1)) lasttouchy = tp.y;

    takeSidMuxSemaphore();
    for( int i=0;i<8;i++ ) {
      tbWrapper.handlePressed( UIBtns[i].button, pressed, tp.x, tp.y );
    }
    for( int i=0;i<8;i++ ) {
      tbWrapper.handleJustPressed( UIBtns[i].button, (char*)UIBtns[i].label );
    }
    for( int i=0;i<8;i++ ) {
      if( tbWrapper.justReleased( UIBtns[i].button, pressed, (char*)UIBtns[i].label ) ) {
        released_button = i;
      }
    }
    if( released_button > -1 ) {
      giveSidMuxSemaphore();
      return_action = UIBtns[released_button].action;
      break;
    }

    //pressed = tft.getTouch(&x, &y, number);
    number = tft.getTouch(&tp, 1);
    pressed = number > 0;
    giveSidMuxSemaphore();
    if( pressed ) {
      lasttouch = millis();
    }
    vTaskDelay(1);
  }

  if( return_action == HID_NONE )  {
    log_d("Touch coords are outside any know touch zone (%d events, x=%d, y=%d, lastx=%d, lasty=%d)", number, tp.x, tp.y, lasttouchx, lasttouchy);
    releaseTouchButtons();
  } else {
    log_d("Firing action %d (%s)", return_action, UIBtns[released_button].label );
  }
  drawTouchButtons();

  return return_action;

  /*

  log_w("got %d touch signals, last coords = [%d,%d]", number, x, y );


  //Wire.requestFrom(0x10, 1); // scan buttons from XPadShield
  int resp = HID_NONE; // touchKeyboard.getLastKeyPressed()

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
  }*/
}

