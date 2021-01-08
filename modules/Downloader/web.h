/*\

  ESP32-SIDView
  https://github.com/tobozo/ESP32-SIDView

  MIT License

  Copyright (c) 2020 tobozo

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  -----------------------------------------------------------------------------

\*/

#ifndef _SID_HVSC_DOWNLOADER_H_
#define _SID_HVSC_DOWNLOADER_H_

//#include <WiFi.h>
//#include <WiFiClient.h>
//#include <WebServer.h>
//#include <ESPmDNS.h>
//#include <WiFiUdp.h>
//#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

HTTPClient http; // this sucks ~40% of program storage space

bool webServerRunning = false;
bool wifiConnected = false;

/*
void OTASetup()
{

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("esp32-SID6581");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  }).onEnd([]() {
    Serial.println("\nEnd");
  }).onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  }).onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  Serial.println("MDNS responder started: esp32-msgeq7.local");

  //   tft.setCursor(0, 0);
  //   tft.print( WiFi.localIP() );
  //   tft.setCursor(0, 16);
  //   tft.setTextSize(1);
  //   tft.print("http://esp32-sid6581.local");
}
*/

/*\
 * Because ESP32 WiFi can't reconnect by itself (bug)
\*/
void stubbornConnect()
{
  uint8_t wifi_retry_count = 0;
  uint8_t max_retries = 10;
  unsigned long stubbornness_factor = 3000; // ms to wait between attempts

  #ifdef ESP32
    while (WiFi.status() != WL_CONNECTED && wifi_retry_count < max_retries)
  #else
    while (WiFi.waitForConnectResult() != WL_CONNECTED && wifi_retry_count < max_retries)
  #endif
  {
    #if defined WIFI_SSID && defined WIFI_PASS
      WiFi.begin( WIFI_SSID, WIFI_PASS ); // put your ssid / pass if required, only needed once
    #else
      WiFi.begin();
    #endif
    Serial.print(WiFi.macAddress());
    Serial.printf(" => WiFi connect - Attempt No. %d\n", wifi_retry_count+1);
    delay( stubbornness_factor );
    wifi_retry_count++;
  }

  if(wifi_retry_count >= max_retries ) {
    Serial.println("no connection, forcing restart");
    ESP.restart();
  }

  if (WiFi.waitForConnectResult() == WL_CONNECTED){
    Serial.println("Connected as");
    Serial.println(WiFi.localIP());
    wifiConnected = true;
  }

}


void wifiOff()
{
  WiFi.mode( WIFI_OFF );
  wifiConnected = false;
  //tft.fillRect(0, 0, TFT_WIDTH, 32, TFT_BLACK );
}



static void (*PrintProgressBar)( float progress, float max );


static bool /*yolo*/wget( const char* url, fs::FS &fs, const char* path )
{
  WiFiClientSecure *client = new WiFiClientSecure;
  client->setCACert( NULL ); // yolo security

  const char* UserAgent = "ESP32HTTPClient";

  http.setUserAgent( UserAgent );
  http.setConnectTimeout( 10000 ); // 10s timeout = 10000

  if( ! http.begin(*client, url ) ) {
    log_e("Can't open url %s", url );
    return false;
  }

  const char * headerKeys[] = {"location", "redirect"};
  const size_t numberOfHeaders = 2;
  http.collectHeaders(headerKeys, numberOfHeaders);

  log_w("URL = %s", url);

  int httpCode = http.GET();

  // file found at server
  if (httpCode == HTTP_CODE_FOUND || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
    String newlocation = "";
    for(int i = 0; i< http.headers(); i++) {
      String headerContent = http.header(i);
      if( headerContent !="" ) {
        newlocation = headerContent;
        Serial.printf("%s: %s\n", headerKeys[i], headerContent.c_str());
      }
    }

    http.end();
    if( newlocation != "" ) {
      log_w("Found 302/301 location header: %s", newlocation.c_str() );
      // delete client;
      return wget( newlocation.c_str(), fs, path );
    } else {
      log_e("Empty redirect !!");
      return false;
    }
  }

  WiFiClient *stream = http.getStreamPtr();

  if( stream == nullptr ) {
    http.end();
    log_e("Connection failed!");
    return false;
  }

  File outFile = fs.open( path, FILE_WRITE );
  if( ! outFile ) {
    log_e("Can't open %s file to save url %s", path, url );
    return false;
  }

  size_t sizeOfBuff = 4096;
  uint8_t *buff = new uint8_t[sizeOfBuff];//

  int len = http.getSize();
  int bytesLeftToDownload = len;
  int bytesDownloaded = 0;

  if( PrintProgressBar ) {
    PrintProgressBar( 0, 100.0 );
  }

  while(http.connected() && (len > 0 || len == -1)) {
    size_t size = stream->available();
    if(size) {
      // read up to 512 byte
      int c = stream->readBytes(buff, ((size > sizeOfBuff) ? sizeOfBuff : size));
      outFile.write( buff, c );
      bytesLeftToDownload -= c;
      bytesDownloaded += c;
      if( bytesLeftToDownload == 0 ) break;
      float progress = (((float)bytesDownloaded / (float)len) * 100.00);
      if( PrintProgressBar ) {
        PrintProgressBar( progress, 100.0 );
        //log_w("%d bytes left", bytesLeftToDownload );
      }
    }
  }
  if( PrintProgressBar ) {
    PrintProgressBar( 100.0, 100.0 );
  }
  outFile.close();
  delete buff;
  delete client;
  return fs.exists( path );
}


#endif
