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

#ifndef _SID_CACHE_MANAGER_H_
#define _SID_CACHE_MANAGER_H_


#include <SidPlayer.h> //https://github.com/hpwit/SID6581
extern SIDTunesPlayer *sidPlayer;


bool getInfoFromSIDFile(fs::FS &fs, const char * path, songstruct * songinfo)
{
  uint8_t stype[5];
  if( &fs==NULL || path==NULL ) {
    log_e("Invalid parameter");
    return false;
  }
  if( !fs.exists( path ) ) {
    log_e("File %s does not exist\n", path);
    return false;
  }
  File file = fs.open( path );
  if(!file) {
    log_e("Unable to open %s\n", path);
    return false;
  }

  memset(stype, 0, 5);
  file.read(stype, 4);
  if( stype[0]!=0x50 || stype[1]!=0x53 || stype[2]!=0x49 || stype[3]!=0x44 ) {
    log_d("Unsupported SID Type: '%s' in file %s", stype, path);
    file.close();
    return false;
  }
  file.seek( 15 );
  songinfo->subsongs = file.read();
  if( songinfo->subsongs == 0 ) songinfo->subsongs = 1; // prevent malformed SID files from zero-ing song lengths
  file.seek( 17 );
  songinfo->startsong = file.read();
  file.seek( 22 );
  file.read( songinfo->name, 32 );
  file.seek( 0x36 );
  file.read( songinfo->author, 32 );
  file.seek( 0x56 );
  file.read( songinfo->published, 32 );

  uint16_t preferred_SID_model[3];
  file.seek( 0x77 );
  preferred_SID_model[0] = ( file.read() & 0x30 ) >= 0x20 ? 8580 : 6581;
  file.seek( 0x77 );
  preferred_SID_model[1] = ( file.read() & 0xC0 ) >= 0x80 ? 8580 : 6581;
  file.seek( 0x76 );
  preferred_SID_model[2] = ( file.read() & 3 ) >= 3 ? 8580 : 6581;

  bool playable = true;

  if( preferred_SID_model[0]!= 6581 || preferred_SID_model[1]!= 6581 || preferred_SID_model[2]!= 6581 ) {
    log_d("Unsupported SID Model?:  0: %d  1: %d  2: %d  in file %s",
      preferred_SID_model[0],
      preferred_SID_model[1],
      preferred_SID_model[2],
      path
    );
    // playable = false;
  }
  // don't lose time calculating hash for unplayable file
  if( playable ) {
    // procrastinate md5 de-hashing
    /*
    if( strcmp( songinfo->md5, "00000000000000000000000000000000" ) == 0 ) {
      snprintf( songinfo->md5, 33, "%s", Sid_md5::calcMd5( file ) );
    }
    */
  }

  file.close();

  if( sidPlayer->MD5Parser != NULL ) {
    if( songinfo->durations != nullptr ) free( songinfo->durations );
    songinfo->durations = (uint32_t*)sid_calloc( songinfo->subsongs, sizeof(uint32_t) );
    if( !sidPlayer->MD5Parser->getDurationsFromSIDPath( songinfo ) ) {
      log_w("SID Path lookup failed, for song '%s' will try md5 hash lookup", songinfo->name );
      if( strcmp( songinfo->md5, "00000000000000000000000000000000" ) == 0 ) {
        snprintf( songinfo->md5, 33, "%s", Sid_md5::calcMd5( file ) );
      }
      sidPlayer->MD5Parser->getDurationsFromMD5Hash( songinfo );
    }
  }

  return playable;
}


// item types for the SID Player
enum FolderItemType
{
  F_FOLDER,            // SID file list for current folder
  F_SUBFOLDER,         // subfolder with SID file list
  F_SUBFOLDER_NOCACHE, // subfolder with no SID file list
  F_PARENT_FOLDER,     // parent folder
  F_SID_FILE           // single SID file
};


typedef bool (*activityTicker)(); // callback for i/o activity during folder enumeration
typedef void (*folderElementPrintCb)( const char* fullpath, FolderItemType type );


