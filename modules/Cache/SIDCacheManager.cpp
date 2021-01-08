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

#include "SIDCacheManager.hpp"



SongCacheManager::~SongCacheManager() {
  free( basename  );
  free( basepath  );
  free( cachepath );
  free( md5path   );
  //if( song != nullptr) free( song );
}



SongCacheManager::SongCacheManager( SIDTunesPlayer *_sidPlayer )
{
  #ifdef BOARD_HAS_PSRAM
    psramInit();
  #endif
  sidPlayer = _sidPlayer;
  fs = sidPlayer->fs;
  basename  = (char*)sid_calloc( 256, sizeof(char) );
  basepath  = (char*)sid_calloc( 256, sizeof(char) );
  cachepath = (char*)sid_calloc( 256, sizeof(char) );
  md5path   = (char*)sid_calloc( 256, sizeof(char) );
  cleanPath.reserve(256);
}



void SongCacheManager::init( const char* path )
{
  /*\
    * path="/sid/MUSICIANS/X/Xonic_the_Fox/Ohne_Dich_Rammstein.sid"
    *  -> basename:  Ohne_Dich_Rammstein.sid
    *  -> basepath:  /sid/MUSICIANS/X/Xonic_the_Fox/
    *  -> cachepath: /sid/MUSICIANS/X/Xonic_the_Fox/.sidcache
    *  -> md5path:   /MUSICIANS/X/Xonic_the_Fox/Ohne_Dich_Rammstein.sid
  \*/
  memset( basename, 0, 255 );
  memset( basepath, 0, 255 );
  memset( cachepath, 0, 255 );
  memset( md5path, 0, 255 );
  String bname = gnu_basename( path );
  strcpy( basename, bname.c_str() );
  memmove( basepath, path, strlen(path)-strlen(basename) ); // remove filename from path
  snprintf( cachepath, 255, "%s%s", basepath, ".sidcache" );
  if( String(path).startsWith( SID_FOLDER ) ) {
    memmove( md5path, path+strlen( SID_FOLDER ), strlen(path) ); // remove leading SID_FOLDER from path
  } else {
    strcpy( md5path, path );
  }
  log_v("\nComputed from %s:\n- basename: %s\n- basepath: %s\n- cachepath: %s\n- md5path: %s\n", path, basename, basepath, cachepath, md5path );
}



bool SongCacheManager::exists( SID_Meta_t *song )
{
  fs::File cacheFile = fs->open( cachepath, FILE_READ );
  if( ! cacheFile ) return false;
  bool ret = false;
  size_t bufsize = sizeof(SID_Meta_t);
  char *SID_Meta_tbuf =  new char[bufsize+1];
  while( cacheFile.readBytes( SID_Meta_tbuf, bufsize ) > 0 ) {
    const SID_Meta_t* tmpsong = (const SID_Meta_t*)SID_Meta_tbuf;
    // jump forward subsongs lengths
    cacheFile.seek( cacheFile.position() + tmpsong->subsongs*sizeof(uint32_t) );
    if( strcmp( (const char*)tmpsong->name, (const char*)song->name ) == 0
    && strcmp( (const char*)tmpsong->author, (const char*)song->author ) == 0 ) {
      ret = true;
      break;
    }
  }
  cacheFile.close();
  delete SID_Meta_tbuf;
  return ret;
}



// because esp-idf vfs implementation fucks up extensions when filename is uppercase/numbers-only
void SongCacheManager::setCleanFileName( char* dest, size_t len, const char* format, const char* src )
{
  String clean = String(src);
  size_t slen = strlen(src);
  String lower = clean;
  lower.toLowerCase();
  if( lower.endsWith( ".sid" ) ) {
    String end = clean.substring( slen-4, slen );
    end.toLowerCase();
    clean = clean.substring( 0, slen-4 ) + end;
  }
  snprintf( dest, len, format, clean.c_str() );
  log_d("Cleaned up %s => %s ", src, dest );
}



