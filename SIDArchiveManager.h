/*
 *
 * SID Archive Manager for Hwpit's SID6581.h
 * copyleft tobozo 2020
 *
 */


//#define SID_FS SPIFFS // deprecated
#define SID_FS       SD // filesystem (SD strongly recommended)
#define SID_FOLDER   "/sid" // local path, NO TRAILING SLASH, MUST BE A FOLDER
#define MD5_FILE     "/md5/Songlengths.full.md5" // local path
#define MD5_FILE_IDX MD5_FILE ".idx"
#define MD5_URL      "https://www.prg.dtu.dk/HVSC/C64Music/DOCUMENTS/Songlengths.md5" // remote url


#define DEST_FS_USES_SD
#include <ESP32-targz.h>
#include "SIDCacheManager.h"



MD5FileConfig MD5Config = { &SID_FS, SID_FOLDER, MD5_FILE, MD5_FILE_IDX, MD5_URL };
//MD5FileParser *MD5Parser = new MD5FileParser( &MD5Config );

int lastresp = -1;
unsigned long lastpush = millis();
unsigned long lastEvent = millis();
int debounce = 200;
uint8_t maxVolume = 10; // value must be between 0 and 15
static bool render = true;
static bool stoprender = false;
int positionInPlaylist = -1;
static TaskHandle_t renderVoicesTaskHandle = NULL;

static void renderVoices( void* param );


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
    sprintf( progressStr, "Progress: %3d%s", int(progress), "%" );
    tft.setTextDatum( MC_DATUM );
    tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
    tft.drawString( progressStr, tft.width()/2, tft.height()/2 );
    tft.setCursor( tft.height()-(tft.fontHeight()+2), 10 );
    tft.printf("    heap: %6d    ", ESP.getFreeHeap() );
  }
}
static void UIPrintGzProgressBar( uint8_t progress )
{
  UIPrintProgressBar( (float)progress );
}

static void UIPrintTarProgress(const char* format, ...)
{
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsprintf(buffer, format, args);
  va_end(args);
  //Serial.println(buffer);
  tft.setTextDatum( MC_DATUM );
  tft.setTextColor( C64_LIGHTBLUE, C64_DARKBLUE );
  int yPos = (tft.height()/2 - tft.fontHeight()*2) + 2;
  tft.fillRect( 0, yPos - tft.fontHeight()/2, tft.width(), tft.fontHeight()+1, C64_DARKBLUE );
  tft.drawString( buffer, tft.width()/2, yPos );
}




struct SID_Archive
{
  const char* name; // archive name
  const char* url; // url to the gzip file
  const char* path; // corresponding absolute path in the md5 file
  MD5FileConfig *cfg;
  char tgzFileName[255] = {0};
  char sidRealPath[255] = {0};
  char cachePath[255]   = {0};

  SID_Archive( const char* n, const char* u, const char* p, MD5FileConfig *c) : name(n), url(u), path(p), cfg(c)
  {
    sprintf( tgzFileName, "/%s.tar.gz", name );
    sprintf( sidRealPath, "%s%s", cfg->sidfolder, path );
    sprintf( cachePath,   "/tmp/%s.sidcache", name );
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
    UIPrintTitle( name, "  Expanding Tgz  " );
    setTarVerify( false ); // false = faster, true = more reliable
    setLoggerCallback( targzNullLoggerCallback ); // comment this out to enable debug (spammy)
    //setTarProgressCallback( targzNullProgressCallback ); // tar handle per-file progress
    setTarMessageCallback( UIPrintTarProgress ); // tar handles filenames
    setProgressCallback( UIPrintGzProgressBar ); // gzip handles progress

    const char *outfolder = String( String( cfg->sidfolder ) + String("/") ).c_str();
    Serial.printf("Will attempt to expand archive %s to folder %s\n", name, cfg->sidfolder );

    if(! tarGzExpander( SID_FS, tgzFileName, SID_FS, cfg->sidfolder, nullptr ) ) {
      Serial.printf("tarGzExpander failed with return code #%d\n", tarGzGetError() );
      return false;
    }
    Serial.printf("Success!\n");
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

    UIPrintTitle( name, "   Downloading   " );
    PrintProgressBar = &UIPrintProgressBar;
    if( !wget( url, *cfg->fs, tgzFileName ) ) {
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

};



struct SID_Archive_Checker
{

  MD5FileConfig *cfg;

  SID_Archive_Checker( MD5FileConfig *c ) : cfg(c) {
    Serial.printf("SID Archive Checker loaded config:\n  SID_FOLDER=%s\n  MD5_PATH=%s\n  MD5_IDX=%s\n  MD5_URL=%s\n",
      cfg->sidfolder,   // /sid"
      cfg->md5filepath, // /md5/Songlengths.full.md5
      cfg->md5idxpath,  // /md5/Songlengths.full.md5.idx
      cfg->md5url      // https://www.prg.dtu.dk/HVSC/C64Music/DOCUMENTS/Songlengths.md5
    );
  }

  void checkArchives( SID_Archive* archives, size_t totalsidpackages )
  {
    //size_t archive
    //size_t totalsidpackages = sizeof( archives ) / sizeof( archives[0] );
    Serial.printf("SID Archive Checker received %d packages to check\n", totalsidpackages );
    // first pass, download all things
    for( int i=0; i<totalsidpackages; i++ ) {
      if( !archives[i].exists() ) {
        if( !archives[i].download() ) {
          Serial.printf("Downloading of %s failed, deleting damaged or empty file\n", archives[i].tgzFileName );
          cfg->fs->remove( archives[i].tgzFileName );
        }
      }
    }

    // also download the MD5 checksums file containing song lengths
    if( ! cfg->fs->exists( cfg->md5filepath ) ) {

      if( !wifiConnected ) stubbornConnect();

      mkdirp( cfg->fs, cfg->md5filepath ); // from ESP32-Targz: create traversing directories if none exist

      if( !wget( cfg->md5url, *cfg->fs, cfg->md5filepath ) ) {
        Serial.println("Failed to download Songlengths.full.md5 :-(");
      } else {
        Serial.println("Downloaded song lengths successfully");
      }
    }

    // now turn off wifi as it ate more than 80Kb ram
    if( wifiConnected ) wifiOff();

    // expand archives
    for( int i=0; i<totalsidpackages; i++ ) {
      if( archives[i].exists() ) {
        if( !archives[i].isExpanded() ) {
          if( !archives[i].expand() ) {
            Serial.printf("Expansion of %s failed, please delete the %s folder and %s archive manually\n", archives[i].name, archives[i].sidRealPath, archives[i].tgzFileName );
          }
        } else {
          Serial.printf("Archive %s looks healthy\n", archives[i].name );
        }
      } else {
        Serial.printf("Archive %s is missing\n", archives[i].name );
      }
    }

    // cache files will be created on first access
    /*
    for( int i=0; i<totalsidpackages; i++ ) {
      if( archives[i].exists() && archives[i].isExpanded() && !archives[i].hasCache() ) {
        Serial.printf("Cache file %s is missing, will rebuild\n", archives[i].cachePath );
        if( ! archives[i].createAllCacheFiles( 4096 ) ) {
          cfg->fs->remove( archives[i].cachePath );
          Serial.println("Halted");
          while(1);
        }
      }
    }
    */

    Serial.println("Exiting SID Archive Checker");
  }

};

SID_Archive_Checker *SidArchiveChecker;
