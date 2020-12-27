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

#ifndef _SID_ARCHIVE_MANAGER_H_
#define _SID_ARCHIVE_MANAGER_H_

#include "web.h"


struct PlaylistRenderer
{
  const char* name;
  bool enabled = false;
  uint8_t index;
  void draw() {
    tft.setTextDatum( MC_DATUM );
    if( enabled ) {
      tft.setTextColor( C64_DARKBLUE , C64_LIGHTBLUE );
    } else {
      tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    }
    int yPos = tft.height()/2 - tft.fontHeight()*(2+2*index);
    tft.fillRect( 0, yPos - tft.fontHeight()/2, tft.width(), tft.fontHeight()+1, C64_DARKBLUE );
    tft.drawString( name, tft.width()/2, yPos );
  }
};



static void UIPrintTitle( const char* title, const char* message = nullptr )
{
  tft.setTextDatum( MC_DATUM );
  tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
  int yPos = ( tft.height()/2 - tft.fontHeight()*4 ) + 2;
  tft.fillRect( 0, yPos - tft.fontHeight()/2, tft.width(), tft.fontHeight()+1, C64_DARKBLUE );
  tft.drawString( title, tft.width()/2, yPos );
  if( message != nullptr ) {
    yPos = ( tft.height()/2 - tft.fontHeight()*2 ) + 2;
    tft.fillRect( 0, yPos - tft.fontHeight()/2, tft.width(), tft.fontHeight()+1, C64_DARKBLUE );
    tft.drawString( message, tft.width()/2, yPos );
  }
}


static void UIPrintProgressBar( float progress, float total=100.0 )
{
  static int8_t lastprogress = -1;
  if( (uint8_t)progress != lastprogress ) {
    lastprogress = (uint8_t)progress;
    char progressStr[64] = {0};
    snprintf( progressStr, 64, "Progress: %3d%s", int(progress), "%" );
    tft.setTextDatum( MC_DATUM );
    tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    tft.drawString( progressStr, tft.width()/2, tft.height()/2 );
    //tft.setCursor( tft.height()-(tft.fontHeight()+2), 10 );
    //tft.printf("    heap: %6d    ", ESP.getFreeHeap() );
  }
}
static void UIPrintGzProgressBar( uint8_t progress )
{
  UIPrintProgressBar( (float)progress );
}

static void UIPrintTarProgress(const char* format, ...)
{
  //char buffer[256];
  char *buffer = (char *)sid_calloc( 256, sizeof(char) );
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, 255, format, args);
  va_end(args);
  //Serial.println(buffer);
  tft.setTextDatum( MC_DATUM );
  tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
  int yPos = (tft.height()/2 - tft.fontHeight()*2) + 2;
  tft.fillRect( 0, yPos - tft.fontHeight()/2, tft.width(), tft.fontHeight()+1, C64_DARKBLUE );
  tft.drawString( buffer, tft.width()/2, yPos );
  free( buffer );
}




struct SID_Archive
{
  /*
  const char* name; // archive name
  const char* url; // url to the gzip file
  const char* path; // corresponding absolute path in the md5 file
  */
  MD5FileConfig *cfg;

  char *tgzFileName;
  char *sidRealPath;
  char *cachePath;

  ~SID_Archive() {
    free( tgzFileName );
    free( sidRealPath );
    free( cachePath   );
  }

  SID_Archive( /*const char* n, const char* u, const char* p,*/ MD5FileConfig *c) : /*name(n), url(u), path(p),*/ cfg(c)
  {
    #ifdef BOARD_HAS_PSRAM
      psramInit();
    #endif
    tgzFileName = (char*)sid_calloc( 256, sizeof( char ) );
    sidRealPath = (char*)sid_calloc( 256, sizeof( char ) );
    cachePath   = (char*)sid_calloc( 256, sizeof( char ) );
    if( tgzFileName == NULL || sidRealPath == NULL || cachePath == NULL || cfg->archive == NULL ) {
      log_e("Unable to init SID_Archive, aborting !");
      return;
    }
    snprintf( tgzFileName, 256, "/%s.tar.gz", cfg->archive->name );
    snprintf( sidRealPath, 256, "%s/.%s.expanded", cfg->sidfolder, cfg->archive->path );
    snprintf( cachePath,   256, "/tmp/%s.sidcache", cfg->archive->name );
    PrintProgressBar = &UIPrintProgressBar;
  };

  bool check()
  {
    if( !exists() ) {
      return false;
    }
    if( !isExpanded() ) {
      return false;
    }
    return true;
  }