bool SongCacheManager::insert( SID_Meta_t *song, bool append )
{
  //fs::File cacheFile;
  if( append ) {
    //songCacheFile = fs->open( cachepath, "a+" );
    // TODO: check that cacheFile.position() is a multiple of SID_Meta_t
  } else {
    songCacheFile = fs->open( cachepath, "w" ); // truncate
  }
  if( ! songCacheFile ) return false;
  snprintf( song->filename, 255, "%s", md5path ); // don't use the full path
  size_t objsize = sizeof( SID_Meta_t );// + song->subsongs*sizeof(uint32_t);
  size_t written_bytes = songCacheFile.write( (uint8_t*)song, objsize );
  //songCacheFile.close();
  return written_bytes == objsize;
}



const String SongCacheManager::makeCachePath( const char* path, const char* cachename )
{
  if( path == NULL || path[0] == '\0' ) {
    log_e("Bad path");
    return "";
  }
  if( cachename == NULL || cachename[0] == '\0' ) {
    log_e("Bad cachename");
    return "";
  }
  if( path[strlen(path)-1] == '/' ) {
    cleanPath = String(path) + String(cachename);
  } else {
    cleanPath = String(path) + "/" + String(cachename);
  }
  log_d("Generated path %s from %s and %s", cleanPath.c_str(), path, cachename );
  return cleanPath;
}



const String SongCacheManager::playListCachePath( const char * path )
{
  return makeCachePath( path, ".sidcache");
}



const String SongCacheManager::dirCachePath( const char * path )
{
  return makeCachePath( path, ".dircache");
}



bool SongCacheManager::folderHasPlaylist( const char* fname )
{
  return fs->exists( playListCachePath( fname ) );
}



bool SongCacheManager::folderHasCache( const char* fname )
{
  return fs->exists( dirCachePath( fname ) );
}



bool SongCacheManager::purge( const char * path )
{
  if( path == NULL || path[0] == '\0' ) {
    log_e("Bad path name");
    return false;
  }
  log_d("Will purge path %s", path );

  String songCachePath = playListCachePath( path );
  String dirCacheFile  = dirCachePath( path );
  log_d("Speculated %s and %s from %s", songCachePath.c_str(), dirCacheFile.c_str(), path );
  if( fs->exists( songCachePath ) ) {
    log_d("Removing %s", songCachePath.c_str() );
    fs->remove( songCachePath );
  } else {
    log_w("No %s (playlist) file to purge", songCachePath.c_str() );
  }
  if( fs->exists( dirCacheFile ) ) {
    log_d("Removing %s", dirCacheFile.c_str() );
    fs->remove( dirCacheFile );
  } else {
    log_w("No %s (dircache) file to purge", dirCacheFile.c_str() );
  }
  return true;
}



bool SongCacheManager::add( const char * path, SID_Meta_t *song )
{
  init( path );
  if( fs->exists( cachepath ) ) { // cache file exists !!
    if( !exists( song ) ) { // song is not in cache file
      log_d("[Cache Insertion]");
      return insert( song, true );
    }
    log_d("[Cache Hit]");
    return true; // song is already in the cache file
  } else { // cache file does not exist
    log_d("[Cache Creation+Insertion]");
    return insert( song, false );
  }
}



int SongCacheManager::addSong( const char * path )
{
  SID_Meta_t *song = (SID_Meta_t*)sid_calloc(1, sizeof(SID_Meta_t) );
  song->durations = nullptr;

  setCleanFileName( song->filename, 255, "%s", path );

  int ret = -1;

  if( sidPlayer->getInfoFromSIDFile( path, song ) ) {
    if( add( path, song ) ) {
      log_d("[%d] Cached %s", ESP.getFreeHeap(), path);
      ret = 1;
    } else {
      log_e("[%d] Caching failed for %s", ESP.getFreeHeap(), path);
      ret = 0;
    }
  } else {
    //log_e("Ignoring unhandled file %s", path);
    ret = 0;
  }
  //if( song != nullptr ) {
  if( song->durations != nullptr ) free( song->durations );
  free( song );
  //}
  return ret;
}



