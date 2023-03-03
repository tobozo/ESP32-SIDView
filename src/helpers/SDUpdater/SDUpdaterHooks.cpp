
#include "./SDUpdaterHooks.hpp"

#if defined SD_UPDATABLE

  #include "../../UI/UI_Utils.hpp"
  #include "PartitionManager.hpp"

  namespace SDUpdaterNS
  {

    using namespace PartitionManager;
    using SIDView::display;

    // SD Updater customizations
    int myAssertStartUpdateFromButton( char* labelLoad,  char* labelSkip, char* labelSave, unsigned long waitdelay )
    {
      static unsigned long last_poll = 0;
      static bool i2c_connected = false;
      if( millis() - last_poll < 50 ) return -1;
      last_poll = millis();
      if( !i2c_connected ) {
        log_w("Starting I2C");
        i2c_connected = true;
        Wire.begin( SDA, SCL ); // connect XPadShield
        freezeTextStyle();
        display->setFont( &Font8x8C64 );
        drawSDUSplashPage(BTN_HINT_MSG);
        display->setFont( &Font8x8C64 );
        drawSDUPushButton( labelLoad, 0, userBtnStyle->Load.BorderColor, userBtnStyle->Load.FillColor, userBtnStyle->Load.TextColor, userBtnStyle->Load.ShadowColor );
        display->setFont( &Font8x8C64 );
        drawSDUPushButton( labelSkip, 1, userBtnStyle->Skip.BorderColor, userBtnStyle->Skip.FillColor, userBtnStyle->Skip.TextColor, userBtnStyle->Load.ShadowColor );
      }
      auto msec = millis();
      do {
        Wire.requestFrom(0x10, 1); // scan buttons from XPadShield
        int resp = Wire.read();
        if( resp == 0x01 ) return 1;
        if( resp == 0x04 ) return 0;
        delay(50);
      } while (millis() - msec < waitdelay);
      return -1;
    }

    void setupButtonsStyle()
    {
      static BtnStyles myBtnStyle;
      // overwrite default properties if needed
      //myBtnStyle.Load = { TFT_ORANGE,                      display->color565( 0xaa, 0x00, 0x00), display->color565( 0xdd, 0xdd, 0xdd) };
      //myBtnStyle.Skip = { display->color565( 0x11, 0x11, 0x11), display->color565( 0x33, 0x88, 0x33), display->color565( 0xee, 0xee, 0xee) };
      myBtnStyle.height          = 28;
      myBtnStyle.width           = 60;
      myBtnStyle.hwidth          = 30;
      myBtnStyle.FontSize        = 0; // buttons font size
      myBtnStyle.MsgFontSize     = 0; // welcome message font size
      //myBtnStyle.MsgFontFolor[0] = TFT_WHITE;
      //myBtnStyle.MsgFontFolor[1] = TFT_BLACK; // foreground, background
      userBtnStyle = &myBtnStyle;

      // display buttons vertically on portrait mode
      /*
      SplashTitleStyle =
      {
        .textColor  = C64_MAROON_565, // C64_LIGHTBLUE_565,
        .bgColor    = C64_MAROON_DARKER_565,
        .fontSize   = 0,
        .textDatum  = MC_DATUM,
        .colorStart = C64_LIGHTBLUE_565,  // gradient color start
        .colorEnd   = C64_DARKBLUE_565,   // gradient color end
      };
      */
      SplashTitleStyle.fontSize   = 0;

      SplashAppNameStyle =
      {
        .textColor  = C64_LIGHTBLUE_565,
        .bgColor    = C64_DARKBLUE_565,
        .fontSize   = 0,
        .textDatum  = BC_DATUM,
        .colorStart = C64_DARKBLUE_565,  // gradient color start
        .colorEnd   = 0,   // gradient color end
      };

      SplashAuthorNameStyle =
      {
        .textColor  = C64_LIGHTBLUE_565,
        .bgColor    = C64_DARKBLUE_565,
        .fontSize   = 0,
        .textDatum  = BC_DATUM,
        .colorStart = C64_DARKBLUE_565,  // gradient color start
        .colorEnd   = 0,   // gradient color end
      };

      SplashAppPathStyle    =
      {
        .textColor  = TFT_DARKGREY,
        .bgColor    = C64_DARKBLUE_565,
        .fontSize   = 0,
        .textDatum  = BC_DATUM,
        .colorStart = C64_DARKBLUE_565,  // gradient color start
        .colorEnd   = 0,   // gradient color end
      };


      BUTTON_WIDTH = 36;
      BUTTON_HWIDTH = BUTTON_WIDTH/2; // button half width
      BUTTON_HEIGHT = 32;

      SDUButtonsXOffset[0] = 34;
      SDUButtonsXOffset[1] = 34;
      SDUButtonsXOffset[2] = 34;

      SDUButtonsYOffset[0] = 0;
      SDUButtonsYOffset[1] = 36;
      SDUButtonsYOffset[2] = 72;

      display->setFont( &Font8x8C64 );

    }

    void runSDUpdaterMenu()
    {
      setupButtonsStyle(); // set styles first (default it 320x240 landscape)
      SDUCfg.setWaitForActionCb( myAssertStartUpdateFromButton );// use my own buttons combination at boot
      checkMenuStickyPartition(); // copy self to SD Card as "/SIDPlayer.bin", also move to OTA1 partition
      checkSDUpdater( SID_FS, MENU_BIN, 1000 ); // on button push => load the "/menu.bin" firmware
      M5.sd_begin();
      delay(300);
    }

  };



#endif // defined SD_UPDATABLE
