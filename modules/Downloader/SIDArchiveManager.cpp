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

#include "SIDArchiveManager.hpp"


SID_Archive::~SID_Archive()
{
  free( tgzFileName );
  free( sidRealPath );
  free( cachePath   );
}


SID_Archive::SID_Archive( MD5FileConfig *c) : cfg(c)
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
  #if defined _SID_HVSC_FILE_INDEXER_H_ && defined SID_DOWNLOAD_ARCHIVES
  PrintProgressBar = &UIPrintProgressBar;
  #endif
};


bool SID_Archive::check()
{
  if( !exists() ) {
    return false;
  }
  if( !isExpanded() ) {
    return false;
  }
  return true;
}


bool SID_Archive::expand()
{
  UIPrintTitle( cfg->archive->name, "  Expanding Tgz  " );
  setExpanded( false ); // mark the archive as not expanded

  TarGzUnpacker *TARGZUnpacker = new TarGzUnpacker();

  TARGZUnpacker->haltOnError( true ); // stop on fail (manual restart/reset required)
  TARGZUnpacker->setTarVerify( false ); // true = enables health checks but slows down the overall process
  TARGZUnpacker->setupFSCallbacks( targzTotalBytesFn, targzFreeBytesFn ); // prevent the partition from exploding, recommended
  TARGZUnpacker->setGzProgressCallback( BaseUnpacker::defaultProgressCallback ); // targzNullProgressCallback or defaultProgressCallback
  TARGZUnpacker->setLoggerCallback( BaseUnpacker::targzPrintLoggerCallback  );    // gz log verbosity
  TARGZUnpacker->setTarProgressCallback( BaseUnpacker::defaultProgressCallback ); // prints the untarring progress for each individual file
  TARGZUnpacker->setTarStatusProgressCallback( BaseUnpacker::defaultTarStatusProgressCallback ); // print the filenames as they're expanded
  TARGZUnpacker->setTarMessageCallback( BaseUnpacker::targzPrintLoggerCallback ); // tar log verbosity

  //setTarVerify( false ); // false = faster, true = more reliable
  //setLoggerCallback( targzNullLoggerCallback ); // comment this out to enable debug (spammy)
  //setTarMessageCallback( UIPrintTarProgress ); // show tar filenames
  //setProgressCallback( UIPrintGzProgressBar ); // show overall gzip progress

  //const char *outfolder = String( String( cfg->sidfolder ) + String("/") ).c_str();
  Serial.printf("Will attempt to expand archive %s to folder %s\n", cfg->archive->name, cfg->sidfolder );

  if(! TARGZUnpacker->tarGzExpander( SID_FS, tgzFileName, SID_FS, cfg->sidfolder, nullptr ) ) {
    Serial.printf("tarGzExpander failed with return code #%d\n", TARGZUnpacker->tarGzGetError() );
    return false;
  }
  Serial.printf("Success!\n");
  setExpanded( true ); // expanding worked fine, mark the archive as expanded
  return true;
}


bool SID_Archive::exists()
{
  return cfg->fs->exists( tgzFileName );
}


bool SID_Archive::isExpanded()
{
  return cfg->fs->exists( sidRealPath );
}


void SID_Archive::setExpanded( bool toggle )
{
  if( ! toggle ) { // set unexpanded
    if( cfg->fs->exists( sidRealPath ) ) {
      cfg->fs->remove( sidRealPath );
      log_w("Archive marked a *not* expanded");
    } else {
      log_d("Archive wasn't marked as expanded, nothing to do");
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


bool SID_Archive::hasCache()
{
  return cfg->fs->exists( cachePath );
}


#if defined _SID_HVSC_FILE_INDEXER_H_ && defined SID_DOWNLOAD_ARCHIVES
bool SID_Archive::download()
{
  if( exists() ) {
    log_w("Archive already downloaded, deleting");
    cfg->fs->remove( tgzFileName );
  }
  if( !wifiConnected ) stubbornConnect();

  mkdirp( cfg->fs, tgzFileName ); // from ESP32-Targz: create traversing directories if none exist

  UIPrintTitle( cfg->archive->name, "   Downloading   " );
  PrintProgressBar = &UIPrintProgressBar;
  if( !wget( HVSC_TGZ_URL, *cfg->fs, tgzFileName ) ) {
    Serial.printf("Failed to download %s :-(\n", tgzFileName);
    return false;
  } else {
    Serial.printf("Downloaded %s successfully\n", tgzFileName);
    return true;
  }
}
#endif

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




SID_Archive_Checker::SID_Archive_Checker( MD5FileConfig *c ) : cfg(c)
{
  archive = new SID_Archive( cfg );
  Serial.printf("SID Archive Checker Config:\n  SID_FOLDER=%s\n  MD5_PATH=%s\n  MD5_IDX=%s\n  MD5_URL=%s\n  HVSC_TGZ_URL=%s\n",
    cfg->sidfolder,
    cfg->md5filepath,
    cfg->md5idxpath,
    MD5_URL,
    HVSC_TGZ_URL
  );
}


void SID_Archive_Checker::checkArchive()
{
  Serial.println("Entering SID Archive Checker");

  checkMd5File();
  checkGzFile();
  // now turn off wifi as it ate more than 80Kb ram
  #if defined _SID_HVSC_FILE_INDEXER_H_ && defined SID_DOWNLOAD_ARCHIVES
  if( wifiConnected ) {
    wifiOff();
  }
  #endif
  checkGzExpand();

  Serial.println("Leaving SID Archive Checker");
}


bool SID_Archive_Checker::checkGzFile( bool force )
{
  // first pass, download all things
  if( archive != nullptr ) {
    if( !archive->exists() ) {
      #if defined _SID_HVSC_FILE_INDEXER_H_ && defined SID_DOWNLOAD_ARCHIVES
      if( !wifiConnected ) stubbornConnect(); // go online
      wifi_occured = true;
      if( !archive->download() ) {
        Serial.printf("Downloading of %s failed, deleting damaged or empty file\n", archive->tgzFileName );
        cfg->fs->remove( archive->tgzFileName );
        return false;
      }
      #else
        return false;
      #endif
    }
    return true;
  }
  return false;
}


bool SID_Archive_Checker::checkGzExpand( bool force )
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

bool SID_Archive_Checker::checkMd5File( bool force )
{
  // also download the MD5 checksums file containing song lengths
  if( force || !cfg->fs->exists( cfg->md5filepath ) ) {
    mkdirp( cfg->fs, cfg->md5filepath ); // from ESP32-Targz: create traversing directories if none exist
    #if defined _SID_HVSC_FILE_INDEXER_H_ && defined SID_DOWNLOAD_ARCHIVES
    if( !wifiConnected ) stubbornConnect(); // go online
    wifi_occured = true;
    if( !wget( MD5_URL, *cfg->fs, cfg->md5filepath ) ) {
      Serial.println("Failed to download Songlengths.full.md5 :-(");
      return false;
    } else {
      Serial.println("Downloaded song lengths successfully");
    }
    #else
      return false;
    #endif
  }
  return true;
}