bool SongCacheManager::update( const char* path, SID_Meta_t *song )
{
  init( path );
  setCleanFileName( song->filename, 255, "%s", md5path ); // don't use the full path
  //snprintf( song->filename, 255, "%s", md5path ); // don't use the full path
  fs::File cacheFile = fs->open( cachepath, "r+" );
  if( ! cacheFile ) return false;
  bool ret = false;
  size_t bufsize = sizeof(SID_Meta_t);
  char *SID_Meta_tbuf =  new char[bufsize+1];
  size_t entry_index = 0;
  while( cacheFile.readBytes( SID_Meta_tbuf, bufsize ) > 0 ) {
    const SID_Meta_t* tmpsong = (const SID_Meta_t*)SID_Meta_tbuf;
    if( strcmp( (const char*)tmpsong->name, (const char*)song->name ) == 0
    && strcmp( (const char*)tmpsong->author, (const char*)song->author ) == 0 ) {
      cacheFile.seek( entry_index*bufsize );
      size_t objsize = bufsize + song->subsongs*sizeof(uint32_t);
      size_t bytes_written = cacheFile.write( (uint8_t*)song, objsize );
      ret= bytes_written == objsize;
      break;
    }
    entry_index++;
  }
  delete SID_Meta_tbuf;
  cacheFile.close();
  return ret; // not found
}



bool SongCacheManager::get( const char* md5path, SID_Meta_t *song )
{
  char *fullpath = (char*)sid_calloc( 256, sizeof(char) );
  snprintf( fullpath, 255, "%s/%s", SID_FOLDER, md5path );
  init( fullpath );
  free( fullpath );
  bool ret = false;
  fs::File cacheFile = fs->open( cachepath, FILE_READ );
  if( ! cacheFile ) return false;
  size_t bufsize = sizeof(SID_Meta_t);
  char *SID_Meta_tbuf =  new char[bufsize+1];
  while( cacheFile.readBytes( SID_Meta_tbuf, bufsize ) > 0 ) {
    const SID_Meta_t* tmpsong = (const SID_Meta_t*)SID_Meta_tbuf;
    if( strcmp( (const char*)tmpsong->filename, md5path ) == 0  ) {
      memcpy( song, tmpsong, bufsize );
      song->durations = (uint32_t*)sid_calloc(song->subsongs, sizeof(uint32_t) );
      cacheFile.readBytes( (char*)song->durations, song->subsongs*sizeof(uint32_t) );
      ret = true;
      break;
    }
  }
  cacheFile.close();
  delete SID_Meta_tbuf;
  return ret;
}



bool SongCacheManager::scanPaginatedFolderCache( const char* pathname, size_t offset, size_t maxitems, folderElementPrintCb printCb, activityTicker tickerCb )
{
  String dirCacheFile = dirCachePath( pathname );
  if( dirCacheFile == "" ) {
    log_e("Bad dirCache filename");
    return false;
  }
  if( ! fs->exists( dirCacheFile ) ) {
    log_e("Cache file %s does not exist, create it first !", dirCacheFile.c_str() );
  }
  fs::File DirCache = fs->open( dirCacheFile );
  if( !DirCache ) {
    log_e("Unable to access dirCacheFile %s", dirCacheFile.c_str() );
    return false;
  }

  getDirCacheSize( &DirCache ); // skip folder size line
  bool ret = true;

  size_t currentoffset = 0;
  while( DirCache.available() ) {
    uint8_t typeint = DirCache.read();
    switch( typeint )
    {
      case F_PLAYLIST:          // SID file list for current folder
      case F_SUBFOLDER:         // subfolder with SID file list
      case F_SUBFOLDER_NOCACHE: // subfolder with no SID file list
      case F_PARENT_FOLDER:     // parent folder
      case F_SID_FILE:          // single SID file
      case F_TOOL_CB:           // not a real file
      break;
      default: // EOF or bad data ?
        goto _end;
    }
    folderItemType_t type = (folderItemType_t) typeint;
    if( DirCache.read() != ':' ) {
      // EOF!
      goto _end;
    }
    String itemName = DirCache.readStringUntil('\n');
    if( itemName == "" ) {
      ret = false;
      goto _end;
    }

    // handle pagination
    if( maxitems == 0 || (currentoffset+1 >= offset && currentoffset+1 < offset+maxitems ) ) {
      log_d("Paginating at offset %d (offset=%d, maxitems=%d)", currentoffset, offset, maxitems );
      printCb( itemName.c_str(), type );
    }

    currentoffset++;

  }
  _end:
  DirCache.close();
  return ret;
}