  bool expand()
  {
    UIPrintTitle( cfg->archive->name, "  Expanding Tgz  " );
    setExpanded( false ); // mark the archive as not expanded
    setTarVerify( false ); // false = faster, true = more reliable
    setLoggerCallback( targzNullLoggerCallback ); // comment this out to enable debug (spammy)
    setTarMessageCallback( UIPrintTarProgress ); // show tar filenames
    setProgressCallback( UIPrintGzProgressBar ); // show overall gzip progress

    const char *outfolder = String( String( cfg->sidfolder ) + String("/") ).c_str();
    Serial.printf("Will attempt to expand archive %s to folder %s\n", cfg->archive->name, cfg->sidfolder );

    if(! tarGzExpander( SID_FS, tgzFileName, SID_FS, cfg->sidfolder, nullptr ) ) {
      Serial.printf("tarGzExpander failed with return code #%d\n", tarGzGetError() );
      return false;
    }
    Serial.printf("Success!\n");
    setExpanded( true ); // expanding worked fine, mark the archive as expanded
    return true;
  }

  bool exists()
  {
    return cfg->fs->exists( tgzFileName );
  }

  bool isExpanded()
  {
    return cfg->fs->exists( sidRealPath );
  }

  void setExpanded( bool toggle = true )
  {
    if( ! toggle ) { // set unexpanded
      if( cfg->fs->exists( sidRealPath ) ) {
        cfg->fs->remove( sidRealPath );
        log_w("Archive marked a *not* expanded");
      } else {
        log_w("Archive wasn't marked as expanded, nothing to do");
      }
      return;
    }
    fs::File expansionProofFile = cfg->fs->open( sidRealPath, FILE_WRITE );
    if(!expansionProofFile ) {
      log_e("Unable to write expansion proof to %s", sidRealPath );
      return;
    }
    expansionProofFile.write( 64 );
    expansionProofFile.close();
  }

  bool hasCache()
  {
    return cfg->fs->exists( cachePath );
  }

  bool download()
  {
    if( exists() ) {
      log_w("Archive already downloaded, deleting");
      cfg->fs->remove( tgzFileName );
    }
    if( !wifiConnected ) stubbornConnect();

    mkdirp( cfg->fs, tgzFileName ); // from ESP32-Targz: create traversing directories if none exist

    UIPrintTitle( cfg->archive->name, "   Downloading   " );
    PrintProgressBar = &UIPrintProgressBar;
    if( !wget( cfg->archive->url, *cfg->fs, tgzFileName ) ) {
      Serial.printf("Failed to download %s :-(\n", tgzFileName);
      return false;
    } else {
      Serial.printf("Downloaded %s successfully\n", tgzFileName);
      return true;
    }
  }
/*
  bool loadCache( SIDTunesPlayer *tmpPlayer )
  {
    if(! cfg->fs->exists( cachePath ) ) {
      log_e("Error! Cache file %s does not exist", cachePath );
      return false;
    }
    File cacheFile = cfg->fs->open( cachePath, FILE_READ );
    size_t bufsize = sizeof(songstruct);
    size_t filesize = cacheFile.size();
    if( filesize == 0 ) {
      Serial.printf("[ERROR] Invalid cache. File %s is empty and should be deleted!", cachePath );
      //cfg->fs->remove( cachePath );
      return false;
    }
    Serial.printf("Found cache (%d bytes) with %d entries (%d bytes each)\n", filesize, filesize/bufsize, bufsize );
    char *songstructbuf =  new char[bufsize+1];
    while( cacheFile.readBytes( songstructbuf, bufsize ) > 0 ) {
      songstruct * p1;
      p1=(songstruct *)sid_calloc(1,sizeof(songstruct));
      if( p1==NULL ) {
          log_e("Not enough memory to add song %d", tmpPlayer->numberOfSongs+1);
          break;
      } else {
        memcpy(p1,(const unsigned char*)songstructbuf,sizeof(songstruct));
        p1->fs= &cfg->fs;
        tmpPlayer->listsongs[tmpPlayer->numberOfSongs] = p1;
        //songdebug( tmpPlayer->listsongs[tmpPlayer->numberOfSongs] );
        tmpPlayer->numberOfSongs++;
      }
    }
    delete songstructbuf;
    cacheFile.close();
    return true;
    //Serial.println("[REMOVE ME] Now deleting cache");
    //cfg->fs->remove("/tmp/sidsongs.cache" );
  }
*/
/*
  // recursively create cache files, this is a very long process
  bool createAllCacheFiles( size_t maxSongs )
  {
    size_t structsize = sizeof( songstruct );

    sidPlayer = new SIDTunesPlayer( maxSongs );
    //sidPlayer->setMaxSongs( maxSongs ); // this will call malloc/ps_malloc
    sidPlayer->addSongsFromFolder( *cfg->fs, sidRealPath, ".sid", true );

    if( sidPlayer->numberOfSongs == 0 ) {
      Serial.printf("No songs to save in playlist, aborting");
      return false;
    }

    Serial.printf("Now matching %d songs length with md5 signatures, please be patient!\n", sidPlayer->numberOfSongs );
    sidPlayer->getSongslengthfromMd5( *cfg->fs, cfg->md5filepath );

    Serial.printf("Now saving %d songs in cache file to disk as %s using blocks of %d bytes\n", sidPlayer->numberOfSongs, cachePath, structsize );

    fs::File cacheFile = cfg->fs->open( cachePath, FILE_WRITE );

    if( !cacheFile ) {
      Serial.printf("Cache file creation failed, aborting\n");
      return false;
    }

    for( int i=0; i<sidPlayer->numberOfSongs; i++ ) {
      size_t written_bytes = cacheFile.write( (uint8_t*)sidPlayer->listsongs[i], structsize );
      if( written_bytes != structsize ) { // SD Card dying ?
        Serial.printf("[Error] BAD SDCARD ?\nSD.write(buff, %d) failed: %d bytes written @offset %d], aborting\n", structsize, written_bytes, i );
        songdebug( sidPlayer->listsongs[i] );
        written_bytes = Serial.write( (uint8_t*)sidPlayer->listsongs[i], structsize );
        Serial.printf("\n[Serial] wrote %d bytes out of %d\n", structsize, written_bytes );
        return false;
      }
    }

    Serial.println("All songs written to cache file, finalizing...");
    cacheFile.close();
    // reopen file for sizing
    cacheFile = cfg->fs->open( cachePath, FILE_READ );
    size_t cacheSize = cacheFile.size();
    cacheFile.close();
    if( cacheSize == 0 ) { // DUH!
      Serial.printf("Saved playlist file is empty (although there are %d elements in playlist), it should be deleted !!\n", sidPlayer->numberOfSongs );
      return false;
    } else {
      Serial.printf("Saved playlist to cache (%d bytes, %d items)\n", cacheSize, cacheSize/structsize );
      return true;
    }

    delete sidPlayer;
    sidPlayer = NULL;
  }
*/
};



