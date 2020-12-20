


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
    log_d("Unsupported SID Model:  0: %d  1: %d  2: %d  in file %s",
      preferred_SID_model[0],
      preferred_SID_model[1],
      preferred_SID_model[2],
      path
    );
    playable = false;
  }
  // don't lose time calculating hash for unplayable file
  if( playable ) {
    // don't lose time calculating hash twice
    if( strcmp( songinfo->md5, "00000000000000000000000000000000" ) == 0 ) {
      sprintf( songinfo->md5,"%s", Sid_md5::calcMd5( file ) );
    }
  }

  file.close();

  if( sidPlayer->MD5Parser != NULL ) {

    if( songinfo->durations != nullptr ) free( songinfo->durations );
    songinfo->durations = (uint32_t*)sid_calloc( songinfo->subsongs, sizeof(uint32_t) );

    sidPlayer->MD5Parser->getDurationsFromSIDPath( songinfo );
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

    char basename[255];
    char basepath[255];
    char cachepath[255];
    char md5path[255];

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
      sprintf( cachepath, "%s%s", basepath, ".sidcache" );
      if( String(path).startsWith( SID_FOLDER ) ) {
        memmove( md5path, path+strlen( SID_FOLDER ), strlen(path) ); // remove leading SID_FOLDER from path
      } else {
        strcpy( md5path, path );
      }
      log_v("\nComputed from %s:\n- basename: %s\n- basepath: %s\n- cachepath: %s\n- md5path: %s\n", path, basename, basepath, cachepath, md5path );
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


    int addSong(fs::FS &fs,  const char * path/*, uint32_t *durations = nullptr, const char* md5hash = nullptr*/ )
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

      sprintf(song->filename,"%s", path );

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
      sprintf( song->filename, "%s", md5path ); // don't use the full path
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
      char fullpath[255] = {0};
      sprintf( fullpath, "%s/%s", SID_FOLDER, md5path );
      init( fullpath );
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
          song->durations = (uint32_t*)sid_calloc(1, song->subsongs*sizeof(uint32_t) );
          cacheFile.readBytes( (char*)song->durations, song->subsongs*sizeof(uint32_t) );
          ret = true;
          break;
        }
      }
      cacheFile.close();
      delete songstructbuf;
      return ret;
    }


    bool folderHasCache( fs::FS &fs, const char* fname )
    {
      if( fname[strlen(fname)-1] == '/' ) {
        return fs.exists( String(fname) + String(".sidcache") );
      }
      return fs.exists( String(fname) + String("/.sidcache") );
    }


    bool scanFolder( fs::FS &fs, const char* pathname, folderElementPrintCb printCb = &debugFolderElement, activityTicker tickerCb = &debugFolderTicker, size_t maxitems=0)
    {
      File root = fs.open( pathname );
      if(!root){
        log_e("Failed to open %s directory\n", pathname);
        return false;
      }
      if(!root.isDirectory()){
        log_e("%s is not a directory\b", pathname);
        return false;
      }

      bool folder_has_cache = folderHasCache( fs, pathname );
      bool build_cache = false;
      bool cache_built = false;

      size_t totalitems = 0;

      if( folder_has_cache ) {
        printCb( pathname, F_FOLDER );
        totalitems++;
      } else {
        build_cache = true;
      }

      File file = root.openNextFile();
      while( file ) {
        if( file.isDirectory()  ) {
          if( folderHasCache( fs, file.name() ) ) {
            printCb( file.name(), F_SUBFOLDER );
          } else {
            printCb( file.name(), F_SUBFOLDER_NOCACHE );
          }
          totalitems++;
        } else {
          // handle both uppercase and lowercase of the extension
          String fName = String( file.name() );
          fName.toLowerCase();

          if( fName.endsWith( ".sid" ) && !file.isDirectory() ) {
            printCb( file.name(), F_SID_FILE );

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

      if( build_cache && cache_built ) {
        printCb( pathname, F_FOLDER );
      }

      root.close();
      return true;
    }


    bool loadCache( SIDTunesPlayer *tmpPlayer, const char* _cachepath )
    {
      if( !String( _cachepath ).endsWith("/.sidcache") ) {
        log_w("Warning: _cachepath %d does not end with '/.sidcache', will append", _cachepath );
        if( _cachepath[strlen(_cachepath)-1] == '/' ) {
          sprintf( cachepath, "%s.sidcache", _cachepath );
        } else {
          sprintf( cachepath, "%s/.sidcache", _cachepath );
        }
      } else {
        sprintf( cachepath, "%s", _cachepath );
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
        if( tmpPlayer->numberOfSongs > sidPlayer->maxSongs ) {
          log_w("Max songs (%d) in player reached, the rest of this cache will be ignored", sidPlayer->maxSongs );
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

      tmpPlayer->setSongstructCachePath( SID_FS, cachepath );

      return true;
      //Serial.println("[REMOVE ME] Now deleting cache");
      //SID_FS.remove("/tmp/sidsongs.cache" );
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
      sprintf( song->filename, "%s", md5path ); // don't use the full path
      size_t objsize = sizeof( songstruct ) + song->subsongs*sizeof(uint32_t);
      size_t written_bytes = cacheFile.write( (uint8_t*)song, objsize );
      cacheFile.close();
      return written_bytes == objsize;
    }


};

SongCacheManager *SongCache = new SongCacheManager();




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