bool SongCacheManager::scanFolderCache( const char* pathname, folderElementPrintCb printCb, activityTicker tickerCb )
{
  return scanPaginatedFolderCache( pathname, 0, 0, printCb, tickerCb );
}



bool SongCacheManager::dirCacheUpdate( fs::File *dirCache, const char*_itemName, folderItemType_t type )
{
  if( !dirCache) {
    log_e("No cache to update!");
    return false;
  }

  getDirCacheSize( dirCache ); // skip folder size line

  log_d("[%d] Will update item %s in cache %s", ESP.getFreeHeap(), dirCache->name(), _itemName );
  while( dirCache->available() ) {
    size_t linepos = dirCache->position();
    uint8_t typeint = dirCache->read();
    switch( typeint )
    {
      case F_PLAYLIST:          // SID file list for current folder
      case F_SUBFOLDER:         // subfolder with SID file list
      case F_SUBFOLDER_NOCACHE: // subfolder with no SID file list
      case F_PARENT_FOLDER:     // parent folder
      case F_SID_FILE:          // single SID file
      case F_TOOL_CB:           // not a real file
      break;
      default: // EOF ?
        log_v("EOF?");
        goto _end;
    }
    if( dirCache->read() != ':' ) {
      // EOF!
      log_v("EOF!");
      goto _end;
    }
    String itemName = dirCache->readStringUntil('\n');

    if( itemName == "" ) return false;

    if( itemName == String(_itemName) ) {
      log_d("[%d] Matching entry, will update", ESP.getFreeHeap() );
      dirCache->seek( linepos );
      dirCache->write( (uint8_t)type );
      return true;
    }
  }
  _end:
  return false;
}



// add an item to dircache file, update parent cache if item is playlist cache
bool SongCacheManager::dirCacheAdd( fs::File *dirCache, const char*itemName, folderItemType_t type )
{
  if( ! dirCache ) {
    log_e("Invalid file");
    return false;
  }
  if( type == F_PLAYLIST ) {
    // notify parent this folder has a playlist
    const char *dName = dirname( (char *)dirCache->name() );
    const char *parent = dirname( (char *)dName );
    String parentCacheName = String( parent ) + "/.dircache";
    if( !fs->exists( parentCacheName ) ) {
      log_e("Can't update parent cache, file %s does not exist", parentCacheName.c_str() );
    } else {
      bool ret = false;
      fs::File parentDirCache = fs->open( parentCacheName, "r+" );
      if( dirCacheUpdate( &parentDirCache, itemName, F_SUBFOLDER ) ) {
        log_d("[%d] Folder type updated in parent cache file %s for playlist item %s", ESP.getFreeHeap(), parentCacheName.c_str(), itemName );
        ret = true;
      } else {
        log_e("[%d] Unable to update parent cache %s", ESP.getFreeHeap(), parentCacheName.c_str() );
      }
      parentDirCache.close();
      return ret;
    }
  } else {
    log_d("Writing dircache entry %s (%d bytes) at offset %d", itemName, strlen( itemName )+2, dirCache->position() );
    dirCache->write( (uint8_t)type );
    dirCache->write( ':' );
    dirCache->write( (uint8_t*)itemName, strlen( itemName ) ); // no null terminating
    dirCache->write( '\n' );
  }
  return true;
}