struct SID_Archive_Checker
{

  MD5FileConfig *cfg;
  SID_Archive *archive = nullptr;
  bool wifi_occured = false;

  SID_Archive_Checker( MD5FileConfig *c ) : cfg(c) {

    archive = new SID_Archive( cfg );

    Serial.printf("SID Archive Checker loaded config:\n  SID_FOLDER=%s\n  MD5_PATH=%s\n  MD5_IDX=%s\n  MD5_URL=%s\n",
      cfg->sidfolder,   // ex:  /sid
      cfg->md5filepath, // ex:  /md5/Songlengths.full.md5
      cfg->md5idxpath,  // ex:  /md5/Songlengths.full.md5.idx
      cfg->md5url       // ex:  https://www.prg.dtu.dk/HVSC/C64Music/DOCUMENTS/Songlengths.md5
    );
  }

  void checkArchive()
  {
    Serial.println("Entering SID Archive Checker");

    checkMd5File();
    checkGzFile();
    // now turn off wifi as it ate more than 80Kb ram
    if( wifiConnected ) {
      wifiOff();
    }
    checkGzExpand();

    Serial.println("Leaving SID Archive Checker");
  }


  bool checkGzFile( bool force = false )
  {
    // first pass, download all things
    if( archive != nullptr ) {
      if( !archive->exists() ) {
        if( !wifiConnected ) stubbornConnect(); // go online
        wifi_occured = true;
        if( !archive->download() ) {
          Serial.printf("Downloading of %s failed, deleting damaged or empty file\n", archive->tgzFileName );
          cfg->fs->remove( archive->tgzFileName );
          return false;
        }
      }
      return true;
    }
    return false;
  }

  bool checkGzExpand( bool force = false )
  {
    // expand archives
    if( archive != nullptr ) {
      if( archive->exists() ) {
        if( force || !archive->isExpanded() ) {
          if( !archive->expand() ) {
            Serial.printf("Expansion of %s failed, please delete the %s folder and %s archive manually\n", cfg->archive->name, archive->sidRealPath, archive->tgzFileName );
            return false;
          }
        } else {
          Serial.printf("Archive %s looks healthy\n", cfg->archive->name );
        }
        return true;
      } else {
        Serial.printf("Archive %s is missing\n", cfg->archive->name );
      }
    }
    return false;
  }

  bool checkMd5File( bool force = false )
  {
    // also download the MD5 checksums file containing song lengths
    if( force || !cfg->fs->exists( cfg->md5filepath ) ) {
      mkdirp( cfg->fs, cfg->md5filepath ); // from ESP32-Targz: create traversing directories if none exist
      if( !wifiConnected ) stubbornConnect(); // go online
      wifi_occured = true;
      if( !wget( cfg->md5url, *cfg->fs, cfg->md5filepath ) ) {
        Serial.println("Failed to download Songlengths.full.md5 :-(");
        return false;
      } else {
        Serial.println("Downloaded song lengths successfully");
      }
    }
    return true;
  }




};



#endif
