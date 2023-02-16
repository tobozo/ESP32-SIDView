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


namespace SIDView
{

  SongCacheManager* SongCache = nullptr;

  SongCacheManager* SongCacheManager::getSongCache() { return SongCache; }


  SongCacheManager::~SongCacheManager()
  {
    free( basename  );
    free( basepath  );
    free( cachepath );
    free( md5path   );
    if( deepsid_dir ) {
      deepsid_dir->close();
      delete deepsid_dir;
    }
    //if( song != nullptr) free( song );
  }



  SongCacheManager::SongCacheManager( SIDTunesPlayer *_sidPlayer, size_t _itemsPerPage )
  {
    #ifdef BOARD_HAS_PSRAM
      psramInit();
    #endif
    sidPlayer = _sidPlayer;
    fs = sidPlayer->fs;

    #if defined ENABLE_HVSC_SEARCH
      wpio->fs = fs;
    #endif

    basename  = (char*)sid_calloc( 256, sizeof(char) );
    basepath  = (char*)sid_calloc( 256, sizeof(char) );
    cachepath = (char*)sid_calloc( 256, sizeof(char) );
    md5path   = (char*)sid_calloc( 256, sizeof(char) );
    cleanPath.reserve(256);

    itemsPerPage = _itemsPerPage;

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
    log_e("Accessing Filesystem from core #%d", xPortGetCoreID() );
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
    log_v("Cleaned up %s => %s ", src, dest );
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
    if( ! songCacheFile ) {
      log_e("Can't insert entry");
      return false;
    }
    snprintf( song->filename, 255, "%s", md5path ); // don't use the full path
    size_t objsize = sizeof( SID_Meta_t );// + song->subsongs*sizeof(uint32_t);
    size_t written_bytes = songCacheFile.write( (uint8_t*)song, objsize );
    //songCacheFile.close();
    return written_bytes == objsize;
  }