void SongCacheManager::dirCacheSort(fs::File *dirCacheFile, size_t maxitems, folderElementPrintCb printCb, activityTicker tickerCb )
{
  size_t dirCacheFileSize = dirCacheFile->size();
  int avail_ram = ESP.getFreeHeap() - 100000; // don't go under 100k heap
  bool use_ram = avail_ram > dirCacheFileSize;
  size_t entries = 0;

  getDirCacheSize( dirCacheFile ); // skip folder size line

  if( use_ram  ) {
    // ram sort
    log_d("[%d] Ram sorting %s", ESP.getFreeHeap(), dirCacheFile->name() );
    std::vector<String> thisFolder;
    while( dirCacheFile->available() ) {
      String blah = dirCacheFile->readStringUntil( '\n' );
      log_d("Feeding folder #%d with '%s'", thisFolder.size(), blah.c_str() );
      thisFolder.push_back( blah );
    }

    log_d("Collected %d items from file %s", thisFolder.size(), dirCacheFile->name() );

    getDirCacheSize( dirCacheFile ); // skip folder size line

    std::sort( thisFolder.begin(), thisFolder.end() );

    log_d("std::sorted %d entries in file %s (at offset %d)", thisFolder.size(), dirCacheFile->name(), dirCacheFile->position() );

    //dirCacheFile->seek(5);

    for( int i=0;i<thisFolder.size();i++ ) {
      size_t startpos = dirCacheFile->position();
      const char* folderData = thisFolder[i].c_str();
      size_t folderSize  = strlen( thisFolder[i].c_str() );
      dirCacheFile->write( (uint8_t*)folderData, folderSize );
      dirCacheFile->seek( startpos + folderSize  ); // dafuq ?
      dirCacheFile->write( '\n' );
      log_d("Written sorted dircache entry '%s' (%d bytes) at offsets [%d-%d]", folderData, folderSize, startpos, dirCacheFile->position() );

      if( maxitems == 0 || entries < maxitems ) {
        printCb( thisFolder[i].c_str()+2, (folderItemType_t)thisFolder[i].c_str()[0] );
      }
      /*
      if( !tickerCb( "Sort", entries ) ) {
        break;
      }
      */
      entries++;
    }

    log_d("Written %d entries in file %s", entries, dirCacheFile->name() );

  } else {
    // disk sort
    log_e("[%d] TODO: slow sort on disk", ESP.getFreeHeap() );
  }
  tickerCb( "", entries );
  //dirCacheFile->close();

}



void SongCacheManager::deleteCache( const char* pathname )
{
  String dircachepath = dirCachePath( pathname );
  if( fs->exists( dircachepath ) )
    fs->remove( dircachepath );
}



size_t SongCacheManager::getDirCacheSize( const char*pathname )
{
  String dircachepath = dirCachePath( pathname );
  File cacheFile = fs->open( dircachepath );
  size_t cachesize = getDirCacheSize( &cacheFile );
  cacheFile.close();
  return cachesize;
}



size_t SongCacheManager::getDirCacheSize( fs::File *cacheFile )
{
  uint8_t *sizeToByte = new uint8_t[sizeof(size_t)];
  cacheFile->seek(0);
  cacheFile->read( sizeToByte, sizeof(size_t) );
  size_t cachesize = 0;
  memcpy( &cachesize, sizeToByte, sizeof(size_t) );
  // skip separator
  if( cacheFile->read() != '\n' ) {
    log_e("[%s] Unexpected byte while thawing %d as %d bytes: %02x %02x %02x %02x (%s)", cacheFile->name(), cachesize, sizeof(size_t), sizeToByte[0], sizeToByte[1], sizeToByte[2], sizeToByte[3], (const char*)sizeToByte );
  } else {
    log_d("Cache file %s claims %d elements", cacheFile->name(), cachesize );
  }
  delete sizeToByte;
  cacheFile->seek(5);
  return cachesize;
}