static void debugFolderElement( const char* fullpath, FolderItemType type )
{
  String basepath = gnu_basename( fullpath );
  switch( type )
  {
    case F_FOLDER:            log_d("Cache Found :%s", basepath.c_str() ); break;
    case F_SUBFOLDER:         log_d("Subfolder Cache Found :%s", basepath.c_str() ); break;
    case F_SUBFOLDER_NOCACHE: log_d("Subfolder Found :%s", basepath.c_str() ); break;
    case F_PARENT_FOLDER:     log_d("Parent folder"); break;
    case F_SID_FILE:          log_d("SID File"); break;
  }
}


static bool debugFolderTicker()
{
  // not really a progress function but can show
  // some "activity" during slow folder scans
   Serial.print(".");
  return true; // continue signal, send false to stop and break the scan loop
}




struct SongCacheManager
{
  public:

    char *basename;
    char *basepath;
    char *cachepath;
    char *md5path;

    ~SongCacheManager() {
      free( basename  );
      free( basepath  );
      free( cachepath );
      free( md5path   );
    }

    SongCacheManager() {
      #ifdef BOARD_HAS_PSRAM
        psramInit();
      #endif
      basename  = (char*)sid_calloc( 256, sizeof(char) );
      basepath  = (char*)sid_calloc( 256, sizeof(char) );
      cachepath = (char*)sid_calloc( 256, sizeof(char) );
      md5path   = (char*)sid_calloc( 256, sizeof(char) );
    }


    void init( const char* path )
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


    String makeCachePath( const char* path, const char* cachename )
    {
      if( path == NULL || path[0] == '\0' ) {
        log_e("Bad path");
        return "";
      }
      if( cachename == NULL || cachename[0] == '\0' ) {
        log_e("Bad cachename");
        return "";
      }
      String cleanPath;
      cleanPath.reserve(256);
      if( path[strlen(path)-1] == '/' ) {
        cleanPath = String(path) + String(cachename);
      } else {
        cleanPath = String(path) + "/" + String(cachename);
      }
      log_d("Generated path %s from %s and %s", cleanPath.c_str(), path, cachename );
      return cleanPath;
    }


    String playListCachePath( const char * path )
    {
      return makeCachePath( path, ".sidcache");
    }


    String dirCachePath( const char * path )
    {
      return makeCachePath( path, ".dircache");
    }


    bool folderHasPlaylist( fs::FS &fs, const char* fname )
    {
      return fs.exists( playListCachePath( fname ) );
    }


    bool folderHasCache( fs::FS &fs, const char* fname )
    {
      return fs.exists( dirCachePath( fname ) );
    }


    bool purge( fs::FS &fs,  const char * path )
    {
      if( path == NULL || path[0] == '\0' ) {
        log_e("Bad path name");
        return false;
      }
      log_w("Will purge path %s", path );

      String songCachePath = playListCachePath( path );
      String dirCacheFile  = dirCachePath( path );
      log_w("Speculated %s and %s from %s", songCachePath.c_str(), dirCacheFile.c_str(), path );
      if( fs.exists( songCachePath ) ) {
        log_e("Removing %s", songCachePath.c_str() );
        fs.remove( songCachePath );
      } else {
        log_e("No %s (playlist) file to purge", songCachePath.c_str() );
      }
      if( fs.exists( dirCacheFile ) ) {
        log_e("Removing %s", dirCacheFile.c_str() );
        fs.remove( dirCacheFile );
      } else {
        log_e("No %s (dircache) file to purge", dirCacheFile.c_str() );
      }
      return true;
    }


    bool add( fs::FS &fs, const char * path, songstruct *song )
    {
      init( path );
      if( fs.exists( cachepath ) ) { // cache file exists !!
        if( !exists( fs, song ) ) { // song is not in cache file
          log_d("[Cache Insertion]");
          return insert( fs, song, true );
        }
        log_d("[Cache Hit]");
        return true; // song is already in the cache file
      } else { // cache file does not exist
        log_d("[Cache Creation+Insertion]");
        return insert( fs, song, false );
      }
    }