  const String SongCacheManager::makeCachePath( const char* path, const char* cachename )
  {
    // TODO: use Deepsid::pathConcat()
    if( path == NULL || path[0] != '/' ) {
      log_e("Bad path request '%s' for cache name '%s', halting", path, cachename);
      while(1) vTaskDelay(1);
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
    if( cleanPath[0] != '/' ) {
      log_w("makeCachePath generated bad path '%s' from '%s' + '%s', halting", cleanPath.c_str(), path, cachename );
      while(1) vTaskDelay(1);
    } else {
      log_v("makeCachePath generated valid path '%s' from '%s' + '%s'", cleanPath.c_str(), path, cachename );
    }
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


  Deepsid::DirItem SongCacheManager::fetchDeepSIDItem( size_t offset )
  {
    Deepsid::DirItem nullItem{ Deepsid::TYPE_NONE, {}, deepsid_dir };
    if( cache_type != CACHE_DEEPSID ) return nullItem;
    if( deepsid_dir == nullptr ) return nullItem;

    deepsid_dir->rewind();

    if( offset > 0 ) {
      log_d("Forwarding deepsid offset to #%d", offset );
      deepsid_dir->forward( offset );
    }

    auto item = deepsid_dir->openNextFile();

    if( !utils::ends_with( item.name(), ".sid" ) ) {
      log_e("%s is not a deepsid file", item.name().c_str() );
      return nullItem;
    }

    auto deepsid_cfg = deepSID.config();

    //std::string path = item.path( true ); // pathname;
    //bool path_is_slashed = (path.find('/') != std::string::npos);
    //std::string encoded_path = utils::pathencode( path );
    //std::string api_path  = deepsid_cfg.api_url + deepsid_cfg.api_hvsc_dir;
    //std::string base_url  = item.url(); // path_is_slashed ? utils::pathConcat( api_path, encoded_path ) : utils::pathConcat( deepsid_dir->httpDir(), encoded_path );
    //std::string base_path = path_is_slashed ? /*utils::pathConcat( SID_FOLDER,*/ path /*)*/ : utils::pathConcat( deepsid_dir->sidFolder(), path );
    // if( ! utils::starts_with( base_path, std::string(SID_FOLDER) ) ) {
    //   // prepend local SID folder name
    //   base_path = utils::pathConcat( SID_FOLDER, base_path );
    //   // TODO: strip "_High_Voltage_SID_Collection" from path
    // }

    std::string base_url  = item.url();
    std::string base_path = utils::pathConcat( SID_FOLDER, item.path( true ) );

    if( !fs->exists( base_path.c_str() ) ) {
      log_d("%s does not exist, will fetch @ %s", base_path.c_str(), base_url.c_str() );
      if( ! net::downloadFile( base_url, fs, base_path ) ) return nullItem;
    }

    item.attr["filename"] = base_path;

    log_d("fetchecked item url: %s, base_path: %s", base_url.c_str(), base_path.c_str() );

    return item;
  }






  bool SongCacheManager::folderHasPlaylist( const char* fname )
  {
    if( cache_type == CACHE_DEEPSID ) { // deepsid json file
      auto deepsid_cfg = deepSID.config();
      auto *dir = new Deepsid::Dir( std::string(fname), &deepsid_cfg );
      log_d("%s is a JSON path, checking existence...", dir->jsonFile().c_str() );
      bool ret = fs->exists( dir->jsonFile().c_str() );
      delete dir;
      return ret;
      //return true;
    } else {
      return fs->exists( playListCachePath( fname ) ); // cached meta file
    }
  }


  bool SongCacheManager::folderHasCache( const char* fname )
  {
    if( cache_type == CACHE_DEEPSID ) { // deepsid json file
      auto deepsid_cfg = deepSID.config();
      auto *dir = new Deepsid::Dir( std::string(fname), &deepsid_cfg );
      log_d("%s is a JSON path, checking existence...", dir->jsonFile().c_str() );
      bool ret = fs->exists( dir->jsonFile().c_str() );
      delete dir;
      return ret;
    } else {
      return fs->exists( dirCachePath( fname ) ); // cached meta file
    }
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


  bool SongCacheManager::add( const char * path, SID_Meta_t *song, bool checkdupes )
  {
    init( path );
    if( songCacheFile && !checkdupes ) {
      log_d("[Cache Blind Insertion] : %s", song->name );
      return insert( song, true );
    }

    if( fs->exists( cachepath ) ) { // cache file exists !!
      if( !exists( song ) ) { // song is not in cache file
        log_d("[Cache Insertion]");
        return insert( song, true );
      }
      return true; // song is already in the cache file
    } else { // cache file does not exist
      log_d("[Cache Creation+Insertion]");
      return insert( song, false );
    }
  }


  int SongCacheManager::addSong( const char * path, bool checkdupes )
  {
    SID_Meta_t *song = (SID_Meta_t*)sid_calloc(1, sizeof(SID_Meta_t) );
    song->durations = nullptr;

    setCleanFileName( song->filename, 255, "%s", path );

    int ret = -1;

    if( sidPlayer->getInfoFromSIDFile( path, song ) ) {
      if( add( path, song, checkdupes ) ) {
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
        size_t objsize = bufsize;// + song->subsongs*sizeof(uint32_t);
        size_t bytes_written = cacheFile.write( (uint8_t*)song, objsize );
        ret = (bytes_written == objsize);
        break;
      }
      entry_index++;
    }
    delete SID_Meta_tbuf;
    cacheFile.close();
    return ret; // not found
  }

  /*

  bool SongCacheManager::get( const char* md5path, SID_Meta_t *song )
  {
    char *fullpath = (char*)sid_calloc( 256, sizeof(char) );
    // TODO: avoid double slashes in md5path
    snprintf( fullpath, 255, "%s/%s", SID_FOLDER, md5path );
    init( fullpath );
    free( fullpath );
    bool ret = false;
    log_e("Accessing Filesystem from core #%d", xPortGetCoreID() );
    fs::File cacheFile = fs->open( cachepath, FILE_READ );
    if( ! cacheFile ) return false;
    size_t bufsize = sizeof(SID_Meta_t);
    char *SID_Meta_tbuf =  new char[bufsize+1];
    while( cacheFile.readBytes( SID_Meta_tbuf, bufsize ) > 0 ) {
      SID_Meta_t* tmpsong = (SID_Meta_t*)SID_Meta_tbuf;
      if( strcmp( (const char*)tmpsong->filename, md5path ) == 0  ) {
        *song = SID_Meta_t(*tmpsong);
        //memcpy( song, tmpsong, bufsize );
        //song->durations = (uint32_t*)sid_calloc(song->subsongs, sizeof(uint32_t) );
        //cacheFile.readBytes( (char*)song->durations, song->subsongs*sizeof(uint32_t) );
        ret = true;
        break;
      }
    }
    cacheFile.close();
    delete SID_Meta_tbuf;
    return ret;
  }

  */


  bool SongCacheManager::scanPaginatedFolderCache( const char* pathname, size_t offset, size_t maxitems, folderElementPrintCb printCb, activityTickerCb tickerCb, size_t *count )
  {
    assert(count);
    String dirCacheFile = dirCachePath( pathname );
    if( dirCacheFile == "" ) {
      log_e("Bad dirCache filename");
      return false;
    }
    //log_e("Accessing Filesystem from core #%d", xPortGetCoreID() );
    if( ! fs->exists( dirCacheFile ) ) {
      log_e("Cache file %s does not exist, create it first !", dirCacheFile.c_str() );
    }
    fs::File DirCache = fs->open( dirCacheFile );
    if( !DirCache ) {
      log_e("Unable to access dirCacheFile %s", dirCacheFile.c_str() );
      return false;
    }
  /*
    if( getDirCacheSize( &DirCache ) == 0 ) {
      return false; // skip folder size line
    }
  */
    size_t folderSize = seekDirCacheSize( &DirCache ); // skip folder size line
    *count += folderSize;
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
        log_v("Paginating at offset %d (offset=%d, maxitems=%d)", currentoffset, offset, maxitems );
        printCb( itemName.c_str(), type );
      }

      currentoffset++;

    }
    _end:
    DirCache.close();
    return ret;
  }


  /*
  bool SongCacheManager::scanFolderCache( const char* pathname, folderElementPrintCb printCb, activityTickerCb tickerCb )
  {
    return scanPaginatedFolderCache( pathname, 0, 0, printCb, tickerCb );
  }
  */


  bool SongCacheManager::dirCacheUpdate( fs::File *dirCache, const char*_itemName, folderItemType_t type )
  {
    if( !dirCache) {
      log_e("No cache to update!");
      return false;
    }

    seekDirCacheSize( dirCache );
    //if( seekDirCacheSize( dirCache ) == 0 ) return false; // skip folder size line

    log_d("[%d] Will update item %s in cache %s", ESP.getFreeHeap(), fs_file_path(dirCache), _itemName );
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
        case F_SYMLINK:           // maybe a real file
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
      const char *dName = dirname( (char *)fs_file_path(dirCache) );
      const char *parent = dirname( (char *)dName );
      String parentCacheName = String( parent ) + "/.dircache";
      if( !fs->exists( parentCacheName ) ) {
        log_e("Can't update parent cache, file %s does not exist", parentCacheName.c_str() );
      } else {
        bool ret = false;
        fs::File parentDirCache = fs->open( parentCacheName, "r+" );
        if( !parentDirCache ) {
          log_e("Unable open parent cache file %s as R+, aborting", parentCacheName.c_str() );
          return false;
        }
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
      size_t written_bytes = 0;
      size_t expect_bytes = ( sizeof(uint8_t) + 1 + strlen( itemName ) + 1 );
      written_bytes += dirCache->write( (uint8_t)type );
      written_bytes += dirCache->write( ':' );
      written_bytes += dirCache->write( (uint8_t*)itemName, strlen( itemName ) ); // no null terminating
      written_bytes += dirCache->write( '\n' );
      if( written_bytes != expect_bytes ) {
        log_e("Write failed, expected %d bytes and got %d for item '%s'", expect_bytes, written_bytes, itemName );
        return false;
      }
    }
    return true;
  }



  void SongCacheManager::dirCacheSort(fs::File *dirCacheFile, size_t maxitems, activityTickerCb tickerCb )
  {
    size_t dirCacheFileSize = dirCacheFile->size();
    int avail_ram = ESP.getFreeHeap() - 100000; // don't go under 100k heap
    bool use_ram = avail_ram > dirCacheFileSize;
    size_t entries = 0;

    seekDirCacheSize( dirCacheFile ); // skip folder size line
    // if( seekDirCacheSize( dirCacheFile ) == 0 ) {
    //   log_e("Bad sorting request, aborting");
    //   return;
    // }

    if( use_ram  ) {
      // ram sort
      log_d("[%d] Ram sorting %s", ESP.getFreeHeap(), fs_file_path(dirCacheFile) );
      std::vector<String> thisFolder;
      while( dirCacheFile->available() ) {
        String blah = dirCacheFile->readStringUntil( '\n' );
        log_d("Feeding folder #%d with '%s'", thisFolder.size(), blah.c_str() );
        tickerCb( "Feed", thisFolder.size() );
        thisFolder.push_back( blah );
      }

      log_d("Collected %d items from file %s", thisFolder.size(), fs_file_path(dirCacheFile) );

      seekDirCacheSize( dirCacheFile ); // skip folder size line
      // if( seekDirCacheSize( dirCacheFile ) == 0 ) {
      //   log_e("Bad sorting request, aborting");
      //   return;
      // }
      //
      tickerCb( "Sort", thisFolder.size() );
      std::sort( thisFolder.begin(), thisFolder.end() );

      log_d("std::sorted %d entries in file %s (at offset %d)", thisFolder.size(), fs_file_path(dirCacheFile), dirCacheFile->position() );

      for( int i=0;i<thisFolder.size();i++ ) {
        size_t startpos = dirCacheFile->position();
        const char* folderData = thisFolder[i].c_str();
        size_t folderSize  = strlen( thisFolder[i].c_str() );
        dirCacheFile->write( (uint8_t*)folderData, folderSize );
        dirCacheFile->seek( startpos + folderSize  ); // dafuq ?
        dirCacheFile->write( '\n' );
        log_d("Written sorted dircache entry '%s' (%d bytes) at offsets [%d-%d]", folderData, folderSize, startpos, dirCacheFile->position() );
        tickerCb( "Save", entries );
        entries++;
      }

      log_d("Written %d entries in file %s", entries, fs_file_path(dirCacheFile) );

    } else {
      // disk sort
      log_e("[%d] TODO: slow sort on disk", ESP.getFreeHeap() );
    }
    tickerCb( "", entries );
    //dirCacheFile->close();
  }



  void SongCacheManager::deleteCache( const char* pathname )
  {
    if( cache_type == CACHE_DOTFILES ) {
      String dircachepath = dirCachePath( pathname );
      if( fs->exists( dircachepath ) )
        fs->remove( dircachepath );
    }
  }


  size_t SongCacheManager::getDirSize( const char* pathname )
  {
    if( cache_type == CACHE_DEEPSID ) {
      return getDeepsidCacheSize( pathname );
    } else {
      return getDirCacheSize( pathname );
    }
  }


  size_t SongCacheManager::getDeepsidCacheSize( const char* pathname )
  {
    if( deepsid_dir ) return deepsid_dir->size();
    return 0;
  }


  size_t SongCacheManager::getDirCacheSize( const char* pathname )
  {
    // if( cacheFile && strcmp( pathname, cacheFile->path() ) == 0 ) {
    //   return lastDirCacheSize;
    // }
    String dircachepath = dirCachePath( pathname );
    File cacheFile = fs->open( dircachepath );
    if( !cacheFile ) {
      log_e("Unable to open dirCachePath %s", pathname );
      return 0;
    }
    size_t cachesize = seekDirCacheSize( &cacheFile );
    cacheFile.close();
    return cachesize;
  }



  size_t SongCacheManager::seekDirCacheSize( fs::File *cacheFile )
  {
    uint8_t *sizeToByte = new uint8_t[sizeof(size_t)];
    cacheFile->seek(0);
    cacheFile->read( sizeToByte, sizeof(size_t) );
    size_t cachesize = 0;
    memcpy( &cachesize, sizeToByte, sizeof(size_t) );
    // skip separator
    if( cacheFile->read() != '\n' ) {
      log_e("[%s] Unexpected byte while thawing %d as %d bytes: %02x %02x %02x %02x (%s)", fs_file_path(cacheFile), cachesize, sizeof(size_t), sizeToByte[0], sizeToByte[1], sizeToByte[2], sizeToByte[3], (const char*)sizeToByte );
      delete sizeToByte;
      return -1;
    } else {
      log_d("Cache file %s claims %d elements", fs_file_path(cacheFile), cachesize );
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
    log_d("Cache file %s will claim %d elements", fs_file_path(dirCacheFile), totalitems );
    delete sizeToByte;
    //dircachesize = totalitems;
  }


  /*
    struct download_cb_t
    {
      net_cb_t onHTTPBegin{net_cb_default};
      net_cb_t onHTTPGet{net_cb_default};
      net_cb_t onFileOpen{net_cb_default};
      net_cb_t onFileWrite{net_cb_default};
      net_cb_t onFileClose{net_cb_default};
      net_cb_t onSuccess{net_cb_default};
      net_cb_t onFail{net_cb_default};
    };
  */


  static activityTickerCb staticTickerCb = nullptr;
  static uint8_t staticTickerProgress = 0;

  void deepsid_cb_main( std::string tickerMsg, std::string logFmt, std::string logVal )
  {
    if(staticTickerCb) staticTickerCb( tickerMsg.c_str(), staticTickerProgress );
    log_d( "%s: %s", logFmt.c_str(), logVal.c_str() );
  }

  void deepsid_onHTTPBegin(const std::string out="") { deepsid_cb_main( "HTTP Begin", "Opening URL", out ); }
  void deepsid_onHTTPGet(const std::string out="")   { deepsid_cb_main( "HTTP Get", "Sending request", out ); }
  void deepsid_onFileOpen(const std::string out="")  { deepsid_cb_main( "Opening", "Will Save file to", out ); }
  void deepsid_onFileWrite(const std::string out="") { deepsid_cb_main( "Saving", "Writing file to", out ); }
  void deepsid_onFileClose(const std::string out="") { deepsid_cb_main( "Saved", "Writing file to", out ); }
  void deepsid_onSuccess(const std::string out="")   { deepsid_cb_main( "Done!", "Successfully written file to", out ); }
  void deepsid_onFail(const std::string out="")      { deepsid_cb_main( "Fail :(", "Failed to writte file to", out ); }


  size_t SongCacheManager::syncDeepSIDFiles( activityTickerCb tickerCb )
  {
    if( deepsid_dir == nullptr ) {
      log_e("No deepsid dir to scan, aborting");
      return 0;
    }

    size_t files_count = deepsid_dir->files_count();
    size_t itemsCount  = 0, syncCount = 0, skipCount = 0, ignoredCount = 0;

    if( files_count == 0 ) {
      log_e("This directory has no files");
      // TODO: go recursive
      return 0;
    }

    staticTickerCb = tickerCb;
    net::download_cb_t DownloaderCallbacks(&deepsid_onHTTPBegin, &deepsid_onHTTPGet, &deepsid_onFileOpen, &deepsid_onFileWrite, &deepsid_onFileClose, &deepsid_onSuccess, &deepsid_onFail);

    auto item = deepsid_dir->openNextFile();

    net::wifiOn();

    while( item.type != Deepsid::TYPE_NONE ) {
      itemsCount++;
      if( item.type == Deepsid::TYPE_FILE  && Deepsid::ends_with( item.name(), ".sid") ) {
        std::string base_url  = item.url();
        std::string base_path = utils::pathConcat( SID_FOLDER, item.path( true ) );
        if( !fs->exists( base_path.c_str() ) ) {
          //log_d("%s does not exist, will fetch @ %s", base_path.c_str(), base_url.c_str() );
          if( ! net::downloadFile( base_url, fs, base_path, DownloaderCallbacks ) ) {
            log_e("[ABORT] %s failed to be fetched from %s", base_path.c_str(), base_url.c_str() );
            net::wifiOff();
            return syncCount;
          }
          syncCount++;
          tickerCb("Sync", syncCount );
        } else {
          skipCount++;
          tickerCb("Skip", skipCount );
        }
      } else {
        ignoredCount++;
        tickerCb("Zap!", ignoredCount );
      }
      item = deepsid_dir->openNextFile();
    }

    net::wifiOff();

    return syncCount;
  }



  bool SongCacheManager::scanPaginatedFolder( const char* pathname, folderElementPrintCb printCb, activityTickerCb tickerCb, size_t *count, size_t offset, size_t maxitems )
  {
    if( cache_type == CACHE_DEEPSID ) {
      return scanPaginatedDeepSIDFolder( pathname, printCb, tickerCb, count, offset, maxitems );
    } else {
      return scanPaginatedLocalFolder( pathname, printCb, tickerCb, count, offset, maxitems );
    }
  }


  bool SongCacheManager::scanFolder( const char* pathname, folderElementPrintCb printCb, activityTickerCb tickerCb, size_t *count, size_t maxitems, bool force_regen )
  {
    //lastDirCacheSize = 0;
    if( cache_type == CACHE_DEEPSID ) {
      return scanDeepSIDFolder( pathname, printCb, tickerCb,count,  maxitems, force_regen );
    } else {
      return scanLocalFolder( pathname, printCb, tickerCb, count, maxitems, force_regen );
    }
  }



  Deepsid::Dir *SongCacheManager::getDeepsidDir(const char* pathname, activityTickerCb tickerCb )
  {
    bool new_dir = false;
    if( deepsid_dir && strcmp( deepsid_dir->path().c_str(), pathname ) != 0 ) {
      log_d("Deepsid folder changed (old: %s, new: %s)", deepsid_dir->path().c_str(), pathname );
      delete deepsid_dir;
      new_dir = true;
    } else if( !deepsid_dir ) {
      log_d("Deepsid folder creation: %s", pathname );
      new_dir = true;
    } else {
      log_d("Deepsid cache hit: %s", pathname );
    }
    if(new_dir) deepsid_dir = deepSID.FS->openDir( pathname, false, tickerCb );
    return deepsid_dir;
  }




  bool SongCacheManager::scanPaginatedDeepSIDFolder( const char* pathname, folderElementPrintCb printCb, activityTickerCb tickerCb, size_t *count, size_t offset, size_t maxitems )
  {
    assert(count);
    //bool new_dir = false;
    size_t deepsid_offset = *count;

    // if( deepsid_dir && strcmp( deepsid_dir->path().c_str(), pathname ) != 0 ) {
    //   log_d("Deepsid folder changed (old: %s, new: %s)", deepsid_dir->path().c_str(), pathname );
    //   delete deepsid_dir;
    //   new_dir = true;
    // } else if( !deepsid_dir ) {
    //   log_d("Deepsid folder creation: %s", pathname );
    //   new_dir = true;
    // } else {
    //   log_d("Deepsid cache hit: %s", pathname );
    // }
    //
    // if(new_dir) deepsid_dir = deepSID.FS->openDir( pathname, false, tickerCb );

    auto dir = getDeepsidDir( pathname, tickerCb );

    if( dir == nullptr ) return false; // failed to load

    *count += dir->size();

    dir->rewind();
    size_t itemsCount = offset==0 ? deepsid_offset : 0;

    if( offset > 0 ) {
      dir->forward( offset-deepsid_offset );
    } else {
      if( String(pathname)!="/" && dir->files_count()>0 ) {
        //printCb( pathname, F_PLAYLIST );
        //*count = *count+1;
        //itemsCount++;
      }
    }

    auto item = dir->openNextFile();


    while( item.type != Deepsid::TYPE_NONE ) {
      //log_d("Item url: %s", item.url().c_str() );
      if( item.type == Deepsid::TYPE_FOLDER ) {
        //log_d("[FOLDER] %s", item.attr["foldername"].c_str() );
        printCb( item.path().c_str(), F_DEEPSID_FOLDER );
      } else if( item.type == Deepsid::TYPE_FILE  /*&& Deepsid::ends_with( item.name(), ".sid")*/ ) {
        //log_d("[FILE] %s", item.path().c_str() );
        std::string base_path = utils::pathConcat( SID_FOLDER, item.path(true) );
        // if( ! utils::starts_with( base_path, std::string(SID_FOLDER) ) ) {
        //   // prepend local SID folder name
        //   base_path = utils::pathConcat( SID_FOLDER, base_path );
        // }
        printCb( base_path.c_str(), Deepsid::ends_with( item.name(), ".sid") ? F_SID_FILE : F_UNSUPPORTED_FILE );
      }

      itemsCount++;

      log_d("[Count=%3d:%-3d] Item path: %s, rewritten: %s", offset, itemsCount, item.path().c_str(), item.path(true).c_str() );

      if( maxitems != 0 && itemsCount>=maxitems ) {
        log_d("Maxitems reached");
        break;
      }

      //printCb( item.path().c_str(), item.type == Deepsid::TYPE_FOLDER ? F_DEEPSID_FOLDER : F_SID_FILE );

      item = dir->openNextFile();
    }

    return true;
  }


  bool SongCacheManager::scanPaginatedLocalFolder( const char* pathname, folderElementPrintCb printCb, activityTickerCb tickerCb, size_t *count, size_t offset, size_t maxitems )
  {
    assert( count );
    fs::File root = fs->open( pathname );
    if(!root){
      log_e("Fatal: failed to open %s directory\n", pathname);
      return false;
    }
    if(!root.isDirectory()){
      log_e("Fatal: %s is not a directory\b", pathname);
      return false;
    }

    if( offset == 0 ) {
      folder_has_playlist = folderHasPlaylist( pathname );
      if( folder_has_playlist ) {
        // insert it now as it'll be ignored
        //printCb( pathname, F_PLAYLIST );
        //if( maxitems>=*count) maxitems-=*count;
        //*count = *count+1;
      }
    } else {
      if( folder_has_playlist ) {
        //*count = *count+1;
        //if( maxitems>=*count) maxitems-=*count;
      }
    }

    root.close();

    bool ret = scanPaginatedFolderCache( pathname, offset, maxitems, printCb, tickerCb, count );

    return ret;
  }




  bool SongCacheManager::scanDeepSIDFolder( const char* pathname, folderElementPrintCb printCb, activityTickerCb tickerCb, size_t *count, size_t maxitems, bool force_regen )
  {
    if( force_regen ) {
      if( deepsid_dir ) {
        deepsid_dir->close();
        delete deepsid_dir;
        deepsid_dir = nullptr;
      }
      deepsid_dir = deepSID.FS->openDir( pathname, true, tickerCb );
    }

    //printCb( pathname, F_PLAYLIST );
    //maxitems = maxitems>0 ? maxitems-1 : 0;

    return scanPaginatedDeepSIDFolder( pathname, printCb, tickerCb, count, 0, maxitems );

  }


  bool SongCacheManager::scanLocalFolder( const char* pathname, folderElementPrintCb printCb, activityTickerCb tickerCb, size_t *count, size_t maxitems, bool force_regen )
  {
    assert(*count);
    fs::File root = fs->open( pathname );
    if( ! root ) {
      log_e("Fatal: failed to open '%s' directory from core #%d", pathname, xPortGetCoreID() );
      return false;
    }
    if(!root.isDirectory()){
      log_e("Fatal: %s is not a directory\b", pathname);
      return false;
    } else {
      log_e("Successfully opened dir %s for scanning", pathname );
    }

    fs::File DirCache;

    folder_has_playlist = folderHasPlaylist( pathname );
    folder_has_cache    = folderHasCache( pathname );

    bool build_cache = false;
    //bool cache_built = false;

    if( force_regen ) {
      purge( pathname );
      folder_has_playlist = false;
      folder_has_cache = false;
    }

    size_t totalitems = 0;

    if( folder_has_playlist ) {
      // insert it now as it'll be ignored
      //printCb( pathname, F_PLAYLIST );
      totalitems = *count;
      //if( count!=nullptr ) *count = *count+1;
    } else {
      build_cache = true;
    }

    if( folder_has_cache ) {
      root.close();
      log_d("[%d] Serving cached folder", ESP.getFreeHeap() );
      //bool ret = scanFolderCache( pathname, printCb, tickerCb );
      bool ret = scanPaginatedFolderCache( pathname, 0, 0, printCb, tickerCb, count );
      log_d("[%d] Scanned folder cache", ESP.getFreeHeap() );
      return ret;
    } else {
      log_d("Generating cached folder (init to size_t 0) for path '%s'", pathname);
      DirCache = fs->open( dirCachePath( pathname ), FILE_WRITE );
      setDirCacheSize( &DirCache, 0 );
    }

    File file = root.openNextFile();
    while( file ) {
      if( file.isDirectory()  ) {
        if( folderHasPlaylist( fs_file_path(&file) ) ) {
          dirCacheAdd( &DirCache, fs_file_path(&file), F_SUBFOLDER );
        } else {
          dirCacheAdd( &DirCache, fs_file_path(&file), F_SUBFOLDER_NOCACHE );
        }
        totalitems++;
      } else {
        // handle both uppercase and lowercase of the extension
        String fName = String( fs_file_path(&file) );
        fName.toLowerCase();

        char cleanfilename[255] = {0};
        setCleanFileName( cleanfilename, 255, "%s", fs_file_path(&file) );

        if( fName.endsWith( ".sid" ) && !file.isDirectory() ) {
          dirCacheAdd( &DirCache, cleanfilename, F_SID_FILE );

          /*
          if( build_cache ) {
            // eligibility for .sidcache ?
            if( addSong( cleanfilename ) > 0 ) {
              cache_built = true;
            }
          }
          */
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
      vTaskDelay(1); // feed the dog
    }

    DirCache.close();

    // reopen as r+ mode to update cache size
    DirCache = fs->open( dirCachePath( pathname ), "r+" );
    setDirCacheSize( &DirCache, totalitems );
    DirCache.close();

    // reopen as r+ mode to sort cache content
    DirCache = fs->open( dirCachePath( pathname ), "r+" );
    dirCacheSort( &DirCache, maxitems, tickerCb );
    // create SID Cache from sorted content
    if( build_cache && buildSidCacheFromDirCache( &DirCache, tickerCb ) ) {
      if( songCacheFile )
        songCacheFile.close();
      //if( folderHasPlaylist( pathname ) ) {
        dirCacheAdd( &DirCache, pathname, F_PLAYLIST );
      //}
    }

    DirCache.close();
    root.close();

    return scanPaginatedFolder( pathname, printCb, tickerCb, count, 0, maxitems );
  }



  bool SongCacheManager::buildSidCacheFromDirCache( fs::File *sortedDirCache, activityTickerCb tickerCb )
  {
    sortedDirCache->seek(0);

    if( seekDirCacheSize( sortedDirCache ) == 0 ) {
      log_e("Dir cache is empty!");
      return false; // skip folder size line
    }
    log_d("Building SidCache for dir %s", sortedDirCache->name() );
    bool cache_built = false;

    //size_t currentoffset = 0;
    size_t saved = 0;

    while( sortedDirCache->available() ) {
      uint8_t typeint = sortedDirCache->read();
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
      if( sortedDirCache->read() != ':' ) {
        // EOF!
        goto _end;
      }
      String itemName = sortedDirCache->readStringUntil('\n');
      if( itemName == "" ) {
        // bad dircache file
        goto _end;
      }
      if( type == F_SID_FILE || type == F_PLAYLIST ) {
        if( addSong( itemName.c_str(), saved==0 ) > 0 ) {
          cache_built = true;
          saved++;
        }
        tickerCb("Cache", saved );
      }
      vTaskDelay(1); // feed the dog
    }

    _end:

    return cache_built;

  }


  void SongCacheManager::makeCachePath( const char* _cachepath, char* cachepath )
  {
    if( !String( _cachepath ).endsWith("/.sidcache") ) {
      log_v("Stray _cachepath: %s (will get .sidcache appended)", _cachepath );
      if( _cachepath[strlen(_cachepath)-1] == '/' ) {
        snprintf( cachepath, 255, "%s.sidcache", _cachepath );
      } else {
        snprintf( cachepath, 255, "%s/.sidcache", _cachepath );
      }
    } else {
      snprintf( cachepath, 255, "%s", _cachepath );
    }
  }




  // for playlist handling
  bool SongCacheManager::getInfoFromItem( const char* _cachepath, size_t itemNum )
  {
    if( cache_type == CACHE_DEEPSID ) {
      return getInfoFromDeepsidItem( _cachepath, itemNum );
    } else {
      return getInfoFromCacheItem( _cachepath, itemNum );
    }
  }


  bool SongCacheManager::getInfoFromDeepsidItem( const char* _cachepath, size_t itemNum )
  {
    makeCachePath( _cachepath, cachepath );
    return getInfoFromDeepsidItem( itemNum );
  }


  bool SongCacheManager::getInfoFromDeepsidItem( size_t itemNum )
  {
    if( deepsid_dir == nullptr ) {
      CacheItemSize = 0;
      return false;
    }

    CacheItemSize = deepsid_dir->size();

    deepsid_dir->rewind();

    if( itemNum>0 ) {
      deepsid_dir->forward( itemNum );
    }

    auto item = deepsid_dir->openNextFile();
    bool ret = false;
    if( item.type == Deepsid::TYPE_FILE ) {
      //if( itemNum == 0 ) CacheItemSize = item.dir->size();
      CacheItemNum  = itemNum;
      ret = item.getSIDMeta( item, currenttrack );
    }
    //delete dir;
    return ret;
  }



  bool SongCacheManager::getInfoFromCacheItem( const char* _cachepath, size_t itemNum )
  {
    makeCachePath( _cachepath, cachepath );
    return getInfoFromCacheItem( itemNum );
  }


  // load cached song information from a .sidcache file into the sidPlayer
  bool SongCacheManager::getInfoFromCacheItem( size_t itemNum )
  {

    if( sidPlayer->isPlaying() ) {
      sidPlayer->stop(); // prevent concurrent SPI access
    }

    delay(200);
    log_v("Accessing FS from core #%d", xPortGetCoreID() );

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
    log_v("Cache file %s (%d bytes) estimated to %d entries (average %d bytes each)", cachepath, filesize, filesize/bufsize, bufsize );
    char *SID_Meta_buf =  new char[bufsize+1];
    size_t itemCount = 0;
    bool ret = false;

    if( itemNum > 0 ) {
      cacheFile.seek( itemNum*bufsize );
    }

    while( cacheFile.available() ) {
      cacheFile.readBytes( SID_Meta_buf, bufsize );// == bufsize
      itemCount++;
      // seek cache item
      if( itemCount-1 != itemNum ) {
        continue;
      }
      log_v("Found the item");

      if( currenttrack != nullptr ) {
        if( currenttrack->durations != nullptr ) {
          log_v("Track has durations, freeing");
          free( currenttrack->durations  );
        }
        log_v("Track needs freeing");
        free( currenttrack );
      }

      currenttrack = (SID_Meta_t *)sid_calloc(1,sizeof(SID_Meta_t));

      if( currenttrack == NULL ) {
        log_e("Unable to alloc %d bytes, aborting", sizeof(SID_Meta_t) );
        return false;
      } else {
        log_v("Copying %d bytes to a %d wide array", sizeof(SID_Meta_t), bufsize+1 );
        *currenttrack = SID_Meta_t(*((SID_Meta_t*)SID_Meta_buf));
        //memcpy( currenttrack, (const unsigned char*)SID_Meta_buf, sizeof(SID_Meta_t) );
        if( !String(currenttrack->filename).startsWith( SID_FOLDER ) ) {
          // insert SID_FOLDER before path
          log_v("Insert SID_FOLDER before path");
          memmove( currenttrack->filename+strlen(SID_FOLDER), currenttrack->filename, strlen(currenttrack->filename)+strlen(SID_FOLDER) );
          memcpy( currenttrack->filename, SID_FOLDER, strlen( SID_FOLDER ) );
        }
        // alloc variable memory for subsong durations
        currenttrack->durations = nullptr; // (uint32_t*)sid_calloc( sidPlayer->currenttrack->subsongs, sizeof(uint32_t) );
        //cacheFile.readBytes( (char*)sidPlayer->currenttrack->durations, sidPlayer->currenttrack->subsongs*sizeof(uint32_t) );
        //songdebug( sidPlayer->currenttrack );
        ret = true;
      }
      if( itemNum > 0 ) break;
    }
    delete SID_Meta_buf;
    cacheFile.close();
    if( itemNum == 0 ) CacheItemSize = itemCount;
    CacheItemNum  = itemNum;
    log_v("Loaded %d subsongs (cache size: %d)", currenttrack->subsongs, CacheItemSize );
    //sidPlayer->setSongstructCachePath( SID_FS, cachepath );
    return ret;
  }








  #if defined ENABLE_HVSC_SEARCH


  // ************************
  // keywords / path indexing
  // ************************


  // callback type = searchResultInsertCb
  bool SongCacheManager::resultInserter( sidpath_t* sp, int rank )
  {
    bool found = false;
    if( sp->type == F_SUBFOLDER ) rank *= 10; // folders first
    for( int i=0; i<searchResults.size(); i++ ) {
      if( searchResults[i].sidpath.offset == sp->offset ) {
        found = true;
        searchResults[i].rank += rank; // increase rank
        break;
      }
    }
    if( !found ) {
      // create entry
      sidpath_rank_t sr;
      sr.sidpath.offset = sp->offset;
      sr.sidpath.type   = sp->type;
      sr.rank = rank; // init rank
      searchResults.push_back( sr );
      return true;
    }
    return false;
  }


  bool SongCacheManager::buildSIDWordsIndex( const char* folderPath, activityTickerCb _ticker,  activityProgressCb _progress )
  {

    if( !psramInit() ) {
      log_e("Unable to init psram");
      return false;
    }
    if( _ticker != nullptr ) {
      log_w("Setting up activity ticker");
      ticker = _ticker;
    }
    if( _progress != nullptr ) {
      progress = _progress;
    }
    // create keywords dictionary from songlengths (memory)
    bool kwret = parseKeywords( true, true, 4, 6 );
    if( !kwret ) {
      log_e("Something went wrong while parsing keywords, aborting");
      return false;
    }
    // spread dictionary in shards (memory)
    keywordsToShards();
    // remove existing shards from filesystem
    bool ret = cleanupShards();
    if( !ret ) return false;
    // create shard files
    size_t total_written = 0;

    for( auto const& item : shards ) {
      size_t shard_written = 0;
      const char shardname = item.first;
      shard_t* shard = item.second;
      const char shardnameCStr[2] = {shardname,'\0'};

      log_w("[Letter '%s' has %d words]", shardnameCStr, shard->wordscount );
      // alpha sort shard->words
      shard->sortWords();

      for( int i=0;i<shard->wordscount;i++ ) {

        wpio->set( shard->words[i] );
        wpio->sortPaths(); // type-sort paths, kinda useless if only folders are indexed

        size_t path_written = wpio->writeShard();

        if( path_written == 0 ) {
          log_e("Empty write for path #%d / shard '%s'", i, shardnameCStr );
        } else {
          log_v("Wrote %d bytes for path #%d / shard '%s'", path_written, i, shardnameCStr );
        }
        shard_written += path_written;
        vTaskDelay(1);
      }
      if( shard_written == 0 ) {
        log_e("Empty write for shard '%s'", shardnameCStr );
      }
      total_written += shard_written;
      log_v("written %d bytes (total=%d so far)", shard_written, total_written );
    }

    // close last shard file
    wpio->closeShardFile();
    wpio->listShardYard();

    /*
    uint8_t g1 = wpio->guessShardNum( "wodnik" ); // real (true positive)
    uint8_t g2 = wpio->guessShardNum( "142" ); // real (true positive)
    uint8_t g3 = wpio->guessShardNum( "456" ); // guessed (false positive)
    uint8_t g4 = wpio->guessShardNum( "bac" ); // guessed (true negative, not in list)
    uint8_t g5 = wpio->guessShardNum( "zzzzzz" ); // guessed (true negative, outside list)
    log_w("Guess results: %x, %x, %x, %x, %x", g1, g2, g3, g4,g5 );
    */

    if( !wpio->saveShardYard() ) {
      log_e("Unable to save shardyard");
      return false;
    }

    wpio->freeShardYard();

    return true;
  }



  bool SongCacheManager::loadShardsIndex()
  {
    if( shardsIdx.size() > 0 ) {
      log_w("Shards index already loaded!");
      return true;
    }
    if( !fs->exists( SHARDS_INDEX ) ) {
      log_e("No shards index to load, reset wcache first!");
      return false;
    }
    fs::File shardIdxFile = fs->open( SHARDS_INDEX );
    if( !shardIdxFile ) {
      log_e("Unable to open shard index file %s", SHARDS_INDEX );
      return false;
    }
    size_t idx_size = 0;
    size_t freeMemBefore = ESP.getFreePsram();
    while( shardIdxFile.available() ) {
      const char shardname = shardIdxFile.read();
      size_t wordscount = 0;
      size_t read_bytes = shardIdxFile.readBytes( (char*)&wordscount, sizeof(size_t));
      if( read_bytes == sizeof(size_t) ) {
        shardsIdx[shardname] = wordscount;
        idx_size += read_bytes;
      } else {
        log_e("Bad content, expecting %d bytes, got %d, aborting read", sizeof(size_t), read_bytes );
        break;
      }
    }
    shardIdxFile.close();
    log_w("[Before:%d, After:%d], Loaded %d bytes", freeMemBefore, ESP.getFreePsram(), idx_size );
    return idx_size > 0;
  }


  #if 0
  bool SongCacheManager::deepSidFind( const char* sentence, folderElementPrintCb printCb, size_t offset, size_t maxitems )
  {
    searchResults.clear();

    /*
    size_t sentence_len = strlen( sentence );

    if( strstr( sentence, " " ) == NULL ) {
      // simple word search
      if(sentence_len<4) maxitems = 8; // don't spam
      // TODO: implement Deepsid finder
      // if( !findFolder( sentence, nullptr, offset, maxitems ) ) return false;
    } else {
      bool found = false;
      char* sentence_arr = (char*)sid_calloc( sentence_len+1, sizeof( char ) );
      if( sentence_arr == NULL ) {
        log_e("Can't alloc %d bytes for sentence", sentence_len+1 );
        return false;
      }
      memcpy( sentence_arr, sentence, sentence_len );
      char* token = strtok(sentence_arr, " ");
      // loop through the string to extract all other tokens
      while( token != NULL ) {
        // TODO: implement Deepsid finder
        // if ( findFolder( String(token).c_str(), nullptr, offset, maxitems*2 ) ) found = true;
        token = strtok(NULL, " ");
      }
      if( !found ) return false;
    }

    // sort results by offset address to reduce seek distance while populating
    std::sort(searchResults.begin(), searchResults.end(), [=](sidpath_rank_t a, sidpath_rank_t b) {
      return a.sidpath.offset < b.sidpath.offset;
    });

    File md5file = fs->open( MD5_FILE );
    if( !md5file ) {
      log_e("Unable to open %s", MD5_FILE );
      return false;
    }

    // gather paths from offsets
    for( int i=0;i<searchResults.size();i++ ) {
      md5file.seek( searchResults[i].sidpath.offset );
      String fname  = md5file.readStringUntil('\n'); // read filename
      if( fname[0]!=';') {
        log_e("Invalid read at offset %d", md5file.position() );
        break; // something wrong, not a valid offset
      }
      fname.replace("; ", "");
      if( searchResults[i].sidpath.type == F_SUBFOLDER ) {
        String bname = gnu_basename( fname );
        fname = fname.substring(0, fname.length() - ( bname.length() + 1 ) );
      }
      log_d("[Result #%d] '%s'", i, fname.c_str() );
      snprintf( searchResults[i].path, 255, "%s%s", SID_FOLDER, fname.c_str() );
    }

    log_d("sorting by rank");
    // sort by rank first for pertinence
    std::sort(searchResults.begin(), searchResults.end(), [=](sidpath_rank_t a, sidpath_rank_t b) {
      return a.rank > b.rank;
      //if( a.rank == b.rank ) return a.sidpath->offset < b.sidpath->offset; // same rank = alphabetical order
      //if( a.rank > b.rank ) return true; // higher rank = hoist
      //return a.sidpath->offset < b.sidpath->offset;
    });

    int printed = 0;
    for( int i=0;i<searchResults.size();i++ ) {
      if( i >= offset && printed+1 < maxitems ) {
        printCb( searchResults[i].path, searchResults[i].sidpath.type );
        printed++;
      }
    }

    md5file.close();
    */
    return true;
  }
  #endif



  bool SongCacheManager::find( const char* sentence, folderElementPrintCb printCb, size_t offset, size_t maxitems )
  {
    searchResults.clear();

    size_t sentence_len = strlen( sentence );

    if( strstr( sentence, " " ) == NULL ) {
      // simple word search
      if(sentence_len<4) maxitems = 8; // don't spam
      if( !findFolder( sentence, nullptr, offset, maxitems ) ) return false;
    } else {
      bool found = false;
      char* sentence_arr = (char*)sid_calloc( sentence_len+1, sizeof( char ) );
      if( sentence_arr == NULL ) {
        log_e("Can't alloc %d bytes for sentence", sentence_len+1 );
        return false;
      }
      memcpy( sentence_arr, sentence, sentence_len );
      char* token = strtok(sentence_arr, " ");
      // loop through the string to extract all other tokens
      while( token != NULL ) {
        if ( findFolder( String(token).c_str(), nullptr, offset, maxitems*2 ) ) found = true;
        token = strtok(NULL, " ");
      }
      if( !found ) return false;
    }

    // sort results by offset address to reduce seek distance while populating
    std::sort(searchResults.begin(), searchResults.end(), [=](sidpath_rank_t a, sidpath_rank_t b) {
      return a.sidpath.offset < b.sidpath.offset;
    });

    File md5file = fs->open( MD5_FILE );
    if( !md5file ) {
      log_e("Unable to open %s", MD5_FILE );
      return false;
    }

    // gather paths from offsets
    for( int i=0;i<searchResults.size();i++ ) {
      md5file.seek( searchResults[i].sidpath.offset );
      String fname  = md5file.readStringUntil('\n'); // read filename
      if( fname[0]!=';') {
        log_e("Invalid read at offset %d", md5file.position() );
        break; // something wrong, not a valid offset
      }
      fname.replace("; ", "");
      if( searchResults[i].sidpath.type == F_SUBFOLDER ) {
        String bname = gnu_basename( fname );
        fname = fname.substring(0, fname.length() - ( bname.length() + 1 ) );
      }
      log_d("[Result #%d] '%s'", i, fname.c_str() );
      snprintf( searchResults[i].path, 255, "%s%s", SID_FOLDER, fname.c_str() );
    }

    log_d("sorting by rank");
    // sort by rank first for pertinence
    std::sort(searchResults.begin(), searchResults.end(), [=](sidpath_rank_t a, sidpath_rank_t b) {
      return a.rank > b.rank;
      //if( a.rank == b.rank ) return a.sidpath->offset < b.sidpath->offset; // same rank = alphabetical order
      //if( a.rank > b.rank ) return true; // higher rank = hoist
      //return a.sidpath->offset < b.sidpath->offset;
    });

    int printed = 0;
    for( int i=0;i<searchResults.size();i++ ) {
      if( i >= offset && printed+1 < maxitems ) {
        printCb( searchResults[i].path, searchResults[i].sidpath.type );
        printed++;
      }
    }

    md5file.close();
    return true;
  }




  bool SongCacheManager::findFolder( const char* keyword, folderElementPrintCb printCb, size_t offset, size_t maxitems, bool exact )
  {

    /*
    if( SidDictSize == 0 || SidDict == nullptr ) {
      if( ! loadShardsIndex() ) {
        log_e("Unable to load shards index, reset wcache first?");
        return;
      }
    }
    */

    if( !wpio->loadShardYard() ) {
      log_e("Unable to load shardyard, aborting");
      return false;
    }

    uint8_t shardId = wpio->guessShardNum( keyword );
    if( shardId == 0xff ) {
      log_w("keyword %s appears in no shard so far", keyword);
      return false;
    }

    size_t results = wpio->findKeyword( shardId, keyword, offset, maxitems, exact, insertResult);
    log_w("Search yielded %d results", results);
    return results > 0;

  /*

    // TODO: get shardname from real index
    const char shardname = keyword[0];
    if( ! loadShard( shardname ) ) {
      log_e("Unable to load shard '%s', incomplete wcache reset?", String(shardname).c_str() );
      return;
    }

    size_t found = 0;
    size_t kept = 0;
    String keywordStr = String(keyword);
    //std::vector<sidpath_t*> offsets;

    // look in that particular shard first
    {
      const shard_t* shard = shards[shardname];
      log_w("[Free:%d] Shard '%s' has %d words", ESP.getFreePsram(), String(shardname).c_str(), shard->wordscount );
      for(int i=0;i<shard->wordscount;i++) {
        wordpath_t* wp = shard->words[i];
        int rank = 0;
        if( strcmp( wp->word, keyword ) == 0 ) rank = 100;
        else if( strstr( wp->word, keyword ) !=NULL ) rank = 33;
        if( rank > 0 ) {
          for( int j=0;j<wp->pathcount;j++ ) {
            found++;
            if( found >= offset ) {
              if( kept+1 < maxitems ) {
                kept += insertOffset( wp->paths[j], rank ) ? 1 : 0;
              }
            }
            if( kept >= maxitems ) return;
          }
        }
      }
    }

    if( kept == 0 || !exact ) {
      // search whole shardyard
      for( auto const& item : shards ) {
        const char sn = item.first;
        if( shardname == sn ) continue; // skip already searched shard
        const shard_t* shard = item.second;
        for(int i=0;i<shard->wordscount;i++) {
          wordpath_t* wp = shard->words[i];
          int rank = 10;
          if( strstr( wp->word, keyword ) !=NULL ) {
            for( int j=0;j<wp->pathcount;j++ ) {
              found++;
              if( found >= offset ) {
                if( kept+1 < maxitems ) {
                  kept += insertOffset( wp->paths[j], rank ) ? 1 : 0;
                }
              }
              if( kept >= maxitems ) return;
            }
          }
        }
      }
    }

    log_w("[Free:%d] Search yielded %d total results (max=%d)", ESP.getFreePsram(), kept, maxitems);
  */
  }



  void SongCacheManager::clearDict()
  {
    /*
    for( int i=SidDictSize; i>0; i-- ) {
      if( SidDict[i] != NULL ) {
        SidDict[i]->clear();
        SidDict[i] = NULL;
      }
    }
    if( SidDict != NULL ) {
      free(SidDict);
      SidDict = NULL;
    }
    SidDictSize = 0;
    SidDictMemSize = 0;
    */
  }



  bool SongCacheManager::parseKeywords( bool parse_files, bool parse_folders, size_t KeywordMinLenFolder,  size_t KeywordMinLenFile )
  {
    File md5file = fs->open( MD5_FILE );
    if( !md5file ) {
      log_e("Unable to open %s", MD5_FILE );
      return false;
    }
    String fname, md5line, dirName, lastDirName = "";
    size_t fsize = md5file.size();
    size_t words_count = 0;
    size_t folders_count = 0;
    size_t files_count = 0;
    size_t keywords_in_folder = 0;

    wpio->resetBanMap();

    while( md5file.available() ) {
      int64_t itemoffset = md5file.position();
      fname  = md5file.readStringUntil('\n'); // read filename
      if( fname.c_str()[0] == '[' ) continue; // skip header
      md5line = md5file.readStringUntil('\n'); // read md5 hash and song lengths
      if( fname[0]!=';') {
        log_w("Invalid read at offset %d", md5file.position() );
        break; // something wrong, not a valid offset
      }
      fname.replace("; ", "");
      String bname = gnu_basename( fname );
      dirName = fname.substring( 0, fname.length() - (bname.length()+1) ); // +1 is for the trailing slash
      if( dirName != lastDirName ) {
        size_t wcount = 0;
        if( parse_folders ) {
          wpio->keywordsInfolder.clear();
          wcount = indexItemWords( dirName.c_str(), F_SUBFOLDER, itemoffset, KeywordMinLenFolder ); // index folder names with min keyword len=3
        }
        words_count += wcount;
        if( lastDirName != "" ) {
          Serial.printf( "    %-32s | +%d\n", lastDirName.c_str(), keywords_in_folder );
        }
        lastDirName = dirName;
        keywords_in_folder = wcount;
        folders_count++;
      } else {
        files_count++;
        keywords_in_folder++;
      }
      if( parse_files ) {
        words_count += indexItemWords( fname.c_str(), F_SID_FILE, itemoffset, KeywordMinLenFile ); // index file names with min keyword len=4
      }
      if( progress ) {
        progress( itemoffset, fsize, words_count, files_count, folders_count );
      }
    }
    wpio->keywordsInfolder.clear();
    Serial.printf( "    %-32s | +%d\n", lastDirName.c_str(), keywords_in_folder );
    md5file.close();
    log_w("[*] Parsing produced %d unique keywords from {%d files, %d folders}", words_count, files_count, folders_count );
    log_w("[*] MemFree=%d, SidDictSize = %d", ESP.getFreePsram(), SidDictSize );

    for( auto item : ban_map ) {
      size_t kwlen = item.first;
      std::sort( ban_map[kwlen].begin(), ban_map[kwlen].end(), [=]( ban_keyword_count_t a, ban_keyword_count_t b ) {
        return a.count > b.count;
      });
      Serial.printf("\n // Len = %d\n", item.first );
      for( int i=0; i<ban_map[kwlen].size(); i++ ) {
        if( i%8==0 ) Serial.println();
        Serial.printf(" {\"%s\",%d},", ban_map[kwlen][i].word.c_str(), ban_map[kwlen][i].count );
      }
    }

    return true;
  }



  void SongCacheManager::keywordsToShards() {
    log_w("Spreading keywords into shards (memory), this may take a while");
    size_t offsetscount = 0;
    //size_t totalpaths = 0;
    size_t totalbytes = 0;

    for( int i=0; i<SidDictSize; i++ ) {
      vTaskDelay(1);
      const char shardname = SidDict[i]->word[0];
      auto search = shards.find( shardname );
      if( search != shards.end() ) { // update existing
        shard_t* shard = search->second;
        shard->addWord( SidDict[i] );
      } else { // insert
        shard_t* shard = (shard_t*)sid_calloc(1, sizeof(shard_t));
        shard->name = shardname;
        shard->addWord( SidDict[i] );
        shards[shardname] = shard;
      }
      totalbytes += wpio->size( SidDict[i] );
      //totalbytes += strlen(SidDict[i]->word);
      //totalbytes += SidDict[i]->pathcount*sizeof(size_t);
      offsetscount   += SidDict[i]->pathcount;
    }

    log_w("DictSize=%d, Shards=%d, Paths=%d", SidDictSize, shards.size(), offsetscount );
    // ideally a 7bits numbered shard should contain sizeof(dictionary)/16 bytes
    //size_t bytes_per_shard = totalbytes/16;
    //bytes_per_shard = bytes_per_shard+256 - (bytes_per_shard%256);
    wpio->setShardYard( 16, totalbytes );
    //log_w("[Total wordpaths: %d bytes] = %d bytes per shard", totalbytes, bytes_per_shard );
  }



  bool SongCacheManager::cleanupShards() {
    if( !fs->exists( SHARDS_FOLDER ) ) {
      log_w("Creating shards folder at %s", SHARDS_FOLDER );
      fs->mkdir( SHARDS_FOLDER );
    } else {
      log_w("Purging shards folder at %s", SHARDS_FOLDER );
      fs::File root = fs->open( SHARDS_FOLDER );
      if(!root){
        log_e("Failed to open shards directory");
        return false;
      }
      if(!root.isDirectory()){
        log_e("%s is not a directory", SHARDS_FOLDER );
        return false;
      }
      fs::File file = root.openNextFile();
      while( file ) {
        vTaskDelay(1);
        if( !file.isDirectory() ) {
          String deleteName = String(SHARDS_FOLDER) + "/" + String( fs_file_path(&file) );
          file.close();
          log_w("Deleting file %s", deleteName.c_str() );
          fs->remove( deleteName );
        } else {
          file.close();
        }
        file = root.openNextFile();
      }
      file.close();
      root.close();
    }
    return true;
  }




  void SongCacheManager::addWordPathToDict(  wordpath_t* wp )
  {
    if( SidDict == nullptr ) {
      SidDict = (wordpath_t**)sid_calloc(SidDictBlockSize, sizeof(wordpath_t*));
      SidDictMemSize = SidDictBlockSize;
      if( SidDict == NULL ) {
        log_e("Unable to alloc %d bytes for keyword", 2*sizeof(wordpath_t*));
        return;
      } else {
        log_w("[Free:%d] Alloc init ok", ESP.getFreePsram() );
      }
    } else {
      if( SidDictSize+1 > SidDictMemSize ) {
        SidDict = (wordpath_t**)sid_realloc(SidDict, (SidDictSize+SidDictBlockSize+1)*sizeof(wordpath_t*) );
        SidDictMemSize += SidDictBlockSize;
      }
    }
    SidDict[SidDictSize] = wp;
    SidDictSize++;
  }



  wordpath_t* SongCacheManager::getWordPathFrom( const char* word )
  {
    if( SidDict == nullptr) {
      if( SidDictSize > 0 ) {
        log_e("Dictionary was deleted!");
      }
      return NULL;
    }

    for( int i=0; i<SidDictSize; i++ ) {
      if( SidDict[i]->word == NULL ) continue;
      if( strcmp( SidDict[i]->word, word ) == 0 ) {
        return SidDict[i];
      }
    }
    return NULL;
  }



  size_t SongCacheManager::indexItemWords( const char* itemPath, folderItemType_t itemType, int64_t offset, size_t KeywordMinLen )
  {
    SID_Meta_t song;
    snprintf( song.filename, 255, "%s", itemPath );
    if( offset == -1 ) {
      offset = sidPlayer->MD5Parser->MD5Index->find( MD5_FILE_IDX, itemPath );
    }
    int indexed_words = 0;
    if( offset > -1 ) {
      // fine
    } else { // don't bother indexing unknown md5
      log_w("Ignoring %s", itemPath );
      vTaskDelay(1);
      return 0;
    }

    String sentence = gnu_basename( itemPath ); // don't index folder name
    size_t sentence_len = sentence.length();
    // replace unwanted chars by spaces, lowercase the rest
    for( int i=0;i<sentence_len; i++ ) {
      switch(sentence[i]) {
        case '/': case '-': case '_': case '.': case ';':
          sentence[i] = ' ';
        break;
        default: // WARNING: NOT UTF8 safe
          sentence[i] = tolower(sentence[i]);
        break;
      }
    }
    if( itemType == F_SID_FILE ) { // remove trailing ".sid" extension
      sentence[sentence_len-4] = '\0'; // snip
      sentence_len -= 4;
    }
    char* sentence_arr = (char*)sid_calloc( sentence_len+1, sizeof( char ) );
    if( sentence_arr == NULL ) {
      log_e("Can't alloc %d bytes for sentence", sentence_len+1 );
      return 0;
    }
    memcpy( sentence_arr, sentence.c_str(), sentence_len );
    char* token = strtok(sentence_arr, " ");
    size_t tokenlen = 0;
    String tokenStr = "";
    // loop through the string to extract all other tokens
    while( token != NULL ) {

      tokenStr = String(token);
      tokenStr.trim();
      tokenlen = tokenStr.length();

      if( wpio->isBannedKeyword(token) ) {
        log_d("Ignored keyword: %s", tokenStr.c_str() );
        goto _next_token;
      }

      if( tokenlen >= KeywordMinLen ) {
        wordpath_t* wp = getWordPathFrom( tokenStr.c_str() );
        if( wp != NULL ) { // found, update shard if applicable
          if( wpio->keywordsInfolder[wp->word]  && wpio->keywordsInfolder[wp->word] >= wpio->max_paths_in_folder ) {
            if( wpio->keywordsInfolder[wp->word] == wpio->max_paths_in_folder ) {
              log_d("Keyword %s has reached max foldercount (%d)", tokenStr.c_str(), wpio->max_paths_in_folder );
            }
            wpio->keywordsInfolder[wp->word]++;
            goto _next_token;
          } else if( wp->pathcount+1 > wpio->max_paths_per_keyword ) {
            log_w("Keyword %s has reached max pathcount (%d) and won't get more insertions", tokenStr.c_str(), wp->pathcount );
            wpio->addBannedKeyword( tokenStr.c_str(), wp->pathcount);
            goto _next_token;
          } else { // update shard
            if( wpio->addPath(offset, itemType, wp) ) {
              log_v("[%d / %d] Increasing %s to %d, offset=%d", ESP.getFreeHeap(), SidDictSize, tokenStr.c_str(), wp->pathcount+1, int(offset) );
              if( !wpio->keywordsInfolder[wp->word] ) wpio->keywordsInfolder[wp->word] = 0; // this check is not necessary
              wpio->keywordsInfolder[wp->word]++;
            } else {
              log_e("An error occured while adding a keyword, aborting");
              goto _final;
            }
          }
        } else { // not found, create shard
          if( wpio->set( offset, itemType, tokenStr.c_str() ) ) {
            log_v("[%d / %d] Adding %s to dictionary, offset=%ld", ESP.getFreeHeap(), SidDictSize, tokenStr.c_str(), int(offset));
            wp = wpio->getwp();
            addWordPathToDict( wp );
            if( !wpio->keywordsInfolder[wp->word] ) wpio->keywordsInfolder[wp->word] = 0; // this check is not necessary
            wpio->keywordsInfolder[wp->word]++;
          } else {
            log_e("An error occured while adding a keyword, aborting");
            goto _final;
          }
        }
        indexed_words++;
        if( ticker != nullptr ) {
          ticker( wp->word, wp->pathcount );
        } else {
          vTaskDelay(1);
        }
      }
      _next_token: token = strtok(NULL, " ");
    }

    _final:
    free( sentence_arr );
    vTaskDelay(1);
    log_d("[Free=%d,Total=%d][+%d] %s", ESP.getFreePsram(), SidDictSize, indexed_words, sentence.c_str() );
    return indexed_words;
  }



  #endif // #if defined ENABLE_HVSC_SEARCH




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
  #endif // 0

}; // end namespace SIDView