void SongCacheManager::setDirCacheSize( fs::File *dirCacheFile, size_t totalitems )
{
  uint8_t *sizeToByte = new uint8_t[sizeof(size_t)];
  memcpy( sizeToByte, &totalitems, sizeof(size_t) );
  //dirCacheFile->seek(0);
  dirCacheFile->write( sizeToByte, sizeof(size_t) );
  dirCacheFile->write( '\n' );
  log_d("Cache file %s will claim %d elements", dirCacheFile->name(), totalitems );
  delete sizeToByte;
  //dircachesize = totalitems;
}



bool SongCacheManager::scanPaginatedFolder( const char* pathname, folderElementPrintCb printCb, activityTicker tickerCb, size_t offset, size_t maxitems )
{
  File DirCache;
  File root = fs->open( pathname );
  if(!root){
    log_e("Fatal: failed to open %s directory\n", pathname);
    return false;
  }
  if(!root.isDirectory()){
    log_e("Fatal: %s is not a directory\b", pathname);
    return false;
  }

  bool folder_has_playlist = folderHasPlaylist( pathname );

  if( folder_has_playlist ) {
    // insert it now as it'll be ignored
    if( offset == 0 ) {
      printCb( pathname, F_PLAYLIST );
      maxitems--;
    }
  }
  root.close();

  bool ret = scanPaginatedFolderCache( pathname, offset, maxitems, printCb, tickerCb );

  return ret;
}



bool SongCacheManager::scanFolder( const char* pathname, folderElementPrintCb printCb, activityTicker tickerCb, size_t maxitems, bool force_regen )
{
  File DirCache;
  File root = fs->open( pathname );
  if(!root){
    log_e("Fatal: failed to open %s directory\n", pathname);
    return false;
  }
  if(!root.isDirectory()){
    log_e("Fatal: %s is not a directory\b", pathname);
    return false;
  }

  bool folder_has_playlist = folderHasPlaylist( pathname );
  bool folder_has_cache    = folderHasCache( pathname );

  bool build_cache = false;
  bool cache_built = false;

  if( force_regen ) {
    purge( pathname );
    folder_has_playlist = false;
    folder_has_cache = false;
  }

  size_t totalitems = 0;

  if( folder_has_playlist ) {
    // insert it now as it'll be ignored
    printCb( pathname, F_PLAYLIST );
    totalitems++;
  } else {
    build_cache = true;
  }

  if( folder_has_cache ) {
    root.close();
    log_d("[%d] Serving cached folder", ESP.getFreeHeap() );
    bool ret = scanFolderCache( pathname, printCb, tickerCb );
    log_d("[%d] Scanned folder cache", ESP.getFreeHeap() );
    return ret;
  } else {
    log_d("Generating cached folder (init to size_t 0)");
    DirCache = fs->open( dirCachePath( pathname ), FILE_WRITE );
    setDirCacheSize( &DirCache, 0 );
  }

  File file = root.openNextFile();
  while( file ) {
    if( file.isDirectory()  ) {
      if( folderHasPlaylist( file.name() ) ) {
        //printCb( file.name(), F_SUBFOLDER );
        dirCacheAdd( &DirCache, file.name(), F_SUBFOLDER );
      } else {
        //printCb( file.name(), F_SUBFOLDER_NOCACHE );
        dirCacheAdd( &DirCache, file.name(), F_SUBFOLDER_NOCACHE );
      }
      totalitems++;
    } else {
      // handle both uppercase and lowercase of the extension
      String fName = String( file.name() );
      fName.toLowerCase();

      char cleanfilename[255] = {0};
      setCleanFileName( cleanfilename, 255, "%s", file.name() );

      if( fName.endsWith( ".sid" ) && !file.isDirectory() ) {
        //printCb( file.name(), F_SID_FILE );
        dirCacheAdd( &DirCache, cleanfilename, F_SID_FILE );

        if( build_cache ) {
          // eligibility for .sidcache ?
          if( addSong( cleanfilename ) > 0 ) {
            cache_built = true;
          }
        }
        totalitems++;

      } else {
        // ignore that file
      }
    }
    file.close();
    file = root.openNextFile();
    if( !tickerCb( "Scan", totalitems ) ) {
      break;
    }
    if( totalitems>=maxItemsPerFolder ) {
      log_w("Total items limitation reached at %d, truncating folder", maxItemsPerFolder );
      break;
    }
  }

  if( build_cache && cache_built && !folder_has_playlist ) {
    dirCacheAdd( &DirCache, pathname, F_PLAYLIST );
    printCb( pathname, F_PLAYLIST );
    if( songCacheFile )
      songCacheFile.close();
    maxitems--;
  }

  DirCache.close();

  // reopen as r+ mode to update cache size
  DirCache = fs->open( dirCachePath( pathname ), "r+" );
  setDirCacheSize( &DirCache, totalitems );
  DirCache.close();

  // reopen as r+ mode to sort cache content
  DirCache = fs->open( dirCachePath( pathname ), "r+" );
  dirCacheSort( &DirCache, maxitems, printCb, tickerCb );
  DirCache.close();

  root.close();

  return true;
}


