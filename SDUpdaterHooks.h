
#include "PartitionManager.h"

// SD Updater customizations

static int myAssertStartUpdateFromButton( char* labelLoad, char* labelSkip, unsigned long waitdelay )
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
    drawSDUMessage();
    drawSDUPushButton( labelLoad, 0, userBtnStyle->Load.BorderColor, userBtnStyle->Load.FillColor, userBtnStyle->Load.TextColor );
    drawSDUPushButton( labelSkip, 1, userBtnStyle->Skip.BorderColor, userBtnStyle->Skip.FillColor, userBtnStyle->Skip.TextColor );
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
  //myBtnStyle.Load = { TFT_ORANGE,                      tft.color565( 0xaa, 0x00, 0x00), tft.color565( 0xdd, 0xdd, 0xdd) };
  //myBtnStyle.Skip = { tft.color565( 0x11, 0x11, 0x11), tft.color565( 0x33, 0x88, 0x33), tft.color565( 0xee, 0xee, 0xee) };
  myBtnStyle.height          = 28;
  myBtnStyle.width           = 60;
  myBtnStyle.hwidth          = 30;
  myBtnStyle.FontSize        = 1; // buttons font size
  myBtnStyle.MsgFontSize     = 1; // welcome message font size
  //myBtnStyle.MsgFontFolor[0] = TFT_WHITE;
  //myBtnStyle.MsgFontFolor[1] = TFT_BLACK; // foreground, background
  userBtnStyle = &myBtnStyle;

  // display buttons vertically on portrait mode

  SDUButtonsXOffset[0] = 34;
  SDUButtonsXOffset[1] = 34;
  SDUButtonsXOffset[2] = 34;

  SDUButtonsYOffset[0] = 0;
  SDUButtonsYOffset[1] = 40;
  SDUButtonsYOffset[2] = 80;

};

