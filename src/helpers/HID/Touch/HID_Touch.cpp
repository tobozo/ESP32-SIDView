#include "HID_Touch.hpp"

#if defined HAS_TOUCH

  #include <Wire.h>
  #include "../../../UI/UI_Utils.hpp"

  UI_Utils::TouchButtonAction_t UIBtns[8] = {
    { nullptr, "play",  1, HID_PLAYSTOP, 16,  276, 30, 24, &UI_Utils::shapeTriangleRightCb },
    { nullptr, "up",    1, HID_UP,       47,  276, 30, 24, &UI_Utils::shapeTriangleUpCb },
    { nullptr, "down",  1, HID_DOWN,     79,  276, 30, 24, &UI_Utils::shapeTriangleDownCb },
    { nullptr, "left",  1, HID_LEFT,     111, 276, 30, 24, &UI_Utils::shapeTrianglePrevCb },
    { nullptr, "right", 1, HID_RIGHT,    143, 276, 30, 24, &UI_Utils::shapeTriangleNextCb },
    { nullptr, "!",     1, HID_ACTION1,  175, 276, 30, 24, &UI_Utils::shapeLoopToggleCb },
    { nullptr, "+",     1, HID_ACTION3,  202, 276, 22, 22, &UI_Utils::shapePlusCb },
    { nullptr, "-",     1, HID_ACTION2,  227, 276, 22, 22, &UI_Utils::shapeMinusCb },
  };

  static bool touch_begun = false;

  bool TouchReady()
  {
    return touch_begun;
  }

  void initTouch()
  {
    if( touch_begun ) {
      return;
    }
    if (SIDView::display->touch()) {
      //SIDView::display->setRotation(0);
      log_i("Touch interface detected");

      {
        auto cfg = SIDView::display->touch()->config();
        cfg.offset_rotation = 0;
        SIDView::display->touch()->config(cfg);
      }


      /*
      SIDView::display->drawString("touch the arrow marker.", 0, SIDView::display->height()>>1);
      SIDView::display->calibrateTouch(nullptr, 0xFFFFFFU, 0x000000U, 30);
      std::int32_t x=-1, y=-1, number = 0;
      while (SIDView::display->getTouch(&x, &y, number))  // getTouch関数でタッチ中の座標を取得できます。
      {
        SIDView::display->fillCircle(x, y, 5, (std::uint32_t)(number * 0x333333u));
        SIDView::display->display();
        ++number;
      }
      SIDView::display->clear();
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


  //#define BUTTON_WIDTH 60
  //#define BUTTON_HWIDTH BUTTON_WIDTH/2 // 30
  //#define BUTTON_HEIGHT 28
  //#define rgb888to565 lgfx::convert_rgb888_to_rgb565

  static std::int32_t lasttouchx=-1, lasttouchy=-1;

  HIDControls readTouch()
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

    number = SIDView::display->getTouch(&tp, 1);
    bool pressed = number > 0; //SIDView::display->getTouch(&x, &y, number);

    if(!pressed) return HID_NONE;

    unsigned long lasttouch = pressed ? millis() : 0;
    unsigned long waitmax = 100; // ms
    int released_button = -1;
    HIDControls return_action = HID_NONE;

    while (pressed || lasttouch+waitmax > millis() ) {
      //SIDView::display->fillCircle( x, y, 4, TFT_ORANGE );
      tp.y-= UI_Utils::touchSpriteOffset; // translate to sprite position

      if( tp.x!=-1) lasttouchx = tp.x;
      if( tp.y!= -(UI_Utils::touchSpriteOffset+1)) lasttouchy = tp.y;

      for( int i=0;i<8;i++ ) {
        UI_Utils::tbWrapper.handlePressed( UIBtns[i].button, pressed, tp.x, tp.y );
      }
      for( int i=0;i<8;i++ ) {
        UI_Utils::tbWrapper.handleJustPressed( UIBtns[i].button, (char*)UIBtns[i].label );
      }
      for( int i=0;i<8;i++ ) {
        if( UI_Utils::tbWrapper.justReleased( UIBtns[i].button, pressed, (char*)UIBtns[i].label ) ) {
          released_button = i;
        }
      }
      if( released_button > -1 ) {
        return_action = UIBtns[released_button].action;
        break;
      }

      //pressed = SIDView::display->getTouch(&x, &y, number);
      number = SIDView::display->getTouch(&tp, 1);
      pressed = number > 0;
      if( pressed ) {
        lasttouch = millis();
      }
      vTaskDelay(1);
    }

    if( return_action == HID_NONE )  {
      log_d("Touch coords are outside any know touch zone (%d events, x=%d, y=%d, lastx=%d, lasty=%d)", number, tp.x, tp.y, lasttouchx, lasttouchy);
      UI_Utils::releaseTouchButtons();
    } else {
      log_d("Firing action %d (%s)", return_action, UIBtns[released_button].label );
    }
    UI_Utils::drawTouchButtons();

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

#endif