    int addSong(fs::FS &fs,  const char * path )
    {
      static songstruct * song = nullptr;

      if( song == nullptr ) {
        song = (songstruct*)sid_calloc( 1, sizeof(songstruct)+1 );
        if(song==NULL) {
          log_e("Not enough memory to add %s file", path);
          return -1;
        }
      } else {
        memset( song, 0, sizeof( songstruct ) );
      }

      song->fs = &fs;

      snprintf(song->filename, 255, "%s", path );

      if( getInfoFromSIDFile( fs, path, song ) ) {
        if( add( fs, path, song ) ) {
          log_w("Added file %s to cache", path);
          return 1;
        } else {
          log_e("Failed to add file %s to cache", path);
          return 0;
        }
      } else {
        //log_e("Ignoring unhandled file %s", path);
        return 0;
      }
    }


    bool update( fs::FS &fs, const char* path, songstruct *song )
    {
      init( path );
      snprintf( song->filename, 255, "%s", md5path ); // don't use the full path
      fs::File cacheFile = fs.open( cachepath, "r+" );
      if( ! cacheFile ) return false;
      bool ret = false;
      size_t bufsize = sizeof(songstruct);
      char *songstructbuf =  new char[bufsize+1];
      size_t entry_index = 0;
      while( cacheFile.readBytes( songstructbuf, bufsize ) > 0 ) {
        const songstruct* tmpsong = (const songstruct*)songstructbuf;
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
      delete songstructbuf;
      cacheFile.close();
      return ret; // not found
    }


    bool get( fs::FS &fs, const char* md5path, songstruct *song )
    {
      char *fullpath = (char*)sid_calloc( 256, sizeof(char) );
      snprintf( fullpath, 255, "%s/%s", SID_FOLDER, md5path );
      init( fullpath );
      free( fullpath );
      bool ret = false;
      fs::File cacheFile = fs.open( cachepath, FILE_READ );
      if( ! cacheFile ) return false;
      size_t bufsize = sizeof(songstruct);
      char *songstructbuf =  new char[bufsize+1];
      while( cacheFile.readBytes( songstructbuf, bufsize ) > 0 ) {
        const songstruct* tmpsong = (const songstruct*)songstructbuf;
        if( strcmp( (const char*)tmpsong->filename, md5path ) == 0  ) {
          memcpy( song, tmpsong, bufsize );
          song->fs = &fs; // set to current filesystem
          song->durations = (uint32_t*)sid_calloc(song->subsongs, sizeof(uint32_t) );
          cacheFile.readBytes( (char*)song->durations, song->subsongs*sizeof(uint32_t) );
          ret = true;
          break;
        }
      }
      cacheFile.close();
      delete songstructbuf;
      return ret;
    }


    bool scanFolderCache( fs::FS &fs, const char* pathname, folderElementPrintCb printCb )
    {
      String dirCacheFile = dirCachePath( pathname );
      if( dirCacheFile == "" ) {
        log_e("Bad dirCache filename");
        return false;
      }
      if( ! fs.exists( dirCacheFile ) ) {
        log_e("Cache file %s does not exist, create it first !", dirCacheFile.c_str() );
      }
      fs::File DirCache = fs.open( dirCacheFile );
      if( !DirCache ) {
        log_e("Unable to access dirCacheFile %s", dirCacheFile.c_str() );
        return false;
      }
      while( DirCache.available() ) {
        uint8_t typeint = DirCache.read();
        switch( typeint )
        {
          case F_FOLDER:            // SID file list for current folder
          case F_SUBFOLDER:         // subfolder with SID file list
          case F_SUBFOLDER_NOCACHE: // subfolder with no SID file list
          case F_PARENT_FOLDER:     // parent folder
          case F_SID_FILE:           // single SID file
          break;
          default: // EOF ?
            goto _end;
        }
        FolderItemType type = (FolderItemType) typeint;
        if( DirCache.read() != ':' ) {
          // EOF!
          goto _end;
        }
        String itemName = DirCache.readStringUntil('\n');
        if( itemName == "" ) return false;

        /*
        if( typeint == F_SUBFOLDER_NOCACHE ) {
          if( folderHasPlaylist( fs, itemName.c_str() ) ) {
            typeint = F_SUBFOLDER;
          }
        }
        */

        printCb( itemName.c_str(), type );
      }
      _end:
      return true;
    }


    bool dirCacheUpdate( fs::File *dirCache, const char*_itemName, FolderItemType type )
    {
      if( !dirCache) {
        log_e("No cache to update!");
        return false;
      }
      log_w("Will update item %s in cache %s", dirCache->name(), _itemName );
      while( dirCache->available() ) {
        size_t linepos = dirCache->position();
        uint8_t typeint = dirCache->read();
        switch( typeint )
        {
          case F_FOLDER:            // SID file list for current folder
          case F_SUBFOLDER:         // subfolder with SID file list
          case F_SUBFOLDER_NOCACHE: // subfolder with no SID file list
          case F_PARENT_FOLDER:     // parent folder
          case F_SID_FILE:           // single SID file
          break;
          default: // EOF ?
            log_w("EOF?");
            goto _end;
        }
        if( dirCache->read() != ':' ) {
          // EOF!
          log_w("EOF!");
          goto _end;
        }
        String itemName = dirCache->readStringUntil('\n');

        if( itemName == "" ) return false;

        if( itemName == String(_itemName) ) {
          log_w("Matching entry, will update");
          dirCache->seek( linepos );
          dirCache->write( (uint8_t)type );
          return true;
        }
      }
      _end:
      return false;
    }


    bool dirCacheAdd( fs::FS &fs, fs::File *dirCache, const char*itemName, FolderItemType type )
    {
      if( ! dirCache ) {
        log_e("Invalid file");
        return false;
      }
      if( type == F_FOLDER ) {
        // notify parent this folder has a playlist
        const char *dName = dirname( (char *)dirCache->name() );
        const char *parent = dirname( (char *)dName );
        String parentCacheName = String( parent ) + "/.dircache";
        if( !fs.exists( parentCacheName ) ) {
          log_e("Can't update parent cache, file %s does not exist", parentCacheName.c_str() );
        } else {
          bool ret = false;
          fs::File parentDirCache = fs.open( parentCacheName, "r+" );
          if( dirCacheUpdate( &parentDirCache, itemName, F_SUBFOLDER ) ) {
            log_w("Folder type updated in parent cache file %s for playlist item %s", parentCacheName.c_str(), itemName );
            ret = true;
          } else {
            log_e("Unable to update parent cache %s", parentCacheName.c_str() );
          }
          parentDirCache.close();
          return ret;
        }
      } else {
        dirCache->write( (uint8_t)type );
        dirCache->write( ':' );
        dirCache->write( (uint8_t*)itemName, strlen( itemName ) ); // no null terminating
        dirCache->write( '\n' );
      }
      return true;
    }


    bool scanFolder( fs::FS &fs, const char* pathname, folderElementPrintCb printCb = &debugFolderElement, activityTicker tickerCb = &debugFolderTicker, size_t maxitems=0, bool force_regen = false)
    {
      File DirCache;
      File root = fs.open( pathname );
      if(!root){
        log_e("Failed to open %s directory\n", pathname);
        return false;
      }
      if(!root.isDirectory()){
        log_e("%s is not a directory\b", pathname);
        return false;
      }

      bool folder_has_playlist = folderHasPlaylist( fs, pathname );
      bool folder_has_cache    = folderHasCache( fs, pathname );
      bool build_cache = false;
      bool cache_built = false;

      if( force_regen ) {
        purge( fs, pathname );
        folder_has_playlist = false;
        folder_has_cache = false;
      }

      size_t totalitems = 0;

      if( folder_has_playlist ) {
        printCb( pathname, F_FOLDER );
        totalitems++;
      } else {
        build_cache = true;
      }

      if( folder_has_cache ) {
        log_d("Serving cached folder");
        root.close();
        return scanFolderCache( fs, pathname, printCb );
      } else {
        log_d("Generating cached folder");
        DirCache = fs.open( dirCachePath( pathname ), FILE_WRITE );
      }

      File file = root.openNextFile();
      while( file ) {
        if( file.isDirectory()  ) {
          if( folderHasPlaylist( fs, file.name() ) ) {
            printCb( file.name(), F_SUBFOLDER );
            dirCacheAdd( fs, &DirCache, file.name(), F_SUBFOLDER );
          } else {
            printCb( file.name(), F_SUBFOLDER_NOCACHE );
            dirCacheAdd( fs, &DirCache, file.name(), F_SUBFOLDER_NOCACHE );
          }
          totalitems++;
        } else {
          // handle both uppercase and lowercase of the extension
          String fName = String( file.name() );
          fName.toLowerCase();

          if( fName.endsWith( ".sid" ) && !file.isDirectory() ) {
            printCb( file.name(), F_SID_FILE );
            dirCacheAdd( fs, &DirCache, file.name(), F_SID_FILE );

            if( build_cache ) {
              if( addSong( fs, file.name() ) > 0 ) {
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
        if( !tickerCb() ) break;
        if( maxitems>0 && totalitems>=maxitems ) break;
      }

      if( build_cache && cache_built && !folder_has_playlist ) {
        printCb( pathname, F_FOLDER );
        dirCacheAdd( fs, &DirCache, pathname, F_FOLDER );
      }

      DirCache.close();
      root.close();
      return true;
    }


    bool loadCache( SIDTunesPlayer *tmpPlayer, const char* _cachepath )
    {
      if( !String( _cachepath ).endsWith("/.sidcache") ) {
        log_w("Warning: _cachepath %d does not end with '/.sidcache', will append", _cachepath );
        if( _cachepath[strlen(_cachepath)-1] == '/' ) {
          snprintf( cachepath, 255, "%s.sidcache", _cachepath );
        } else {
          snprintf( cachepath, 255, "%s/.sidcache", _cachepath );
        }
      } else {
        snprintf( cachepath, 255, "%s", _cachepath );
      }

      if(! SID_FS.exists( cachepath ) ) {
        log_e("Error! Cache file %s does not exist", cachepath );
        return false;
      }
      File cacheFile = SID_FS.open( cachepath, FILE_READ );
      size_t bufsize = sizeof(songstruct);
      size_t filesize = cacheFile.size();
      if( filesize == 0 ) {
        Serial.printf("[ERROR] Invalid cache. File %s is empty and should be deleted!", cachepath );
        //SID_FS.remove( cachepath );
        return false;
      }
      Serial.printf("Cache file %s (%d bytes) estimated to %d entries (average %d bytes each)\n", cachepath, filesize, filesize/bufsize, bufsize );
      char *songstructbuf =  new char[bufsize+1];
      while( cacheFile.readBytes( songstructbuf, bufsize ) == bufsize ) {
        if( tmpPlayer->numberOfSongs > tmpPlayer->maxSongs ) {
          log_w("Max songs (%d) in player reached, the rest of this cache will be ignored", tmpPlayer->maxSongs );
          break;
        }
        songstruct * song;
        song=(songstruct *)sid_calloc(1,sizeof(songstruct));
        if( song==NULL ) {
            log_e("Not enough memory to add song %d", tmpPlayer->numberOfSongs+1);
            break;
        } else {
          memcpy(song,(const unsigned char*)songstructbuf,sizeof(songstruct));
          song->fs= &SID_FS;
          if( !String(song->filename).startsWith( SID_FOLDER ) ) {
            // insert SID_FOLDER before path
            memmove( song->filename+strlen(SID_FOLDER), song->filename, strlen(song->filename)+strlen(SID_FOLDER) );
            memcpy( song->filename, SID_FOLDER, strlen( SID_FOLDER ) );
          }
          // alloc variable memory for subsong durations
          song->durations = (uint32_t*)sid_calloc(1, song->subsongs*sizeof(uint32_t) );
          cacheFile.readBytes( (char*)song->durations, song->subsongs*sizeof(uint32_t) );


          tmpPlayer->listsongs[tmpPlayer->numberOfSongs] = song;
          songdebug( tmpPlayer->listsongs[tmpPlayer->numberOfSongs] );
          tmpPlayer->numberOfSongs++;
        }
      }
      delete songstructbuf;
      cacheFile.close();
      //tmpPlayer->setSongstructCachePath( SID_FS, cachepath );
      return true;
    }


  private:

    bool exists( fs::FS &fs, songstruct *song )
    {
      fs::File cacheFile = fs.open( cachepath, FILE_READ );
      if( ! cacheFile ) return false;
      bool ret = false;
      size_t bufsize = sizeof(songstruct);
      char *songstructbuf =  new char[bufsize+1];
      while( cacheFile.readBytes( songstructbuf, bufsize ) > 0 ) {
        const songstruct* tmpsong = (const songstruct*)songstructbuf;
        // jump forward subsongs lengths
        cacheFile.seek( cacheFile.position() + tmpsong->subsongs*sizeof(uint32_t) );
        if( strcmp( (const char*)tmpsong->name, (const char*)song->name ) == 0
        && strcmp( (const char*)tmpsong->author, (const char*)song->author ) == 0 ) {
          ret = true;
          break;
        }
      }
      cacheFile.close();
      delete songstructbuf;
      return ret;
    }


    bool insert( fs::FS &fs, songstruct *song, bool append = false )
    {
      fs::File cacheFile;
      if( append ) {
        cacheFile = fs.open( cachepath, "a+" );
        // TODO: check that cacheFile.position() is a multiple of songstruct
      } else {
        cacheFile = fs.open( cachepath, "w" ); // truncate
      }
      if( ! cacheFile ) return false;
      snprintf( song->filename, 255, "%s", md5path ); // don't use the full path
      size_t objsize = sizeof( songstruct ) + song->subsongs*sizeof(uint32_t);
      size_t written_bytes = cacheFile.write( (uint8_t*)song, objsize );
      cacheFile.close();
      return written_bytes == objsize;
    }

};



  #if 0


  int createCacheFromFolder( fs::FS &fs, const char* foldername, const char* filetype, bool recursive )
  {
    File root = fs.open( foldername );
    if(!root){
      log_e("Failed to open %s directory\n", foldername);
      return 0;
    }
    if(!root.isDirectory()){
      log_e("%s is not a directory\b", foldername);
      return 0;
    }
    File file = root.openNextFile();
    while(file){
      // handle both uppercase and lowercase of the extension
      String fName = String( file.name() );
      fName.toLowerCase();

      if( fName.endsWith( filetype ) && !file.isDirectory() ) {
        if( SongCache->addSong(fs, file.name() ) == -1 ) return -1;
        // log_v("[INFO] Added [%s] ( %d bytes )\n", file.name(), file.size() );
      } else if( file.isDirectory()  ) {
        if( recursive ) {
            int ret = createCacheFromFolder( fs, file.name(), filetype, true );
            if( ret == -1 ) return -1;
        } else {
            log_i("[INFO] Ignored [%s]\n", file.name() );
        }
      } else {
        log_i("[INFO] Ignored [%s]\n", file.name() );

      }
      file = root.openNextFile();
    }
    return 1;
  }



  // TODO: deprecate this
  int createCacheFromMd5( fs::FS &fs, const char* foldername, const char* md5path )
  {
    File md5File = fs.open( md5path );
    if( !md5File ) return -1;
    char    filename[255]   = {0};
    char    md5hash[33]     = {0};
    char    fullpath[255]   = {0};
    int32_t linesCount      = -1;
    size_t  md5FileSize     = md5File.size();
    size_t  totalDecoded    = 0; // entries count
    size_t  totalProcessed  = 0; // matches count
    size_t  totalIgnored    = 0; // missing files count
    size_t  totalTruncated  = 0; // too many subsongs
    uint32_t durations[256] = {0}; // looks like even 64 isn't enough, let's cap this
    size_t durationsSize    = sizeof( durations ) / sizeof( uint32_t );
    uint8_t progress        = 0;
    uint8_t lastprogress    = 0;
    unsigned long start     = millis();
    // speed up testing
    //md5File.seek( md5FileSize-4096 );
    //md5File.readStringUntil('\n');

    while ( md5File.available() ) {
      linesCount++;
      String line    = md5File.readStringUntil('\n');
      size_t lineLen = strlen( line.c_str() );
      size_t cursorPosition = md5File.position();
      if( lineLen < 10 ) break; // end of file or garbage, no need to continue
      progress = (100*cursorPosition)/md5FileSize;
      if( progress != lastprogress ) {
        lastprogress = progress;
        Serial.printf("MD5 Database Parsing Progress: %3d%s (%5d/%-5d matches, %d lines, %d bytes read), \n", progress, "%", totalProcessed, totalDecoded, linesCount, cursorPosition );
      }
      if( line[0]=='[') continue; // ignore
      if( line[0]==';') { // is filename
        memmove( filename, line.c_str()+2, lineLen ); // extract filename (and trim the first two characters "; ")
        sprintf( fullpath, "%s%s", SID_FOLDER, filename ); // compute real path on filesystem
        linesCount++;
        String md5line = md5File.readStringUntil('\n') + " "; // read next line and append space as terminator
        const char* md5linecstr = md5line.c_str(); // alias this to a const char* to avoid later stack smashing
        lineLen = strlen( md5linecstr ); // how long is that string ?
        if( lineLen <= 32 ) {
          Serial.printf("[ERROR] Expected md5 hash not found at line %d\n", linesCount );
          continue;
        }
        // string looks healthy enough for data extraction
        memmove( md5hash, md5linecstr, 32 ); // capture md5 hash for later
        totalDecoded++;
        // now see if it's worth decoding the durations
        if( !fs.exists( fullpath ) ) {
          log_d("[NOTICE] File not found: %s", filename );
          totalIgnored++;
          continue;
        }

        //int durationsFound = getDurationsFromMd5String( md5line, durations );
  /*
        memset( durations, 0, durationsSize*sizeof(uint32_t) ); // blank the array
        int res = md5line.indexOf('='); // where do the timings start in the string ?
        if (res == -1 || res >= lineLen ) { // the md5 file is malformed or an i/o error occured
          log_e("[ERROR] bad md5 line: %s at line %d / offset %d", md5linecstr, linesCount-1, cursorPosition );
          continue;
        };

        char parsedTime[11]     = {0}; // for storing mm.ss.SSS
        uint8_t parsedTimeCount = 0; // how many durations/subsongs
        uint8_t parsedIndex     = 0; // substring parser index
        for( int i=res; i<lineLen; i++ ) {
          //char parsedChar = md5linecstr[i];
          switch( md5linecstr[i] ) {
            case '=': // durations begin!
              parsedIndex = 0;
            break;
            case ' ': // next or last duration
              if( parsedIndex > 0 ) {
                parsedTime[parsedIndex+1] = '\0'; // null terminate
                int mm,ss,SSS; // for storing the extracted time values
                sscanf( parsedTime, "%d:%d.%d", &mm, &ss, &SSS );
                durations[parsedTimeCount] = (mm*60*1000)+(ss*1000)+SSS;
                //Serial.printf("[Track #%d:%d] (%16s) %s / %d ms\n", totalProcessed, parsedTimeCount, (const char*)filename, parsedTime, durations[parsedTimeCount] );
                parsedTimeCount++;
              }
              parsedIndex = 0;
            break;
            default:
              if( parsedIndex > 10 ) { // "mm.ss.SSS" pattern can't be more than 10 chars, something went wrong
                Serial.printf("[ERROR] Substring index overflow for track %s at subsong #%d items\n", (const char*)filename, parsedTimeCount );
                parsedIndex = 0;
                goto _addSongCache; // break out of switch and parent loop
              }
              parsedTime[parsedIndex] = md5linecstr[i];
              parsedIndex++;
          }
          if( parsedTimeCount >= durationsSize ) { // prevent stack smashing
            Serial.printf("[WARNING] track %s has too many subsongs (max=%d), truncating at index %d\n", (const char*)filename, durationsSize, parsedTimeCount-1);
            totalTruncated++;
            break;
          }
        }
        if( parsedTimeCount > 0 ) {
          log_d("[Track #%d] (%16s) %d song(s)", totalProcessed, (const char*)filename, parsedTimeCount );
          totalProcessed++;
        }
        _addSongCache:
        if(1) { ; } // stupid compiler wants nonempty tokens after a goto label
        */
        // see if the cache needs updating
        //addSongToCache( fs, fullpath, durations, md5hash );

      } // end if( line is filename )
    }//end while( md5File.available() )

    unsigned long totalduration = millis() - start;

    Serial.printf("MD5 File scanning finished !\nScan duration: %d seconds\nProcessed lines: %d (%d bytes)\nMatched entries: %d/%d\nTruncated subsongs: %d\nMissing files:%d\n",
      uint32_t(totalduration/1000),
      linesCount,
      md5FileSize,
      totalProcessed,
      totalDecoded,
      totalTruncated,
      totalIgnored
    );

    return linesCount;
  }


  #endif


#endif
