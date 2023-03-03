
#include "./PartitionManager.hpp"

#if defined SD_UPDATABLE

  #include "../../UI/UI_Utils.hpp"


  namespace PartitionManager
  {

    using namespace UI_Utils;
    using SIDView::display;

    /*
    static esp_image_metadata_t getSketchMeta( const esp_partition_t* running ) {
      esp_image_metadata_t data;
      if ( !running ) return data;
      const esp_partition_pos_t running_pos  = {
        .offset = running->address,
        .size = running->size,
      };
      data.start_addr = running_pos.offset;
      esp_image_verify( ESP_IMAGE_VERIFY, &running_pos, &data );
      return data;
    }
    */

    bool comparePartition(const esp_partition_t* src1, const esp_partition_t* src2, size_t length)
    {
      size_t lengthLeft = length;
      const size_t bufSize = SPI_FLASH_SEC_SIZE;
      std::unique_ptr<uint8_t[]> buf1(new uint8_t[bufSize]);
      std::unique_ptr<uint8_t[]> buf2(new uint8_t[bufSize]);
      uint32_t offset = 0;
      size_t i;
      while( lengthLeft > 0) {
        size_t readBytes = (lengthLeft < bufSize) ? lengthLeft : bufSize;
        if (!ESP.flashRead(src1->address + offset, reinterpret_cast<uint32_t*>(buf1.get()), (readBytes + 3) & ~3)
        || !ESP.flashRead(src2->address + offset, reinterpret_cast<uint32_t*>(buf2.get()), (readBytes + 3) & ~3)) {
            return false;
        }
        for (i = 0; i < readBytes; ++i) if (buf1[i] != buf2[i]) return false;
        lengthLeft -= readBytes;
        offset += readBytes;
      }
      return true;
    }


    bool copyPartition(File* fs, const esp_partition_t* dst, const esp_partition_t* src, size_t length)
    {
      display->fillRect( 0, display->height()-10, display->width(), 10, 0);
      size_t lengthLeft = length;
      const size_t bufSize = SPI_FLASH_SEC_SIZE;
      std::unique_ptr<uint8_t[]> buf(new uint8_t[bufSize]);
      uint32_t offset = 0;
      uint32_t progress = 0, progressOld = 0;
      while( lengthLeft > 0) {
        size_t readBytes = (lengthLeft < bufSize) ? lengthLeft : bufSize;
        if (!ESP.flashRead(src->address + offset, reinterpret_cast<uint32_t*>(buf.get()), (readBytes + 3) & ~3)
        || !ESP.flashEraseSector((dst->address + offset) / bufSize)
        || !ESP.flashWrite(dst->address + offset, reinterpret_cast<uint32_t*>(buf.get()), (readBytes + 3) & ~3)) {
            return false;
        }
        if (fs) fs->write(buf.get(), (readBytes + 3) & ~3);
        lengthLeft -= readBytes;
        offset += readBytes;
        progress = display->width() * offset / length;
        if (progressOld != progress) {
          progressOld = progress;
          //!!        display->progressBar( 110, 112, 100, 20, progress);
          display->fillRect( 0, display->height()-10, progress, 10, TFT_GREEN);
        }
      }
      return true;
    }


    void copyPartition( const char* menubinfilename )
    {
      const esp_partition_t *running = esp_ota_get_running_partition();
      const esp_partition_t *nextupdate = esp_ota_get_next_update_partition(NULL);
      size_t sksize = ESP.getSketchSize();
      bool flgSD = false;//M5_FS.begin( TFCARD_CS_PIN, SPI, 40000000 );
      File dst;
      if (flgSD) {
        dst = (SID_FS.open(menubinfilename, FILE_WRITE ));
        display->drawString("Overwriting " MENU_BIN, 16, 38, &Font8x8C64);
      }
      if (copyPartition( flgSD ? &dst : NULL, nextupdate, running, sksize)) {
        display->drawString("Done", 16, 52, &Font8x8C64);
      }
      if (flgSD) dst.close();
    }


    void checkMenuStickyPartition( const char* menubinfilename )
    {
      if( menubinfilename == nullptr ) menubinfilename = MENU_BIN;
      const esp_partition_t *running = esp_ota_get_running_partition();
      const esp_partition_t *nextupdate = esp_ota_get_next_update_partition(NULL);
      display->setTextDatum(MC_DATUM);
      display->setCursor(0,0);
      if (!nextupdate) {
        // display->setTextFont(4);
        display->print("! WARNING !\r\nNo OTA partition.\r\nCan't use SD-Updater.");
        delay(3000);
      } else if (running && running->label[3] == '0' && nextupdate->label[3] == '1') {
        display->drawString("TobozoLauncher on app0", 16, 10, &Font8x8C64);
        size_t sksize = ESP.getSketchSize();
        if (!comparePartition(running, nextupdate, sksize)) {
          bool flgSD = SID_FS.begin(/* TFCARD_CS_PIN, SPI, 40000000 */);
          display->drawString("Synchronizing 'app1' partition", 16, 24, &Font8x8C64);
          File dst;
          if (flgSD) {
            dst = (SID_FS.open(menubinfilename, FILE_WRITE ));
            display->drawString("Overwriting " MENU_BIN, 16, 38, &Font8x8C64);
          }
          if (copyPartition( flgSD ? &dst : NULL, nextupdate, running, sksize)) {
            display->drawString("Done", 16, 52, &Font8x8C64);
          }
          if (flgSD) dst.close();
        }
        SDUpdater::updateNVS();
        display->drawString("Hot-loading 'app1' partition", 16, 66, &Font8x8C64);
        if (Update.canRollBack()) {
          Update.rollBack();
          ESP.restart();
        } else {
          display->print("! WARNING !\r\nUpdate.rollBack() failed.");
          log_e("Failed to rollback after copy");
        }
      }
      display->setTextDatum(TL_DATUM);
    }

  };

#endif // defined SD_UPDATABLE