void SongCacheManager::makeCachePath( const char* _cachepath, char* cachepath )
{
  if( !String( _cachepath ).endsWith("/.sidcache") ) {
    log_d("Stray _cachepath: %s (will get .sidcache appended)", _cachepath );
    if( _cachepath[strlen(_cachepath)-1] == '/' ) {
      snprintf( cachepath, 255, "%s.sidcache", _cachepath );
    } else {
      snprintf( cachepath, 255, "%s/.sidcache", _cachepath );
    }
  } else {
    snprintf( cachepath, 255, "%s", _cachepath );
  }
}


// TODO: deprecate this
#if 0
bool SongCacheManager::loadCache( const char* _cachepath )
{
  makeCachePath( _cachepath, cachepath );

  if(! fs->exists( cachepath ) ) {
    log_e("Error! Cache file %s does not exist", cachepath );
    return false;
  }
  File cacheFile = fs->open( cachepath, FILE_READ );
  size_t bufsize = sizeof(SID_Meta_t);
  size_t filesize = cacheFile.size();
  if( filesize == 0 ) {
    log_e("[ERROR] Invalid cache. File %s is empty and should be deleted!", cachepath );
    //SID_FS.remove( cachepath );
    return false;
  }
  log_d("Cache file %s (%d bytes) estimated to %d entries (average %d bytes each)", cachepath, filesize, filesize/bufsize, bufsize );
  char *SID_Meta_tbuf =  new char[bufsize+1];
  while( cacheFile.readBytes( SID_Meta_tbuf, bufsize ) == bufsize ) {
    if( sidPlayer->numberOfSongs >= sidPlayer->maxSongs ) {
      log_w("Max songs (%d) in player reached, the rest of this cache will be ignored", sidPlayer->maxSongs );
      break;
    }
    SID_Meta_t * song;
    song=(SID_Meta_t *)sid_calloc(1,sizeof(SID_Meta_t));
    if( song==NULL ) {
        log_e("Not enough memory to add song %d", sidPlayer->numberOfSongs+1);
        break;
    } else {
      memcpy(song,(const unsigned char*)SID_Meta_tbuf,sizeof(SID_Meta_t));
      //song->fs= &SID_FS;
      if( !String(song->filename).startsWith( SID_FOLDER ) ) {
        // insert SID_FOLDER before path
        memmove( song->filename+strlen(SID_FOLDER), song->filename, strlen(song->filename)+strlen(SID_FOLDER) );
        memcpy( song->filename, SID_FOLDER, strlen( SID_FOLDER ) );
      }
      // alloc variable memory for subsong durations
      song->durations = (uint32_t*)sid_calloc(1, song->subsongs*sizeof(uint32_t) );
      cacheFile.readBytes( (char*)song->durations, song->subsongs*sizeof(uint32_t) );
      sidPlayer->listsongs[sidPlayer->numberOfSongs] = song;
      //songdebug( sidPlayer->listsongs[sidPlayer->numberOfSongs] );
      sidPlayer->numberOfSongs++;
    }
  }
  delete SID_Meta_tbuf;
  cacheFile.close();
  log_w("Loaded %d songs into player", sidPlayer->numberOfSongs );
  //sidPlayer->setSongstructCachePath( SID_FS, cachepath );
  return true;
}
#endif


bool SongCacheManager::getInfoFromCacheItem( const char* _cachepath, size_t itemNum )
{

  makeCachePath( _cachepath, cachepath );
  return getInfoFromCacheItem( itemNum );

}

// load cached song information from a .sidcache file into the sidPlayer
bool SongCacheManager::getInfoFromCacheItem( size_t itemNum )
{

  sidPlayer->stop(); // prevent concurrent SPI access

  if(! fs->exists( cachepath ) ) {
    log_e("Error! Cache file %s does not exist", cachepath );
    return false;
  }
  File cacheFile = fs->open( cachepath );
  if( !cacheFile ) {
    log_e("[ERROR] Cache File %s is unreadable!", cachepath );
    return false;
  }
  size_t bufsize = sizeof(SID_Meta_t);
  size_t filesize = cacheFile.size();
  if( filesize == 0 ) {
    log_e("[ERROR] File %s appears empty, SD error ?", cachepath );
    //SID_FS.remove( cachepath );
    return false;
  }
  log_d("Cache file %s (%d bytes) estimated to %d entries (average %d bytes each)", cachepath, filesize, filesize/bufsize, bufsize );
  char *SID_Meta_buf =  new char[bufsize+1];
  size_t itemCount = 0;
  bool ret = false;

  while( cacheFile.available() ) {
    cacheFile.readBytes( SID_Meta_buf, bufsize );// == bufsize
    itemCount++;
    if( itemCount-1 != itemNum ) {
      continue;
    }

    if( sidPlayer->currenttrack != nullptr ) {
      if( sidPlayer->currenttrack->durations != nullptr ) {
        free( sidPlayer->currenttrack->durations  );
      }
      free( sidPlayer->currenttrack );
    }

    sidPlayer->currenttrack = (SID_Meta_t *)sid_calloc(1,sizeof(SID_Meta_t));

    if( sidPlayer->currenttrack == NULL ) {
      log_e("Not enough memory to add song :-(");
    } else {
      memcpy( sidPlayer->currenttrack, (const unsigned char*)SID_Meta_buf, sizeof(SID_Meta_t) );
      //sidPlayer->currenttrack->fs= &SID_FS;
      if( !String(sidPlayer->currenttrack->filename).startsWith( SID_FOLDER ) ) {
        // insert SID_FOLDER before path
        memmove( sidPlayer->currenttrack->filename+strlen(SID_FOLDER), sidPlayer->currenttrack->filename, strlen(sidPlayer->currenttrack->filename)+strlen(SID_FOLDER) );
        memcpy( sidPlayer->currenttrack->filename, SID_FOLDER, strlen( SID_FOLDER ) );
      }
      // alloc variable memory for subsong durations
      sidPlayer->currenttrack->durations = nullptr; // (uint32_t*)sid_calloc( sidPlayer->currenttrack->subsongs, sizeof(uint32_t) );
      //cacheFile.readBytes( (char*)sidPlayer->currenttrack->durations, sidPlayer->currenttrack->subsongs*sizeof(uint32_t) );
      //songdebug( sidPlayer->currenttrack );
      ret = true;
    }
  }
  delete SID_Meta_buf;
  cacheFile.close();
  CacheItemSize = itemCount;
  CacheItemNum  = itemNum;
  log_d("Loaded %d songs into player (cache size: %d)", sidPlayer->currenttrack->subsongs, CacheItemSize );
  //sidPlayer->setSongstructCachePath( SID_FS, cachepath );
  return ret;
}

